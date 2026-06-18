#include "io.h"
#include "screen.h"
#include "network.h"

#define PCI_ADDR 0xCF8
#define PCI_DATA 0xCFC

#define RX_BUF_SIZE 0x2000
#define RX_BUF_PHYS 0x210000
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

static unsigned short tcp_sport = 12345;
static unsigned short tcp_dport = 80;
static unsigned int   tcp_seq_local  = 0;
static unsigned int   tcp_seq_remote = 0;
static int tcp_state = 0;

static unsigned char tx_buf[TX_BUF_SIZE] __attribute__((aligned(4)));
static volatile unsigned char* rx_buf = (volatile unsigned char*)RX_BUF_PHYS;

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

static unsigned int pci_read(unsigned char bus, unsigned char dev, unsigned char func, unsigned char off)
{
    unsigned int addr = 0x80000000 | ((unsigned int)bus << 16) | ((unsigned int)dev << 11) | ((unsigned int)func << 8) | (off & 0xFC);
    outl(PCI_ADDR, addr);
    return inl(PCI_DATA);
}

static void pci_write(unsigned char bus, unsigned char dev, unsigned char func, unsigned char off, unsigned int val)
{
    unsigned int addr = 0x80000000 | ((unsigned int)bus << 16) | ((unsigned int)dev << 11) | ((unsigned int)func << 8) | (off & 0xFC);
    outl(PCI_ADDR, addr);
    outl(PCI_DATA, val);
}

static int rtl_detect(void)
{
    unsigned char bus, dev;
    unsigned int id, bar, cmd;
    for (bus = 0; bus < 2; bus++) {
        for (dev = 0; dev < 32; dev++) {
            id = pci_read(bus, dev, 0, 0);
            if (id == 0xFFFFFFFF || id == 0) continue;
            if (id == 0x813910EC) {
                cmd = pci_read(bus, dev, 0, 0x04);
                pci_write(bus, dev, 0, 0x04, cmd | 0x07);
                bar = pci_read(bus, dev, 0, 0x10);
                io_base = (unsigned short)(bar & 0xFFFC);
                net_putstr("RTL8139 found at IO 0x");
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

    outl(io_base + 0x30, RX_BUF_PHYS);

    outl(io_base + 0x40, 0x03000700);
    outl(io_base + 0x44, 0x0000070F);

    outb(io_base + 0x37, 0x0C);

    outw(io_base + 0x3C, 0x0000);

    net_putstr("RTL8139 initialized\n");
}

static void rtl_send_packet(unsigned char* pkt, int len)
{
    int i;
    unsigned int status;
    int timeout;
    int tx_len;

    tx_len = len < 60 ? 60 : len;

    if (len > TX_BUF_SIZE) len = TX_BUF_SIZE;
    for (i = 0; i < len; i++) {
        tx_buf[i] = pkt[i];
    }
    for (i = len; i < tx_len; i++) {
        tx_buf[i] = 0;
    }

    status = inl(io_base + 0x10);
    net_putstr("TSD before=");
    net_putnum(status);

    outl(io_base + 0x20, (unsigned int)(unsigned long)tx_buf);
    outl(io_base + 0x10, (unsigned int)(tx_len & 0xFFF));

    status = inl(io_base + 0x10);
    net_putstr(" after=");
    net_putnum(status);
    net_putstr(" len=");
    net_putnum((unsigned int)len);
    screen_put_char('\n');

    timeout = 5000000;
    while (timeout > 0) {
        status = inl(io_base + 0x10);
        if (status & 0x2000) break;
        if (status & 0x8000) {
            net_putstr("TX abort, TSD=");
            net_putnum(status);
            screen_put_char('\n');
            break;
        }
        timeout--;
    }

    if (timeout == 0) {
        net_putstr("TX timeout, TSD=");
        net_putnum(status);
        screen_put_char('\n');
    }
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

static int arp_resolve(unsigned char* target_ip)
{
    unsigned char pkt[64];
    static unsigned char rx[2048];
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

    timeout = 5000000;
    while (timeout > 0) {
        len = rtl_poll_packet(rx, sizeof(rx));
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
    static unsigned char udp_pkt[1500];
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
    static unsigned char dns_q[512];
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
        static unsigned char rx[2048];
        int len = rtl_poll_packet(rx, sizeof(rx));
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
                                } else {
                                    for (i = 0; i < *(rx + pos); i++) { }
                                    pos += *(rx + pos) + 1;
                                    break;
                                }
                            }
                            while (pos < len - 16) {
                                if (rx[pos] == 0xC0) pos += 2;
                                else { pos += *(rx + pos) + 1; pos += 4; break; }
                                pos += 4;
                            }
                            pos += 6;
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
    static unsigned char pkt[2048];
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
    static unsigned char pseudo[2048];
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

static void tcp_build_and_send(unsigned char* dest_mac, unsigned char* dest_ip,
                                unsigned short flags, unsigned char* data, int data_len)
{
    static unsigned char tcp_seg[2048];
    int i, tcp_len;
    unsigned short cksum;

    if (data_len > 2028) data_len = 2028;
    tcp_len = 20 + data_len;

    tcp_seg[0] = (unsigned char)(tcp_sport >> 8);
    tcp_seg[1] = (unsigned char)(tcp_sport & 0xFF);
    tcp_seg[2] = (unsigned char)(tcp_dport >> 8);
    tcp_seg[3] = (unsigned char)(tcp_dport & 0xFF);

    tcp_seg[4] = (unsigned char)(tcp_seq_local >> 24);
    tcp_seg[5] = (unsigned char)(tcp_seq_local >> 16);
    tcp_seg[6] = (unsigned char)(tcp_seq_local >> 8);
    tcp_seg[7] = (unsigned char)(tcp_seq_local & 0xFF);

    tcp_seg[8]  = (unsigned char)(tcp_seq_remote >> 24);
    tcp_seg[9]  = (unsigned char)(tcp_seq_remote >> 16);
    tcp_seg[10] = (unsigned char)(tcp_seq_remote >> 8);
    tcp_seg[11] = (unsigned char)(tcp_seq_remote & 0xFF);

    tcp_seg[12] = 0x50;
    tcp_seg[13] = flags;
    tcp_seg[14] = 0x20; tcp_seg[15] = 0x00;
    tcp_seg[16] = 0x00; tcp_seg[17] = 0x00;
    tcp_seg[18] = 0x00; tcp_seg[19] = 0x00;

    for (i = 0; i < data_len; i++) tcp_seg[20 + i] = data[i];

    cksum = tcp_compute_checksum(local_ip, dest_ip, tcp_seg, tcp_len);
    tcp_seg[16] = (unsigned char)(cksum >> 8);
    tcp_seg[17] = (unsigned char)(cksum & 0xFF);

    ip_send(dest_mac, dest_ip, IP_PROTO_TCP, tcp_seg, tcp_len);

    if (flags & TCP_FLAG_SYN) tcp_seq_local++;
    if (data_len > 0 && (flags & (TCP_FLAG_SYN | TCP_FLAG_FIN)) == 0) {
        tcp_seq_local += (unsigned int)data_len;
    }
}

static int tcp_connect(void)
{
    int timeout;

    if (!gateway_resolved) {
        net_putstr("ARP first\n");
        return 0;
    }

    tcp_seq_local = 1000;
    tcp_seq_remote = 0;
    tcp_state = TCP_STATE_SYN_SENT;

    tcp_build_and_send(gateway_mac, remote_ip, TCP_FLAG_SYN, (unsigned char*)0, 0);

    timeout = 10000000;
    while (timeout > 0) {
        static unsigned char rx[2048];
        int len = rtl_poll_packet(rx, sizeof(rx));
        if (len > 0) {
            arp_respond(rx, len);

            if (len >= 34 && rx[12] == 0x08 && rx[13] == 0x00 && rx[23] == IP_PROTO_TCP) {
                unsigned short sport, dport;
                sport = ((unsigned short)rx[34] << 8) | rx[35];
                dport = ((unsigned short)rx[36] << 8) | rx[37];

                if (sport == tcp_dport && dport == tcp_sport) {
                    unsigned char flags = rx[47];
                    if (tcp_state == TCP_STATE_SYN_SENT && (flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
                        tcp_seq_remote  = ((unsigned int)rx[38] << 24) | ((unsigned int)rx[39] << 16) | ((unsigned int)rx[40] << 8) | (unsigned int)rx[41];
                        tcp_seq_remote++;
                        tcp_state = TCP_STATE_ESTABLISHED;

                        tcp_build_and_send(gateway_mac, remote_ip, TCP_FLAG_ACK, (unsigned char*)0, 0);
                        return 1;
                    }
                }
            }
        }
        timeout--;
    }

    net_putstr("TCP connect timeout\n");
    return 0;
}

static void tcp_send_data(unsigned char* data, int data_len)
{
    tcp_build_and_send(gateway_mac, remote_ip, TCP_FLAG_PSH | TCP_FLAG_ACK, data, data_len);
}

static int tcp_recv_data(unsigned char* buf, int max_len, int timeout_cycles)
{
    while (timeout_cycles > 0) {
        unsigned char rx[2048];
        int len = rtl_poll_packet(rx, sizeof(rx));
        if (len > 0) {
            arp_respond(rx, len);

            if (len >= 34 && rx[12] == 0x08 && rx[13] == 0x00 && rx[23] == IP_PROTO_TCP) {
                unsigned short sport, dport;
                sport = ((unsigned short)rx[34] << 8) | rx[35];
                dport = ((unsigned short)rx[36] << 8) | rx[37];

                if (sport == net_htons(tcp_dport) && dport == net_htons(tcp_sport)) {
                    unsigned char flags = rx[47];
                    int ip_hdr_len = (rx[14] & 0x0F) * 4;
                    int tcp_data_offset = ((rx[46] >> 4) & 0x0F) * 4;
                    int data_start = 14 + ip_hdr_len + tcp_data_offset;
                    int data_len = len - data_start;
                    unsigned int expected_seq = tcp_seq_remote;

                    if (flags & TCP_FLAG_RST) {
                        tcp_state = TCP_STATE_CLOSED;
                        return -1;
                    }

                    if (flags & TCP_FLAG_FIN) {
                        tcp_seq_remote++;
                        tcp_build_and_send(gateway_mac, remote_ip, TCP_FLAG_ACK, (unsigned char*)0, 0);
                        tcp_state = TCP_STATE_CLOSED;
                        return 0;
                    }

                    if (data_len > 0) {
                        unsigned int rx_seq = ((unsigned int)rx[38] << 24) | ((unsigned int)rx[39] << 16) | ((unsigned int)rx[40] << 8) | (unsigned int)rx[41];

                        if (rx_seq == tcp_seq_remote) {
                            int copy_len = data_len;
                            if (copy_len > max_len) copy_len = max_len;
                            {
                                int i;
                                for (i = 0; i < copy_len; i++) buf[i] = rx[data_start + i];
                            }

                            tcp_seq_remote += (unsigned int)data_len;

                            tcp_build_and_send(gateway_mac, remote_ip, TCP_FLAG_ACK, (unsigned char*)0, 0);
                            return copy_len;
                        }
                    }

                    if (flags & TCP_FLAG_SYN) {
                        tcp_seq_remote = ((unsigned int)rx[38] << 24) | ((unsigned int)rx[39] << 16) | ((unsigned int)rx[40] << 8) | (unsigned int)rx[41];
                        tcp_seq_remote++;
                        tcp_build_and_send(gateway_mac, remote_ip, TCP_FLAG_ACK, (unsigned char*)0, 0);
                    }
                }
            }
        }
        timeout_cycles--;
    }
    return 0;
}

static void tcp_close(void)
{
    if (tcp_state == TCP_STATE_ESTABLISHED) {
        tcp_state = TCP_STATE_FIN_SENT;
        tcp_build_and_send(gateway_mac, remote_ip, TCP_FLAG_FIN | TCP_FLAG_ACK, (unsigned char*)0, 0);
    }
}

int network_init(void)
{
    int i;
    unsigned char gateway_ip[4] = {10, 0, 2, 2};

    net_putstr("Network init...\n");

    if (!rtl_detect()) {
        net_putstr("No RTL8139 found\n");
        return 0;
    }

    rtl_init();

    for (i = 0; i < 6; i++) gateway_mac[i] = 0;

    if (!arp_resolve(gateway_ip)) return 0;

    return 1;
}

int network_http_get(const char* host, const char* path)
{
    static unsigned char rx_buf_data[4096];
    static char request[1024];
    int req_len, i, total = 0;
    int header_done = 0;
    int body_len = 0;
    int in_chunked = 0;
    int chunk_size = 0;
    int line_pos = 0;
    char line_buf[1024];

    if (!io_base) {
        net_putstr("Network not initialized\n");
        return 0;
    }

    if (!gateway_resolved) {
        unsigned char gw_ip[4] = {10, 0, 2, 2};
        if (!arp_resolve(gw_ip)) return 0;
    }

    if (!ip_parse(host, remote_ip)) {
        net_putstr("Resolving ");
        net_putstr(host);
        net_putstr("...\n");
        if (!dns_resolve(host, remote_ip)) {
            net_putstr("DNS failed: ");
            net_putstr(host);
            screen_put_char('\n');
            return 0;
        }
    }

    net_putstr("Connecting to ");
    net_putstr(host);
    net_putstr("...\n");

    tcp_dport = 80;
    if (!tcp_connect()) return 0;

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
            const char* http_ver = " HTTP/1.0\r\nHost: ";
            for (i = 0; http_ver[i] && req_len < 1023; i++) request[req_len++] = http_ver[i];
        }
        for (i = 0; host[i] && req_len < 1023; i++) request[req_len++] = host[i];
        {
            const char* ending = "\r\nConnection: close\r\n\r\n";
            for (i = 0; ending[i] && req_len < 1023; i++) request[req_len++] = ending[i];
        }
    }

    tcp_send_data((unsigned char*)request, req_len);
    net_putstr("Waiting for response...\n\n");

    header_done = 0;
    line_pos = 0;
    body_len = 0;
    in_chunked = 0;
    chunk_size = 0;

    while (1) {
        int len = tcp_recv_data(rx_buf_data, sizeof(rx_buf_data) - 1, 5000000);
        if (len < 0) {
            net_putstr("\nConnection reset\n");
            break;
        }
        if (len == 0) {
            if (tcp_state == TCP_STATE_CLOSED) {
                break;
            }
            net_putstr("\nNo more data\n");
            break;
        }

        total += len;

        for (i = 0; i < len; i++) {
            char c = (char)rx_buf_data[i];
            if (!header_done) {
                if (c == '\n') {
                    if (line_pos > 0 && line_buf[line_pos - 1] == '\r') line_buf[line_pos - 1] = 0;
                    else line_buf[line_pos] = 0;

                    if (line_pos == 0 || (line_pos == 1 && line_buf[0] == '\r')) {
                        header_done = 1;
                        net_putstr("- body -\n");
                        line_pos = 0;
                        continue;
                    }

                    line_pos = 0;
                    continue;
                }
                if (line_pos < 1023) line_buf[line_pos++] = c;
            } else {
                if (in_chunked) {
                    int j;
                    for (j = 0; j < len; j++) {
                        screen_put_char(rx_buf_data[j]);
                    }
                    i = len;
                    break;
                } else {
                    if (c == '\n') {
                        screen_put_char('\n');
                        body_len++;
                    } else {
                        screen_put_char(c);
                    }
                }
            }
        }
    }

    tcp_close();

    screen_put_char('\n');
    net_putstr("Done, ");
    net_putnum(body_len);
    net_putstr(" lines\n");
    return 1;
}
