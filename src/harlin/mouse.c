#include "mouse.h"
#include "io.h"
#include "interrupt.h"

#define PS2_CMD_PORT    0x64
#define PS2_DATA_PORT   0x60

#define MOUSE_IRQ       12

static volatile int mouse_x = 0;
static volatile int mouse_y = 0;
static volatile u8 mouse_buttons = 0;

#define PKT_BUF_SIZE 16
static volatile u8 pkt_buf[PKT_BUF_SIZE][3];
static volatile int pkt_head = 0;
static volatile int pkt_tail = 0;
static volatile int pkt_count = 0;

static int mouse_wait_write(void)
{
    int i;
    for (i = 0; i < 100000; i++) {
        if ((inb(PS2_CMD_PORT) & 0x02) == 0)
            return 0;
    }
    return -1;
}

static int mouse_wait_read(void)
{
    int i;
    for (i = 0; i < 100000; i++) {
        if (inb(PS2_CMD_PORT) & 0x01)
            return 0;
    }
    return -1;
}

static void mouse_write_cmd(u8 cmd)
{
    mouse_wait_write();
    outb(PS2_CMD_PORT, cmd);
}

static void mouse_write_data(u8 data)
{
    mouse_wait_write();
    outb(PS2_CMD_PORT, 0xD4);
    mouse_wait_write();
    outb(PS2_DATA_PORT, data);
}

static u8 mouse_read_data(void)
{
    mouse_wait_read();
    return inb(PS2_DATA_PORT);
}

static void mouse_irq_handler(void)
{
    u8 data = inb(PS2_DATA_PORT);
    static int pkt_state = 0;
    static u8 pkt[3];

    pkt[pkt_state] = data;
    pkt_state++;

    if (pkt_state == 3) {
        pkt_state = 0;
        if (pkt_count < PKT_BUF_SIZE) {
            pkt_buf[pkt_tail][0] = pkt[0];
            pkt_buf[pkt_tail][1] = pkt[1];
            pkt_buf[pkt_tail][2] = pkt[2];
            pkt_tail = (pkt_tail + 1) % PKT_BUF_SIZE;
            pkt_count++;
        }
    }
}

void mouse_init(void)
{
    u8 status;
    int flush_count = 0;

    mouse_x = 0;
    mouse_y = 0;
    mouse_buttons = 0;
    pkt_head = 0;
    pkt_tail = 0;
    pkt_count = 0;

    mouse_write_cmd(0xA8);

    mouse_write_cmd(0x20);
    status = mouse_read_data();
    status |= 0x02;
    mouse_write_cmd(0x60);
    mouse_wait_write();
    outb(PS2_DATA_PORT, status);

    while ((inb(PS2_CMD_PORT) & 0x01) && flush_count < 256) {
        inb(PS2_DATA_PORT);
        flush_count++;
    }

    mouse_write_data(0xF6);
    mouse_read_data();

    mouse_write_data(0xF4);
    mouse_read_data();

    irq_register(MOUSE_IRQ, mouse_irq_handler);
}

int mouse_has_data(void)
{
    return pkt_count > 0;
}

int mouse_read_packet(int* dx, int* dy, u8* buttons)
{
    unsigned long long flags;
    int got = 0;

    asm volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");

    if (pkt_count > 0) {
        u8 b0 = pkt_buf[pkt_head][0];
        int ddx = (int)(s8)pkt_buf[pkt_head][1];
        int ddy = (int)(s8)pkt_buf[pkt_head][2];
        ddy = -ddy;

        mouse_buttons = b0 & 0x07;
        mouse_x += ddx;
        mouse_y += ddy;

        if (dx) *dx = ddx;
        if (dy) *dy = ddy;
        if (buttons) *buttons = mouse_buttons;

        pkt_head = (pkt_head + 1) % PKT_BUF_SIZE;
        pkt_count--;

        if (pkt_count > PKT_BUF_SIZE - 2) {
            pkt_head = 0;
            pkt_tail = 0;
            pkt_count = 0;
        }

        got = 1;
    }

    asm volatile ("pushq %0; popfq" : : "r"(flags) : "memory");
    return got;
}

int mouse_get_x(void) { return mouse_x; }
int mouse_get_y(void) { return mouse_y; }
u8 mouse_get_buttons(void) { return mouse_buttons; }

void mouse_set_position(int x, int y)
{
    mouse_x = x;
    mouse_y = y;
}
