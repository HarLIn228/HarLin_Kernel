#include "io.h"
#include "screen.h"
#include "network.h"
#include "interrupt.h"
#include "pmm.h"
#include "pci.h"
#include "vmm.h"
#include "kmalloc.h"
#include "harlin_API.h"
#include "spinlock.h"

#define RX_BUF_SIZE 0x2000
#define TX_BUF_SIZE  2048
#define TX_BUF_COUNT 4

#define ETHERTYPE_IP  0x0800
#define ETHERTYPE_ARP 0x0806

#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10

#define TCP_STATE_CLOSED     0
#define TCP_STATE_SYN_SENT   1
#define TCP_STATE_ESTABLISHED 2
#define TCP_STATE_FIN_SENT   3

static unsigned short io_base = 0;
static unsigned char local_mac[6];
static unsigned char gateway_mac[6];
static int gateway_resolved = 0;

static unsigned char local_ip[4]  = {10, 0, 2, 15};
static unsigned char remote_ip[4] = {0};

static volatile int net_irq_done = 0;
static volatile int dhcp_in_progress = 0;
static struct spinlock net_irq_lock = { 0 };
static int net_irq_lock_inited = 0;

static void net_irq_handler(void);
static int rtl_poll_packet(unsigned char* buf, int max_len);

static struct tcp_conn g_connections[MAX_TCP_CONN];
static unsigned short tcp_next_sport = 40000;
static struct spinlock tcp_table_lock = { 0 };
static int tcp_table_lock_inited = 0;

static struct tcp_conn* tcp_get_conn(int id)
{
    if (id < 0 || id >= MAX_TCP_CONN) return (struct tcp_conn*)0;
    if (g_connections[id].state == TCP_STATE_CLOSED) return (struct tcp_conn*)0;
    return &g_connections[id];
}

static int tcp_alloc_conn(void)
{
    int i;
    int found = -1;
    if (!tcp_table_lock_inited) {
        spinlock_init(&tcp_table_lock);
        tcp_table_lock_inited = 1;
    }
    spinlock_acquire(&tcp_table_lock);
    for (i = 0; i < MAX_TCP_CONN; i++) {
        if (g_connections[i].state == TCP_STATE_CLOSED) {
            Harlin_Fill(&g_connections[i], 0, sizeof(struct tcp_conn));
            found = i;
            break;
        }
    }
    spinlock_release(&tcp_table_lock);
    return found;
}

static void tcp_free_conn(int id)
{
    if (id < 0 || id >= MAX_TCP_CONN) return;
    if (!tcp_table_lock_inited) return;
    spinlock_acquire(&tcp_table_lock);
    g_connections[id].state = TCP_STATE_CLOSED;
    g_connections[id].mac_resolved = 0;
    spinlock_release(&tcp_table_lock);
}

static volatile unsigned char* rx_buf = 0;

static void net_putstr(const char* s)
{
    while (*s) screen_put_char(*s++);
}

static void net_putnum(unsigned int n)
{
    char buf[12];
    int i = 0;
    if (n == 0) { screen_put_char('0'); return; }
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i > 0) screen_put_char(buf[--i]);
}

static int ip_parse(const char* s, unsigned char* out)
{
    int i, num, dots;
    for (i = 0; i < 4; i++) {
        num = 0;
        while (*s >= '0' && *s <= '9') {
            num = num * 10 + (*s - '0');
            s++;
        }
        if (num > 255) return 0;
        out[i] = (unsigned char)num;
        if (i < 3) {
            if (*s != '.') return 0;
            s++;
        }
    }
    return (*s == 0 || *s == ' ' || *s == ':');
}

static unsigned short net_htons(unsigned short v)
{
    return ((v & 0xFF) << 8) | ((v >> 8) & 0xFF);
}

static unsigned short net_checksum(unsigned char* data, int len)
{
    unsigned int sum = 0;
    int i;
    if (len <= 0) return 0;
    for (i = 0; i < len - 1; i += 2) {
        sum += ((unsigned short)data[i] << 8) | data[i + 1];
    }
    if (len & 1) {
        sum += ((unsigned short)data[len - 1] << 8);
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (unsigned short)(~sum);
}

static int rtl_detect(void)
{
    struct pci_device dev;
    int idx;
    static const u16 known_ids[] = {
        0x8139, 0x8138, 0x1300
    };
    int i;
    for (i = 0; i < (int)(sizeof(known_ids) / sizeof(known_ids[0])); i++) {
        idx = pci_find_device(0x10EC, known_ids[i], &dev);
        if (idx >= 0) {
            u64 bar;
            pci_enable_busmaster(&dev);
            if (pci_get_bar(&dev, 0, &bar) == 0) {
                io_base = (unsigned short)(bar & 0xFFFC);
                net_putstr("RTL NIC found at IO 0x");
                {
                    char hex[] = "0123456789ABCDEF";
                    screen_put_char(hex[(io_base >> 12) & 0xF]);
                    screen_put_char(hex[(io_base >> 8) & 0xF]);
                    screen_put_char(hex[(io_base >> 4) & 0xF]);
                    screen_put_char(hex[io_base & 0xF]);
                }
                screen_put_char('\n');
                return 1;
            }
        }
    }
    return 0;
}

static void rtl_init(void)
{
    int i;
    unsigned int val;
    unsigned long rx_phys;

    local_mac[0] = inb(io_base + 0);
    local_mac[1] = inb(io_base + 1);
    local_mac[2] = inb(io_base + 2);
    local_mac[3] = inb(io_base + 3);
    local_mac[4] = inb(io_base + 4);
    local_mac[5] = inb(io_base + 5);

    net_putstr("MAC: ");
    {
        char hex[] = "0123456789ABCDEF";
        for (i = 0; i < 6; i++) {
            screen_put_char(hex[local_mac[i] >> 4]);
            screen_put_char(hex[local_mac[i] & 0xF]);
            if (i < 5) screen_put_char(':');
        }
    }
    screen_put_char('\n');

    outb(io_base + 0x52, 0x00);

    outb(io_base + 0x37, 0x10);
    for (i = 0; i < 200000; i++) {
        val = inb(io_base + 0x37);
    }

    rx_buf = (volatile unsigned char*)kmalloc(RX_BUF_SIZE + 16);
    if (!rx_buf) {
        net_putstr("RTL8139 rx buffer alloc failed\n");
        return;
    }
    rx_phys = vmm_get_phys((u64)rx_buf);
    if (rx_phys >> 32) {
        net_putstr("RTL8139 rx buffer above 4GB\n");
        kfree((void*)rx_buf);
        rx_buf = 0;
        return;
    }
    outl(io_base + 0x30, (unsigned int)rx_phys);

    outl(io_base + 0x40, 0x03000700);
    outl(io_base + 0x44, 0x0000070F);

    outb(io_base + 0x37, 0x0C);

    outw(io_base + 0x3C, 0x0005);

    irq_register(11, net_irq_handler);

    net_putstr("RTL8139 initialized\n");
}

static void net_irq_handler(void)
{
    u64 flags;
    unsigned short isr;
    if (!net_irq_lock_inited) {
        spinlock_init(&net_irq_lock);
        net_irq_lock_inited = 1;
    }
    flags = spinlock_acquire_irqsave(&net_irq_lock);
    isr = inw(io_base + 0x3E);
    outw(io_base + 0x3E, isr);
    net_irq_done = 1;
    spinlock_release_irqrestore(&net_irq_lock, flags);
}

static void net_wait_irq(void)
{
    unsigned int timeout = 10000000;
    unsigned long long flags;
    asm volatile ("pushfq; popq %0" : "=r"(flags) : : "memory");
    if (!(flags & (1ULL << 9)))
        return;
    net_irq_done = 0;
    while (!net_irq_done && timeout--) {
        asm volatile ("sti; hlt; cli" : : : "memory");
    }
    if (!net_irq_done) {
        inw(io_base + 0x3E);
    }
}

static int rtl_send_packet(unsigned char* pkt, int len)
{
    unsigned char* tx_buf;
    unsigned long phys;
    int i;
    int tx_len;

    if (!pkt || len <= 0)
        return -1;
    if (len > TX_BUF_SIZE)
        return -1;

    tx_len = len < 60 ? 60 : len;

    tx_buf = (unsigned char*)kmalloc(TX_BUF_SIZE);
    if (!tx_buf)
        return -1;

    for (i = 0; i < len; i++) {
        tx_buf[i] = pkt[i];
    }
    for (i = len; i < tx_len; i++) {
        tx_buf[i] = 0;
    }

    phys = vmm_get_phys((u64)tx_buf);
    if (phys >> 32) {
        kfree(tx_buf);
        return -1;
    }
    outl(io_base + 0x20, (unsigned int)phys);
    outl(io_base + 0x10, (unsigned int)(tx_len & 0xFFF));

    net_wait_irq();

    kfree(tx_buf);
    return tx_len;
}

static int rtl_poll_packet(unsigned char* buf, int max_len)
{
    unsigned short capr, cbr;
    unsigned short pkt_status, pkt_len;
    int offset, i;

    capr = inw(io_base + 0x38);
    cbr  = inw(io_base + 0x3A);

    if (capr == cbr) return 0;

    offset = capr % RX_BUF_SIZE;

    pkt_status = (unsigned short)rx_buf[offset] | ((unsigned short)rx_buf[offset + 1] << 8);
    pkt_len    = (unsigned short)rx_buf[offset + 2] | ((unsigned short)rx_buf[offset + 3] << 8);

    if (!(pkt_status & 0x0001)) {
        outw(io_base + 0x38, cbr);
        return 0;
    }

    if (pkt_len > (unsigned short)(max_len + 4)) pkt_len = (unsigned short)(max_len + 4);
    if (pkt_len < 60) pkt_len = 60;

    for (i = 4; i < (int)pkt_len && (i - 4) < max_len; i++) {
        buf[i - 4] = rx_buf[(offset + i) % RX_BUF_SIZE];
    }

    capr = (unsigned short)((capr + pkt_len + 3) & 0xFFFC);
    outw(io_base + 0x38, capr - 0x10);

    return pkt_len - 4;
}

static int net_wait_packet(unsigned char* buf, int max_len)
{
    int len;
    int waits;
    for (waits = 100000; waits > 0; waits--) {
        len = rtl_poll_packet(buf, max_len);
        if (len > 0) return len;
    }
    return 0;
}

static int arp_resolve(unsigned char* target_ip)
{
    unsigned char pkt[64];
    unsigned char rx[2048];
    int i, len, timeout;
    unsigned short op;

    for (i = 0; i < 64; i++) pkt[i] = 0;

    for (i = 0; i < 6; i++) pkt[i] = 0xFF;
    for (i = 0; i < 6; i++) pkt[i + 6] = local_mac[i];

    pkt[12] = 0x08; pkt[13] = 0x06;

    pkt[14] = 0x00; pkt[15] = 0x01;
    pkt[16] = 0x08; pkt[17] = 0x00;
    pkt[18] = 0x06;
    pkt[19] = 0x04;
    pkt[20] = 0x00; pkt[21] = 0x01;
    for (i = 0; i < 6; i++) pkt[22 + i] = local_mac[i];
    for (i = 0; i < 4; i++) pkt[28 + i] = local_ip[i];
    for (i = 0; i < 6; i++) pkt[32 + i] = 0x00;
    for (i = 0; i < 4; i++) pkt[38 + i] = target_ip[i];

    net_putstr("ARP query ");
    net_putnum(target_ip[0]); screen_put_char('.');
    net_putnum(target_ip[1]); screen_put_char('.');
    net_putnum(target_ip[2]); screen_put_char('.');
    net_putnum(target_ip[3]); screen_put_char('\n');

    rtl_send_packet(pkt, 42);

    timeout = 5000;
    while (timeout > 0) {
        len = net_wait_packet(rx, sizeof(rx));
        if (len > 0) {
            if (len >= 42) {
                op = ((unsigned short)rx[21] << 8) | rx[20];
                if (rx[12] == 0x08 && rx[13] == 0x06 && op == 0x0002) {
                    int match = 1;
                    for (i = 0; i < 4; i++) {
                        if (rx[28 + i] != target_ip[i]) { match = 0; break; }
                    }
                    if (match) {
                        for (i = 0; i < 6; i++) gateway_mac[i] = rx[22 + i];
                        gateway_resolved = 1;
                        net_putstr("ARP resolved\n");
                        return 1;
                    }
                }
            }
        }
        timeout--;
    }

    net_putstr("ARP failed\n");
    return 0;
}

static int arp_respond(unsigned char* rx, int len)
{
    unsigned char pkt[64];
    int i;
    unsigned short op;

    if (dhcp_in_progress) return 0;
    if (len < 42) return 0;
    if (rx[12] != 0x08 || rx[13] != 0x06) return 0;

    op = ((unsigned short)rx[21] << 8) | rx[20];
    if (op != 0x0001) return 0;

    {
        int match = 1;
        for (i = 0; i < 4; i++) {
            if (rx[38 + i] != local_ip[i]) { match = 0; break; }
        }
        if (!match) return 0;
    }

    for (i = 0; i < 6; i++) pkt[i] = rx[6 + i];
    for (i = 0; i < 6; i++) pkt[i + 6] = local_mac[i];

    pkt[12] = 0x08; pkt[13] = 0x06;

    pkt[14] = 0x00; pkt[15] = 0x01;
    pkt[16] = 0x08; pkt[17] = 0x00;
    pkt[18] = 0x06;
    pkt[19] = 0x04;
    pkt[20] = 0x00; pkt[21] = 0x02;
    for (i = 0; i < 6; i++) pkt[22 + i] = local_mac[i];
    for (i = 0; i < 4; i++) pkt[28 + i] = local_ip[i];
    for (i = 0; i < 6; i++) pkt[32 + i] = rx[6 + i];
    for (i = 0; i < 4; i++) pkt[38 + i] = rx[28 + i];

    rtl_send_packet(pkt, 42);
    return 1;
}

static int ip_send(unsigned char* dest_mac, unsigned char* dest_ip, unsigned char proto, unsigned char* payload, int pay_len);

static int udp_send(unsigned char* dest_mac, unsigned char* dest_ip,
                     unsigned short src_port, unsigned short dst_port,
                     unsigned char* data, int data_len)
{
    unsigned char udp_pkt[1500];
    int i, udp_len;
    unsigned short cksum;

    udp_len = 8 + data_len;

    udp_pkt[0] = (unsigned char)(src_port >> 8);
    udp_pkt[1] = (unsigned char)(src_port & 0xFF);
    udp_pkt[2] = (unsigned char)(dst_port >> 8);
    udp_pkt[3] = (unsigned char)(dst_port & 0xFF);
    udp_pkt[4] = (unsigned char)(udp_len >> 8);
    udp_pkt[5] = (unsigned char)(udp_len & 0xFF);
    udp_pkt[6] = 0x00;
    udp_pkt[7] = 0x00;

    for (i = 0; i < data_len; i++) udp_pkt[8 + i] = data[i];

    return ip_send(dest_mac, dest_ip, IP_PROTO_UDP, udp_pkt, udp_len);
}

static unsigned short dns_id = 0;

int dns_resolve(const char* domain, unsigned char* out_ip)
{
    unsigned char dns_q[512];
    unsigned char dns_server[4] = {8, 8, 8, 8};
    int i, q_len;
    int domain_len;
    int timeout;
    const char* p;

    if (!gateway_resolved) {
        unsigned char gw_ip[4] = {10, 0, 2, 2};
        if (!arp_resolve(gw_ip)) return 0;
    }

    dns_id++;
    dns_q[0] = (unsigned char)(dns_id >> 8);
    dns_q[1] = (unsigned char)(dns_id & 0xFF);
    dns_q[2] = 0x01; dns_q[3] = 0x00;
    dns_q[4] = 0x00; dns_q[5] = 0x01;
    dns_q[6] = 0x00; dns_q[7] = 0x00;
    dns_q[8] = 0x00; dns_q[9] = 0x00;
    dns_q[10] = 0x00; dns_q[11] = 0x00;

    p = domain;
    q_len = 12;
    while (*p) {
        domain_len = 0;
        while (p[domain_len] && p[domain_len] != '.') domain_len++;
        if (domain_len > 63) domain_len = 63;
        if (q_len + domain_len + 1 >= 512) {
            net_putstr("DNS domain too long\n");
            return 0;
        }
        dns_q[q_len++] = (unsigned char)domain_len;
        for (i = 0; i < domain_len; i++) dns_q[q_len++] = p[i];
        p += domain_len;
        if (*p == '.') p++;
    }
    if (q_len + 5 >= 512) {
        net_putstr("DNS domain too long\n");
        return 0;
    }
    dns_q[q_len++] = 0x00;

    dns_q[q_len++] = 0x00; dns_q[q_len++] = 0x01;
    dns_q[q_len++] = 0x00; dns_q[q_len++] = 0x01;

    udp_send(gateway_mac, dns_server, 12346, 53, dns_q, q_len);

    timeout = 5000000;
    while (timeout > 0) {
        unsigned char rx[2048];
        int len = net_wait_packet(rx, sizeof(rx));
        if (len > 0) {
            arp_respond(rx, len);

            if (len >= 34 && rx[12] == 0x08 && rx[13] == 0x00 && rx[23] == IP_PROTO_UDP) {
                unsigned short sport, dport;
                int ip_hdr_len = (rx[14] & 0x0F) * 4;
                if (len < ip_hdr_len + 8 + 14) { timeout--; continue; }
                sport = ((unsigned short)rx[14 + ip_hdr_len] << 8) | rx[14 + ip_hdr_len + 1];
                dport = ((unsigned short)rx[14 + ip_hdr_len + 2] << 8) | rx[14 + ip_hdr_len + 3];
                if (sport == 53 && dport == 12346) {
                    int dns_off = 14 + ip_hdr_len + 8;
                    unsigned short r_id = ((unsigned short)rx[dns_off] << 8) | rx[dns_off + 1];
                    if (r_id == dns_id) {
                        unsigned short ans_count = ((unsigned short)rx[dns_off + 6] << 8) | rx[dns_off + 7];
                        if (ans_count > 0) {
                            int pos = dns_off + q_len;
                            while (pos < len - 16) {
                                if (rx[pos] == 0xC0) {
                                    pos += 2;
                                    break;
                                }
                                if (rx[pos] == 0) {
                                    pos++;
                                    break;
                                }
                                if ((int)rx[pos] + 1 >= len - pos)
                                    break;
                                pos += (int)rx[pos] + 1;
                            }
                            while (pos < len - 16) {
                                if (rx[pos] == 0xC0) {
                                    pos += 2;
                                    break;
                                }
                                if (rx[pos] == 0) {
                                    pos++;
                                    break;
                                }
                                if ((int)rx[pos] + 1 >= len - pos)
                                    break;
                                pos += (int)rx[pos] + 1;
                            }
                            pos += 10;
                            if (pos + 4 <= len) {
                                if ((((unsigned short)rx[pos - 2] << 8) | rx[pos - 1]) == 0x0004) {
                                    out_ip[0] = rx[pos];
                                    out_ip[1] = rx[pos + 1];
                                    out_ip[2] = rx[pos + 2];
                                    out_ip[3] = rx[pos + 3];
                                    return 1;
                                }
                            }
                        }
                    }
                }
            }
        }
        timeout--;
    }

    net_putstr("DNS timeout\n");
    return 0;
}

static int ip_send(unsigned char* dest_mac, unsigned char* dest_ip, unsigned char proto, unsigned char* payload, int pay_len)
{
    unsigned char pkt[2048];
    int i, total_len;
    unsigned short cksum;

    if (pay_len > 2014) pay_len = 2014;
    total_len = 14 + 20 + pay_len;

    for (i = 0; i < 6; i++) pkt[i] = dest_mac[i];
    for (i = 0; i < 6; i++) pkt[i + 6] = local_mac[i];
    pkt[12] = 0x08;
    pkt[13] = 0x00;

    pkt[14] = 0x45;
    pkt[15] = 0x00;
    pkt[16] = (unsigned char)((20 + pay_len) >> 8);
    pkt[17] = (unsigned char)(20 + pay_len);
    pkt[18] = 0x00; pkt[19] = 0x00;
    pkt[20] = 0x00; pkt[21] = 0x00;
    pkt[22] = 0x40;
    pkt[23] = proto;
    pkt[24] = 0x00; pkt[25] = 0x00;
    for (i = 0; i < 4; i++) pkt[26 + i] = local_ip[i];
    for (i = 0; i < 4; i++) pkt[30 + i] = dest_ip[i];

    cksum = net_checksum(pkt + 14, 20);
    pkt[24] = (unsigned char)(cksum >> 8);
    pkt[25] = (unsigned char)(cksum & 0xFF);

    for (i = 0; i < pay_len; i++) pkt[34 + i] = payload[i];

    rtl_send_packet(pkt, total_len);
    return 1;
}

static int tcp_compute_checksum(unsigned char* src_ip, unsigned char* dst_ip, unsigned char* tcp_seg, int tcp_len)
{
    unsigned char pseudo[2048];
    int i, pseudo_len = 12 + tcp_len;

    for (i = 0; i < 4; i++) pseudo[i] = src_ip[i];
    for (i = 0; i < 4; i++) pseudo[4 + i] = dst_ip[i];
    pseudo[8] = 0;
    pseudo[9] = IP_PROTO_TCP;
    pseudo[10] = (unsigned char)(tcp_len >> 8);
    pseudo[11] = (unsigned char)(tcp_len & 0xFF);

    for (i = 0; i < tcp_len; i++) pseudo[12 + i] = tcp_seg[i];

    return net_checksum(pseudo, pseudo_len);
}

static void tcp_build_and_send(struct tcp_conn* conn,
                                unsigned short flags, unsigned char* data, int data_len)
{
    unsigned char tcp_seg[2048];
    int i, tcp_len;
    unsigned short cksum;

    if (data_len > 2028) data_len = 2028;
    tcp_len = 20 + data_len;

    tcp_seg[0] = (unsigned char)(conn->sport >> 8);
    tcp_seg[1] = (unsigned char)(conn->sport & 0xFF);
    tcp_seg[2] = (unsigned char)(conn->dport >> 8);
    tcp_seg[3] = (unsigned char)(conn->dport & 0xFF);

    tcp_seg[4] = (unsigned char)(conn->seq_local >> 24);
    tcp_seg[5] = (unsigned char)(conn->seq_local >> 16);
    tcp_seg[6] = (unsigned char)(conn->seq_local >> 8);
    tcp_seg[7] = (unsigned char)(conn->seq_local & 0xFF);

    tcp_seg[8]  = (unsigned char)(conn->seq_remote >> 24);
    tcp_seg[9]  = (unsigned char)(conn->seq_remote >> 16);
    tcp_seg[10] = (unsigned char)(conn->seq_remote >> 8);
    tcp_seg[11] = (unsigned char)(conn->seq_remote & 0xFF);

    tcp_seg[12] = 0x50;
    tcp_seg[13] = flags;
    tcp_seg[14] = 0x20; tcp_seg[15] = 0x00;
    tcp_seg[16] = 0x00; tcp_seg[17] = 0x00;
    tcp_seg[18] = 0x00; tcp_seg[19] = 0x00;

    for (i = 0; i < data_len; i++) tcp_seg[20 + i] = data[i];

    cksum = tcp_compute_checksum(local_ip, conn->remote_ip, tcp_seg, tcp_len);
    tcp_seg[16] = (unsigned char)(cksum >> 8);
    tcp_seg[17] = (unsigned char)(cksum & 0xFF);

    ip_send(conn->remote_mac, conn->remote_ip, IP_PROTO_TCP, tcp_seg, tcp_len);

    if (flags & TCP_FLAG_SYN) conn->seq_local++;
    if (data_len > 0 && (flags & (TCP_FLAG_SYN | TCP_FLAG_FIN)) == 0) {
        conn->seq_local += (unsigned int)data_len;
    }
}

static int tcp_verify_checksum(unsigned char* src_ip, unsigned char* dst_ip, unsigned char* tcp_seg, int tcp_len)
{
    unsigned short received = ((unsigned short)tcp_seg[16] << 8) | tcp_seg[17];
    unsigned short computed;
    unsigned char saved[2];
    saved[0] = tcp_seg[16];
    saved[1] = tcp_seg[17];
    tcp_seg[16] = 0;
    tcp_seg[17] = 0;
    computed = tcp_compute_checksum(src_ip, dst_ip, tcp_seg, tcp_len);
    tcp_seg[16] = saved[0];
    tcp_seg[17] = saved[1];
    if (computed == 0) computed = 0xFFFF;
    return received == computed;
}

static int tcp_connect_conn(struct tcp_conn* conn)
{
    int timeout;
    int syn_retries = 0;
    int rto_cycles = 1000000;

    if (!conn->mac_resolved) {
        if (!arp_resolve(conn->remote_ip)) return 0;
        {
            int i;
            for (i = 0; i < 6; i++) conn->remote_mac[i] = gateway_mac[i];
        }
        conn->mac_resolved = 1;
    }

    conn->seq_local = 1000;
    conn->seq_remote = 0;
    conn->state = TCP_STATE_SYN_SENT;

    while (syn_retries < 5) {
        tcp_build_and_send(conn, TCP_FLAG_SYN, (unsigned char*)0, 0);
        syn_retries++;

        timeout = rto_cycles;
        while (timeout > 0) {
            unsigned char rx[2048];
            int len = net_wait_packet(rx, sizeof(rx));
            if (len > 0) {
                arp_respond(rx, len);

                if (len >= 34 && rx[12] == 0x08 && rx[13] == 0x00 && rx[23] == IP_PROTO_TCP) {
                    unsigned short sport, dport;
                    int ip_hdr_len = (rx[14] & 0x0F) * 4;
                    int tcp_start = 14 + ip_hdr_len;
                    sport = ((unsigned short)rx[tcp_start] << 8) | rx[tcp_start + 1];
                    dport = ((unsigned short)rx[tcp_start + 2] << 8) | rx[tcp_start + 3];

                    if (sport == conn->dport && dport == conn->sport) {
                        unsigned char flags;
                        int tcp_hdr_len = ((rx[tcp_start + 12] >> 4) & 0x0F) * 4;
                        int tcp_total = len - tcp_start;
                        if (tcp_total < tcp_hdr_len) { timeout--; continue; }
                        if (len >= 14 + 4) {
                            unsigned char src_ip[4], dst_ip[4];
                            int i;
                            for (i = 0; i < 4; i++) src_ip[i] = rx[14 + 12 + i];
                            for (i = 0; i < 4; i++) dst_ip[i] = rx[14 + 16 + i];
                            if (!tcp_verify_checksum(src_ip, dst_ip, rx + tcp_start, tcp_total)) {
                                timeout--;
                                continue;
                            }
                        }
                        flags = rx[tcp_start + 13];
                        if (flags & TCP_FLAG_RST) {
                            conn->state = TCP_STATE_CLOSED;
                            return 0;
                        }
                        if (conn->state == TCP_STATE_SYN_SENT && (flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
                            conn->seq_remote  = ((unsigned int)rx[tcp_start + 4] << 24) | ((unsigned int)rx[tcp_start + 5] << 16) | ((unsigned int)rx[tcp_start + 6] << 8) | (unsigned int)rx[tcp_start + 7];
                            conn->seq_remote++;
                            conn->state = TCP_STATE_ESTABLISHED;

                            tcp_build_and_send(conn, TCP_FLAG_ACK, (unsigned char*)0, 0);
                            return 1;
                        }
                    }
                }
            }
            timeout--;
        }
        rto_cycles = rto_cycles * 2;
        if (rto_cycles > 20000000)
            rto_cycles = 20000000;
    }

    conn->state = TCP_STATE_CLOSED;
    return 0;
}

static int tcp_wait_for_ack(struct tcp_conn* conn, unsigned int ack_expected, int timeout_cycles)
{
    while (timeout_cycles > 0) {
        unsigned char rx[2048];
        int len = net_wait_packet(rx, sizeof(rx));
        if (len > 0) {
            arp_respond(rx, len);
            if (len >= 34 && rx[12] == 0x08 && rx[13] == 0x00 && rx[23] == IP_PROTO_TCP) {
                int ip_hdr_len = (rx[14] & 0x0F) * 4;
                int tcp_start = 14 + ip_hdr_len;
                unsigned short sport, dport;
                if (tcp_start + 20 > len) { timeout_cycles--; continue; }
                sport = ((unsigned short)rx[tcp_start] << 8) | rx[tcp_start + 1];
                dport = ((unsigned short)rx[tcp_start + 2] << 8) | rx[tcp_start + 3];
                if (sport != conn->dport || dport != conn->sport) { timeout_cycles--; continue; }
                {
                    int tcp_total = len - tcp_start;
                    int tcp_hdr_len = ((rx[tcp_start + 12] >> 4) & 0x0F) * 4;
                    if (tcp_total < tcp_hdr_len) { timeout_cycles--; continue; }
                    {
                        unsigned char src_ip[4], dst_ip[4];
                        int i;
                        for (i = 0; i < 4; i++) src_ip[i] = rx[14 + 12 + i];
                        for (i = 0; i < 4; i++) dst_ip[i] = rx[14 + 16 + i];
                        if (!tcp_verify_checksum(src_ip, dst_ip, rx + tcp_start, tcp_total)) {
                            timeout_cycles--;
                            continue;
                        }
                    }
                }
                {
                    unsigned int rx_ack = ((unsigned int)rx[tcp_start + 8] << 24) | ((unsigned int)rx[tcp_start + 9] << 16) | ((unsigned int)rx[tcp_start + 10] << 8) | (unsigned int)rx[tcp_start + 11];
                    if (rx_ack >= ack_expected)
                        return 1;
                }
            }
        }
        timeout_cycles--;
    }
    return 0;
}

static void tcp_send_data_conn(struct tcp_conn* conn, unsigned char* data, int data_len)
{
    int rto_cycles = 1000000;
    int retries = 0;
    int copy_len = data_len;
    if (copy_len > MAX_TCP_SEND_SIZE) copy_len = MAX_TCP_SEND_SIZE;
    if (copy_len < 0) copy_len = 0;
    {
        int i;
        for (i = 0; i < copy_len; i++)
            conn->unacked_data[i] = data[i];
    }
    conn->unacked_len = copy_len;
    conn->unacked_in_flight = 1;
    conn->unacked_seq = conn->seq_local;

    while (retries < 4) {
        tcp_build_and_send(conn, TCP_FLAG_PSH | TCP_FLAG_ACK, conn->unacked_data, conn->unacked_len);
        if (tcp_wait_for_ack(conn, conn->unacked_seq + (unsigned int)conn->unacked_len, rto_cycles)) {
            conn->seq_local += (unsigned int)conn->unacked_len;
            conn->unacked_in_flight = 0;
            return;
        }
        retries++;
        rto_cycles = rto_cycles * 2;
        if (rto_cycles > 16000000) rto_cycles = 16000000;
    }
    conn->seq_local += (unsigned int)conn->unacked_len;
    conn->unacked_in_flight = 0;
}

static int tcp_recv_data_conn(struct tcp_conn* conn, unsigned char* buf, int max_len, int timeout_cycles)
{
    while (timeout_cycles > 0) {
        unsigned char rx[2048];
        int len = net_wait_packet(rx, sizeof(rx));
        if (len > 0) {
            arp_respond(rx, len);

            if (len >= 34 && rx[12] == 0x08 && rx[13] == 0x00 && rx[23] == IP_PROTO_TCP) {
                int ip_hdr_len = (rx[14] & 0x0F) * 4;
                int tcp_start = 14 + ip_hdr_len;
                int tcp_data_offset;
                int data_start;
                int data_len;
                unsigned short sport, dport;
                unsigned char flags;

                if (tcp_start + 20 > len) { timeout_cycles--; continue; }
                sport = ((unsigned short)rx[tcp_start] << 8) | rx[tcp_start + 1];
                dport = ((unsigned short)rx[tcp_start + 2] << 8) | rx[tcp_start + 3];

                if (sport != conn->dport || dport != conn->sport) {
                    timeout_cycles--;
                    continue;
                }
                {
                    int tcp_total = len - tcp_start;
                    int tcp_hdr_len = ((rx[tcp_start + 12] >> 4) & 0x0F) * 4;
                    if (tcp_total < tcp_hdr_len) { timeout_cycles--; continue; }
                    {
                        unsigned char src_ip[4], dst_ip[4];
                        int i;
                        for (i = 0; i < 4; i++) src_ip[i] = rx[14 + 12 + i];
                        for (i = 0; i < 4; i++) dst_ip[i] = rx[14 + 16 + i];
                        if (!tcp_verify_checksum(src_ip, dst_ip, rx + tcp_start, tcp_total)) {
                            timeout_cycles--;
                            continue;
                        }
                    }
                }

                flags = rx[tcp_start + 13];
                tcp_data_offset = ((rx[tcp_start + 12] >> 4) & 0x0F) * 4;
                data_start = tcp_start + tcp_data_offset;
                data_len = len - data_start;

                if (flags & TCP_FLAG_RST) {
                    conn->state = TCP_STATE_CLOSED;
                    return -1;
                }

                if (flags & TCP_FLAG_FIN) {
                    conn->seq_remote++;
                    tcp_build_and_send(conn, TCP_FLAG_ACK, (unsigned char*)0, 0);
                    conn->state = TCP_STATE_CLOSED;
                    return 0;
                }

                if (data_len > 0) {
                    unsigned int rx_seq = ((unsigned int)rx[tcp_start + 4] << 24) | ((unsigned int)rx[tcp_start + 5] << 16) | ((unsigned int)rx[tcp_start + 6] << 8) | (unsigned int)rx[tcp_start + 7];

                    if (rx_seq == conn->seq_remote) {
                        int copy_len = data_len;
                        if (copy_len > max_len) copy_len = max_len;
                        {
                            int i;
                            for (i = 0; i < copy_len; i++) buf[i] = rx[data_start + i];
                        }

                        conn->seq_remote += (unsigned int)data_len;

                        tcp_build_and_send(conn, TCP_FLAG_ACK, (unsigned char*)0, 0);
                        return copy_len;
                    }
                }

                if (flags & TCP_FLAG_SYN) {
                    conn->seq_remote = ((unsigned int)rx[tcp_start + 4] << 24) | ((unsigned int)rx[tcp_start + 5] << 16) | ((unsigned int)rx[tcp_start + 6] << 8) | (unsigned int)rx[tcp_start + 7];
                    conn->seq_remote++;
                    tcp_build_and_send(conn, TCP_FLAG_ACK, (unsigned char*)0, 0);
                }
            }
        }
        timeout_cycles--;
    }
    return 0;
}

static void tcp_close_conn_internal(struct tcp_conn* conn)
{
    if (conn->state == TCP_STATE_ESTABLISHED) {
        conn->state = TCP_STATE_FIN_SENT;
        tcp_build_and_send(conn, TCP_FLAG_FIN | TCP_FLAG_ACK, (unsigned char*)0, 0);
    }
}

int tcp_connect_remote(unsigned char* ip, unsigned short port)
{
    int id = tcp_alloc_conn();
    struct tcp_conn* conn;
    int ok;
    if (id < 0) return -1;
    if (!tcp_table_lock_inited) return -1;
    spinlock_acquire(&tcp_table_lock);
    conn = &g_connections[id];
    conn->state = TCP_STATE_CLOSED;
    conn->sport = tcp_next_sport++;
    conn->dport = port;
    {
        int i;
        for (i = 0; i < 4; i++) conn->remote_ip[i] = ip[i];
    }
    conn->mac_resolved = 0;
    spinlock_release(&tcp_table_lock);
    ok = tcp_connect_conn(conn);
    if (!ok) {
        spinlock_acquire(&tcp_table_lock);
        conn->state = TCP_STATE_CLOSED;
        conn->mac_resolved = 0;
        spinlock_release(&tcp_table_lock);
        return -1;
    }
    return id;
}

int tcp_send(int conn_id, unsigned char* data, int data_len)
{
    struct tcp_conn* conn;
    if (conn_id < 0 || conn_id >= MAX_TCP_CONN) return -1;
    if (!tcp_table_lock_inited) return -1;
    spinlock_acquire(&tcp_table_lock);
    conn = &g_connections[conn_id];
    if (conn->state == TCP_STATE_CLOSED) {
        spinlock_release(&tcp_table_lock);
        return -1;
    }
    tcp_send_data_conn(conn, data, data_len);
    spinlock_release(&tcp_table_lock);
    return data_len;
}

int tcp_recv(int conn_id, unsigned char* buf, int max_len, int timeout)
{
    struct tcp_conn* conn;
    int ret;
    if (conn_id < 0 || conn_id >= MAX_TCP_CONN) return -1;
    if (!tcp_table_lock_inited) return -1;
    spinlock_acquire(&tcp_table_lock);
    conn = &g_connections[conn_id];
    if (conn->state == TCP_STATE_CLOSED) {
        spinlock_release(&tcp_table_lock);
        return -1;
    }
    ret = tcp_recv_data_conn(conn, buf, max_len, timeout);
    spinlock_release(&tcp_table_lock);
    return ret;
}

void tcp_close_conn(int conn_id)
{
    struct tcp_conn* conn;
    if (conn_id < 0 || conn_id >= MAX_TCP_CONN) return;
    if (!tcp_table_lock_inited) return;
    spinlock_acquire(&tcp_table_lock);
    conn = &g_connections[conn_id];
    if (conn->state == TCP_STATE_CLOSED) {
        spinlock_release(&tcp_table_lock);
        return;
    }
    tcp_close_conn_internal(conn);
    conn->state = TCP_STATE_CLOSED;
    conn->mac_resolved = 0;
    spinlock_release(&tcp_table_lock);
}

static int dhcp_send_and_recv(unsigned char* tx_pkt, int tx_len,
                               unsigned char* rx_buf, int rx_max,
                               int wait_cycles)
{
    unsigned char bcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    unsigned char bcast_ip[4] = {255, 255, 255, 255};
    unsigned char saved_ip[4];
    int i, result = 0;

    for (i = 0; i < 4; i++) saved_ip[i] = local_ip[i];
    dhcp_in_progress = 1;
    for (i = 0; i < 4; i++) local_ip[i] = 0;

    rtl_send_packet(tx_pkt, tx_len);

    for (i = 0; i < 4; i++) local_ip[i] = saved_ip[i];
    dhcp_in_progress = 0;

    while (wait_cycles > 0) {
        int len = net_wait_packet(rx_buf, rx_max);
        if (len > 0) {
            if (len >= 42 && rx_buf[12] == 0x08 && rx_buf[13] == 0x00 && rx_buf[23] == IP_PROTO_UDP) {
                unsigned short dport = ((unsigned short)rx_buf[36] << 8) | rx_buf[37];
                if (dport == 68) {
                    int ip_hdr_len = (rx_buf[14] & 0x0F) * 4;
                    int udp_start = 14 + ip_hdr_len + 8;
                    int dhcp_len = len - udp_start;
                    if (dhcp_len > 0) {
                        if (rx_buf[14 + ip_hdr_len + 2] == (unsigned char)((dhcp_len + 8) >> 8) &&
                            rx_buf[14 + ip_hdr_len + 3] == (unsigned char)((dhcp_len + 8) & 0xFF)) {
                            result = dhcp_len;
                            break;
                        }
                    }
                }
            }
        }
        wait_cycles--;
    }

    for (i = 0; i < 4; i++) local_ip[i] = saved_ip[i];
    return result;
}

static int dhcp_build_common(unsigned char* pkt, int msg_type,
                              unsigned char* req_ip, unsigned char* server_id)
{
    int i, off;

    pkt[0] = 1;
    pkt[1] = 1;
    pkt[2] = 6;
    pkt[3] = 0;
    pkt[4] = 0x12; pkt[5] = 0x34; pkt[6] = 0x56; pkt[7] = 0x78;
    pkt[8] = 0; pkt[9] = 0;
    pkt[10] = 0x80; pkt[11] = 0x00;

    for (i = 0; i < 4; i++) pkt[12 + i] = 0;
    for (i = 0; i < 4; i++) pkt[16 + i] = 0;
    for (i = 0; i < 4; i++) pkt[20 + i] = 0;
    for (i = 0; i < 4; i++) pkt[24 + i] = 0;

    for (i = 0; i < 6; i++) pkt[28 + i] = local_mac[i];
    for (i = 6; i < 16; i++) pkt[28 + i] = 0;

    for (i = 0; i < 64; i++) pkt[44 + i] = 0;
    for (i = 0; i < 128; i++) pkt[108 + i] = 0;

    pkt[236] = 0x63; pkt[237] = 0x82; pkt[238] = 0x53; pkt[239] = 0x63;

    off = 240;
    pkt[off++] = 53;
    pkt[off++] = 1;
    pkt[off++] = (unsigned char)msg_type;

    if (req_ip) {
        pkt[off++] = 50;
        pkt[off++] = 4;
        for (i = 0; i < 4; i++) pkt[off++] = req_ip[i];
    }

    if (server_id) {
        pkt[off++] = 54;
        pkt[off++] = 4;
        for (i = 0; i < 4; i++) pkt[off++] = server_id[i];
    }

    if (msg_type == 1) {
        pkt[off++] = 55;
        pkt[off++] = 3;
        pkt[off++] = 1;
        pkt[off++] = 3;
        pkt[off++] = 6;
    }

    pkt[off++] = 255;

    while ((off - 14 - 20 - 8) < 300) {
        pkt[off++] = 0;
    }

    return off;
}

static void dhcp_build_ip_udp(unsigned char* pkt, int total_len)
{
    int i, udp_len;
    unsigned short cksum;

    udp_len = total_len - 14 - 20;

    pkt[14] = 0x45;
    pkt[15] = 0x00;
    pkt[16] = (unsigned char)((20 + udp_len) >> 8);
    pkt[17] = (unsigned char)(20 + udp_len);
    pkt[18] = 0x00; pkt[19] = 0x00;
    pkt[20] = 0x00; pkt[21] = 0x00;
    pkt[22] = 0x40;
    pkt[23] = IP_PROTO_UDP;
    pkt[24] = 0x00; pkt[25] = 0x00;

    for (i = 0; i < 4; i++) pkt[26 + i] = 0;
    for (i = 0; i < 4; i++) pkt[30 + i] = 255;

    cksum = net_checksum(pkt + 14, 20);
    pkt[24] = (unsigned char)(cksum >> 8);
    pkt[25] = (unsigned char)(cksum & 0xFF);

    pkt[34] = 0; pkt[35] = 68;
    pkt[36] = 0; pkt[37] = 67;
    pkt[38] = (unsigned char)(udp_len >> 8);
    pkt[39] = (unsigned char)(udp_len & 0xFF);
    pkt[40] = 0; pkt[41] = 0;

    for (i = 0; i < 6; i++) {
        pkt[i] = 0xFF;
        pkt[i + 6] = local_mac[i];
    }
    pkt[12] = 0x08; pkt[13] = 0x00;
}

int dhcp_request(void)
{
    unsigned char pkt[1024];
    unsigned char rx[2048];
    unsigned char dhcp_rx[512];
    unsigned char offered_ip[4];
    unsigned char server_id[4];
    unsigned char gateway[4];
    unsigned char dns_server[4];
    unsigned char subnet[4];
    int i, off, dhcp_len;
    int got_offer = 0;
    unsigned char bcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    net_putstr("DHCP: sending discover...\n");

    off = dhcp_build_common(pkt, 1, (unsigned char*)0, (unsigned char*)0);
    dhcp_build_ip_udp(pkt, off);
    pkt[26] = 0; pkt[27] = 0; pkt[28] = 0; pkt[29] = 0;
    for (i = 0; i < 6; i++) pkt[i] = 0xFF;
    for (i = 0; i < 6; i++) pkt[i + 6] = local_mac[i];

    dhcp_len = dhcp_send_and_recv(pkt, off, rx, sizeof(rx), 50000);
    if (dhcp_len <= 0) {
        net_putstr("DHCP: no offer\n");
        return 0;
    }

    {
        int ip_hl = (rx[14] & 0x0F) * 4;
        int dhcp_start = 14 + ip_hl + 8;

        if (rx[dhcp_start + 0] != 2) {
            net_putstr("DHCP: not a reply\n");
            return 0;
        }

        if (rx[dhcp_start + 1] != 1 || rx[dhcp_start + 2] != 6) {
            net_putstr("DHCP: bad htype/hlen\n");
            return 0;
        }

        for (i = 0; i < 4; i++) offered_ip[i] = rx[dhcp_start + 16 + i];

        {
            int opt_off = dhcp_start + 240;
            int magic = (rx[opt_off] << 24) | (rx[opt_off+1] << 16) | (rx[opt_off+2] << 8) | rx[opt_off+3];
            if (magic != 0x63825363) {
                net_putstr("DHCP: bad magic\n");
                return 0;
            }
            opt_off += 4;

            gateway[0] = gateway[1] = gateway[2] = gateway[3] = 0;
            dns_server[0] = dns_server[1] = dns_server[2] = dns_server[3] = 0;
            subnet[0] = subnet[1] = subnet[2] = subnet[3] = 0;
            server_id[0] = server_id[1] = server_id[2] = server_id[3] = 0;

            while (opt_off < dhcp_start + dhcp_len) {
                unsigned char tag = rx[opt_off++];
                unsigned char len;
                if (tag == 255) break;
                if (tag == 0) continue;
                len = rx[opt_off++];
                if (tag == 53 && len == 1) {
                    int mt = rx[opt_off];
                    if (mt != 2) {
                        net_putstr("DHCP: unexpected type ");
                        net_putnum(mt);
                        net_putstr("\n");
                        return 0;
                    }
                } else if (tag == 54 && len == 4) {
                    for (i = 0; i < 4; i++) server_id[i] = rx[opt_off + i];
                } else if (tag == 1 && len == 4) {
                    for (i = 0; i < 4; i++) subnet[i] = rx[opt_off + i];
                } else if (tag == 3 && len == 4) {
                    for (i = 0; i < 4; i++) gateway[i] = rx[opt_off + i];
                } else if (tag == 6 && len == 4) {
                    for (i = 0; i < 4; i++) dns_server[i] = rx[opt_off + i];
                }
                opt_off += len;
            }
        }
    }

    net_putstr("DHCP: offer received, sending request...\n");

    off = dhcp_build_common(pkt, 3, offered_ip, server_id);
    dhcp_build_ip_udp(pkt, off);
    for (i = 0; i < 6; i++) pkt[i] = 0xFF;
    for (i = 0; i < 6; i++) pkt[i + 6] = local_mac[i];

    {
        int sport_off = 14 + 20 + 0;
        pkt[sport_off] = 0; pkt[sport_off + 1] = 68;
    }

    dhcp_len = dhcp_send_and_recv(pkt, off, rx, sizeof(rx), 50000);
    if (dhcp_len <= 0) {
        net_putstr("DHCP: no ack\n");
        return 0;
    }

    {
        int ip_hl = (rx[14] & 0x0F) * 4;
        int dhcp_start = 14 + ip_hl + 8;

        if (rx[dhcp_start + 0] != 2) {
            net_putstr("DHCP: ack not a reply\n");
            return 0;
        }

        for (i = 0; i < 4; i++) local_ip[i] = rx[dhcp_start + 16 + i];

        {
            int opt_off = dhcp_start + 240;
            int magic = (rx[opt_off] << 24) | (rx[opt_off+1] << 16) | (rx[opt_off+2] << 8) | rx[opt_off+3];
            if (magic != 0x63825363) {
                net_putstr("DHCP: ack bad magic\n");
                return 1;
            }
            opt_off += 4;

            while (opt_off < dhcp_start + dhcp_len) {
                unsigned char tag = rx[opt_off++];
                unsigned char len;
                if (tag == 255) break;
                if (tag == 0) continue;
                len = rx[opt_off++];
                if (tag == 53 && len == 1) {
                    int mt = rx[opt_off];
                    if (mt != 5 && mt != 4) {
                        net_putstr("DHCP: unexpected ack type ");
                        net_putnum(mt);
                        net_putstr("\n");
                        return 0;
                    }
                } else if (tag == 1 && len == 4) {
                    for (i = 0; i < 4; i++) subnet[i] = rx[opt_off + i];
                } else if (tag == 3 && len == 4) {
                    for (i = 0; i < 4; i++) gateway[i] = rx[opt_off + i];
                } else if (tag == 6 && len == 4) {
                    for (i = 0; i < 4; i++) dns_server[i] = rx[opt_off + i];
                }
                opt_off += len;
            }
        }

        if (gateway[0] != 0 || gateway[1] != 0 || gateway[2] != 0 || gateway[3] != 0) {
            unsigned char gw_ip[4];
            for (i = 0; i < 4; i++) gw_ip[i] = gateway[i];
            for (i = 0; i < 6; i++) gateway_mac[i] = 0;
            gateway_resolved = 0;
            arp_resolve(gw_ip);
        }

        net_putstr("DHCP: OK, IP ");
        net_putnum(local_ip[0]); screen_put_char('.');
        net_putnum(local_ip[1]); screen_put_char('.');
        net_putnum(local_ip[2]); screen_put_char('.');
        net_putnum(local_ip[3]);
        net_putstr("\n");
        return 1;
    }
}

int network_init(void)
{
    int i;

    net_putstr("Network init...\n");

    if (!rtl_detect()) {
        net_putstr("No RTL8139 found\n");
        return 0;
    }

    rtl_init();

    for (i = 0; i < 6; i++) gateway_mac[i] = 0;

    net_putstr("DHCP...\n");
    if (dhcp_request()) {
        return 1;
    }

    net_putstr("DHCP failed, using static config\n");
    {
        unsigned char static_gw[4] = {10, 0, 2, 2};
        local_ip[0] = 10; local_ip[1] = 0; local_ip[2] = 2; local_ip[3] = 15;
        if (!arp_resolve(static_gw)) {
            net_putstr("Gateway unresolved, network limited\n");
        }
    }

    return 1;
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

int network_http_get(const char* host, const char* path)
{
    unsigned char* rx_buf_data = (unsigned char*)kmalloc(4096);
    char request[1024];
    int req_len, i, total = 0;
    int header_done = 0;
    int body_len = 0;
    int line_pos = 0;
    char line_buf[1024];
    int is_chunked = 0;
    int chunk_state = 0;
    int chunk_size = 0;
    int chunk_remaining = 0;
    int chunk_crlf = 0;
    int status_code = 0;
    int content_length = -1;
    int body_remaining = 0;
    unsigned char dst_ip[4];
    int conn_id;
    struct tcp_conn* conn;

#define CHUNK_STATE_SIZE     0
#define CHUNK_STATE_DATA     1
#define CHUNK_STATE_END_CRLF 2
#define CHUNK_STATE_DONE     3

    if (!rx_buf_data) {
        net_putstr("OOM: http rx buffer\n");
        return 0;
    }

    if (!io_base) {
        net_putstr("Network not initialized\n");
        goto cleanup;
    }

    if (!gateway_resolved) {
        unsigned char gw_ip[4] = {10, 0, 2, 2};
        if (!arp_resolve(gw_ip)) goto cleanup;
    }

    if (!ip_parse(host, dst_ip)) {
        net_putstr("Resolving ");
        net_putstr(host);
        net_putstr("...\n");
        if (!dns_resolve(host, dst_ip)) {
            net_putstr("DNS failed: ");
            net_putstr(host);
            screen_put_char('\n');
            goto cleanup;
        }
    }

    net_putstr("Connecting to ");
    net_putstr(host);
    net_putstr("...\n");

    conn_id = tcp_connect_remote(dst_ip, 80);
    if (conn_id < 0) goto cleanup;

    if (!tcp_table_lock_inited) goto cleanup;
    spinlock_acquire(&tcp_table_lock);
    conn = &g_connections[conn_id];
    if (conn->state == TCP_STATE_CLOSED) {
        spinlock_release(&tcp_table_lock);
        goto cleanup;
    }
    spinlock_release(&tcp_table_lock);

    net_putstr("Connected, sending request...\n");

    req_len = 0;
    {
        const char* method = "GET ";
        for (i = 0; method[i] && req_len < 1023; i++) request[req_len++] = method[i];
        if (path[0] == 0) {
            if (req_len < 1023) request[req_len++] = '/';
        } else {
            for (i = 0; path[i] && req_len < 1023; i++) request[req_len++] = path[i];
        }
        {
            const char* http_ver = " HTTP/1.1\r\nHost: ";
            for (i = 0; http_ver[i] && req_len < 1023; i++) request[req_len++] = http_ver[i];
        }
        for (i = 0; host[i] && req_len < 1023; i++) request[req_len++] = host[i];
        {
            const char* ending = "\r\nUser-Agent: HarLin-Kernel/1.6.2\r\nAccept: */*\r\nConnection: close\r\n\r\n";
            for (i = 0; ending[i] && req_len < 1023; i++) request[req_len++] = ending[i];
        }
    }

    tcp_send(conn_id, (unsigned char*)request, req_len);
    net_putstr("Waiting for response...\n\n");

    header_done = 0;
    line_pos = 0;
    body_len = 0;
    content_length = -1;
    body_remaining = -1;

    while (1) {
        int len = tcp_recv(conn_id, rx_buf_data, sizeof(rx_buf_data) - 1, 5000000);
        if (len < 0) {
            net_putstr("\nConnection reset\n");
            break;
        }
        if (len == 0) {
            if (!tcp_table_lock_inited) break;
            spinlock_acquire(&tcp_table_lock);
            conn = &g_connections[conn_id];
            if (conn_id < 0 || conn_id >= MAX_TCP_CONN || conn->state == TCP_STATE_CLOSED) {
                spinlock_release(&tcp_table_lock);
                break;
            }
            spinlock_release(&tcp_table_lock);
            net_putstr("\nNo more data\n");
            break;
        }

        total += len;

        for (i = 0; i < len; i++) {
            char c = (char)rx_buf_data[i];
            if (!header_done) {
                if (c == '\n') {
                    if (line_pos > 0 && line_buf[line_pos - 1] == '\r') line_buf[line_pos - 1] = 0;
                    else if (line_pos < 1024) line_buf[line_pos] = 0;

                    if (line_pos == 0 || (line_pos == 1 && line_buf[0] == '\r')) {
                        header_done = 1;
                        if (status_code == 0) {
                            status_code = 0;
                        }
                        if (status_code >= 300 && status_code < 400) {
                            net_putstr("- redirect/no body -\n");
                        } else {
                            net_putstr("- body -\n");
                        }
                        line_pos = 0;
                        continue;
                    }

                    if (status_code == 0 && line_pos > 5 &&
                        (line_buf[0] == 'H' || line_buf[0] == 'h') &&
                        (line_buf[1] == 'T' || line_buf[1] == 't') &&
                        (line_buf[2] == 'T' || line_buf[2] == 't') &&
                        (line_buf[3] == 'P' || line_buf[3] == 'p') &&
                        (line_buf[4] == '/' || line_buf[4] == ' ')) {
                        int j = 0;
                        while (line_buf[j] && line_buf[j] != ' ') j++;
                        while (line_buf[j] == ' ') j++;
                        status_code = 0;
                        while (line_buf[j] >= '0' && line_buf[j] <= '9') {
                            status_code = status_code * 10 + (line_buf[j] - '0');
                            j++;
                        }
                        net_putstr("HTTP ");
                        net_putnum(status_code);
                        net_putstr("\n");
                    } else if (status_code != 0) {
                        int j = 0;
                        while (line_buf[j] == ' ' || line_buf[j] == '\t') j++;
                        if (Harlin_Compare(line_buf + j, "Content-Length:") == 0) {
                            int v = 0;
                            j += 15;
                            while (line_buf[j] == ' ' || line_buf[j] == '\t') j++;
                            while (line_buf[j] >= '0' && line_buf[j] <= '9') {
                                v = v * 10 + (line_buf[j] - '0');
                                j++;
                            }
                            content_length = v;
                            body_remaining = v;
                        } else if (Harlin_Compare(line_buf + j, "Transfer-Encoding:") == 0) {
                            int k = j + 18;
                            while (line_buf[k] == ' ' || line_buf[k] == '\t') k++;
                            if (Harlin_Compare(line_buf + k, "chunked") == 0 ||
                                Harlin_Compare(line_buf + k, "Chunked") == 0) {
                                is_chunked = 1;
                            }
                        }
                    }

                    line_pos = 0;
                    continue;
                }
                if (line_pos >= 1023) {
                    line_pos = 0;
                    continue;
                }
                line_buf[line_pos++] = c;
            } else {
                if (is_chunked) {
                    if (chunk_state == CHUNK_STATE_DONE)
                        continue;

                    if (chunk_state == CHUNK_STATE_SIZE) {
                        int v = hex_value(c);
                        if (v >= 0) {
                            chunk_size = (chunk_size * 16) + v;
                        } else if (c == '\r') {
                            chunk_crlf = 1;
                        } else if (c == '\n') {
                            if (chunk_size == 0) {
                                chunk_state = CHUNK_STATE_DONE;
                            } else {
                                chunk_remaining = chunk_size;
                                chunk_state = CHUNK_STATE_DATA;
                            }
                            chunk_size = 0;
                            chunk_crlf = 0;
                        } else if (c == ';' || c == ' ' || c == '\t') {
                            continue;
                        } else {
                            continue;
                        }
                    } else if (chunk_state == CHUNK_STATE_DATA) {
                        screen_put_char(c);
                        if (c == '\n') body_len++;
                        chunk_remaining--;
                        if (chunk_remaining == 0) {
                            chunk_state = CHUNK_STATE_END_CRLF;
                            chunk_crlf = 0;
                        }
                    } else if (chunk_state == CHUNK_STATE_END_CRLF) {
                        if (c == '\r') {
                            chunk_crlf = 1;
                        } else if (c == '\n') {
                            chunk_state = CHUNK_STATE_SIZE;
                            chunk_crlf = 0;
                        }
                    }
                } else {
                    screen_put_char(c);
                    if (c == '\n') body_len++;
                    if (body_remaining > 0) {
                        body_remaining--;
                        if (body_remaining == 0) {
                            goto body_done;
                        }
                    }
                }
            }
        }
    }
body_done:

    tcp_close_conn(conn_id);

    screen_put_char('\n');
    net_putstr("Done, ");
    net_putnum(body_len);
    net_putstr(" lines, status ");
    net_putnum(status_code);
    if (content_length >= 0) {
        net_putstr(", length ");
        net_putnum(content_length);
    }
    net_putstr("\n");
    kfree(rx_buf_data);
    return status_code > 0 && status_code < 400 ? 1 : 0;
cleanup:
    kfree(rx_buf_data);
    return 0;
#undef CHUNK_STATE_SIZE
#undef CHUNK_STATE_DATA
#undef CHUNK_STATE_END_CRLF
#undef CHUNK_STATE_DONE
}

int network_https_get(const char* host, const char* path)
{
    net_putstr("HTTPS not yet supported by HarLin\n");
    net_putstr("TLS 1.2/1.3 handshake requires crypto driver (see drv_loader)\n");
    net_putstr("Falling back to plaintext HTTP...\n");
    return network_http_get(host, path);
}
