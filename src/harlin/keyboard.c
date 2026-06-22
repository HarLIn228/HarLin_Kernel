#include "io.h"
#include "interrupt.h"
#include "keyboard.h"
#include "screen.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_CMD_PORT    0x64

#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
#define SC_LSHIFT_REL 0xAA
#define SC_RSHIFT_REL 0xB6
#define SC_LCTRL  0x1D
#define SC_LCTRL_REL 0x9D
#define SC_LALT   0x38
#define SC_LALT_REL  0xB8
#define SC_CAPS   0x3A
#define SC_NUM    0x45
#define SC_SCROLL 0x46

#define KEYBUF_SIZE 256

static int shift_count = 0;
static int ctrl_count = 0;
static int alt_count = 0;
static int caps_lock = 0;
static int num_lock = 0;
static int scroll_lock = 0;
static volatile unsigned char keybuf[KEYBUF_SIZE];
static volatile int keybuf_head = 0;
static volatile int keybuf_tail = 0;
static volatile int keybuf_overflow = 0;

static unsigned char scancode_to_ascii[128] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, 0, 0,
    ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static unsigned char scancode_to_ascii_shift[128] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0, 0,
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0, 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, 0, 0,
    ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static unsigned char scancode_to_ascii_caps[128] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0,
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', 0, 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`', 0,
    '\\', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', 0, 0, 0,
    ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void keyboard_wait_input(void)
{
    while (inb(KEYBOARD_STATUS_PORT) & 0x02)
        asm volatile ("pause");
}

static void keyboard_wait_output(void)
{
    while (!(inb(KEYBOARD_STATUS_PORT) & 0x01))
        asm volatile ("pause");
}

static void keyboard_irq_handler(void)
{
    unsigned char scancode = inb(KEYBOARD_DATA_PORT);
    int next = (keybuf_tail + 1) % KEYBUF_SIZE;

    if (scancode == SC_LSHIFT || scancode == SC_RSHIFT) {
        shift_count++;
    } else if (scancode == SC_LSHIFT_REL || scancode == SC_RSHIFT_REL) {
        if (shift_count > 0) shift_count--;
    } else if (scancode == SC_LCTRL) {
        ctrl_count++;
    } else if (scancode == SC_LCTRL_REL) {
        if (ctrl_count > 0) ctrl_count--;
    } else if (scancode == SC_LALT) {
        alt_count++;
    } else if (scancode == SC_LALT_REL) {
        if (alt_count > 0) alt_count--;
    } else if (scancode == SC_CAPS) {
        caps_lock = !caps_lock;
        keyboard_set_leds((scroll_lock ? KEY_LED_SCROLL : 0) |
                          (num_lock ? KEY_LED_NUM : 0) |
                          (caps_lock ? KEY_LED_CAPS : 0));
    } else if (scancode == SC_NUM) {
        num_lock = !num_lock;
        keyboard_set_leds((scroll_lock ? KEY_LED_SCROLL : 0) |
                          (num_lock ? KEY_LED_NUM : 0) |
                          (caps_lock ? KEY_LED_CAPS : 0));
    } else if (scancode == SC_SCROLL) {
        scroll_lock = !scroll_lock;
        keyboard_set_leds((scroll_lock ? KEY_LED_SCROLL : 0) |
                          (num_lock ? KEY_LED_NUM : 0) |
                          (caps_lock ? KEY_LED_CAPS : 0));
    }

    if (next != keybuf_head) {
        keybuf[keybuf_tail] = scancode;
        keybuf_tail = next;
    } else {
        keybuf_overflow++;
    }
}

static int ps2_wait_write(void)
{
    int i;
    for (i = 0; i < 100000; i++) {
        if ((inb(KEYBOARD_STATUS_PORT) & 0x02) == 0)
            return 0;
    }
    return -1;
}

static int ps2_wait_read(void)
{
    int i;
    for (i = 0; i < 100000; i++) {
        if (inb(KEYBOARD_STATUS_PORT) & 0x01)
            return 0;
    }
    return -1;
}

static void ps2_send_cmd(u8 cmd)
{
    ps2_wait_write();
    outb(KEYBOARD_CMD_PORT, cmd);
}

static u8 ps2_read_data(void)
{
    if (ps2_wait_read() != 0)
        return 0;
    return inb(KEYBOARD_DATA_PORT);
}

static int ps2_self_test(void)
{
    ps2_send_cmd(0xAA);
    if (ps2_wait_read() != 0)
        return -1;
    return (ps2_read_data() == 0x55) ? 0 : -1;
}

static int ps2_detect(void)
{
    u8 conf;
    ps2_send_cmd(0x20);
    conf = ps2_read_data();
    if ((conf & 0x01) && (conf & 0x02)) return 2;
    if (conf & 0x01) return 1;
    if (conf & 0x02) return 1;
    return 0;
}

void keyboard_init(void)
{
    int flush_count = 0;
    int ps2_ports = 0;
    int self_test_ok = 0;
    shift_count = 0;
    ctrl_count = 0;
    alt_count = 0;
    caps_lock = 0;
    num_lock = 0;
    scroll_lock = 0;
    keybuf_overflow = 0;
    keybuf_head = 0;
    keybuf_tail = 0;
    ps2_ports = ps2_detect();
    if (ps2_ports > 0) {
        self_test_ok = (ps2_self_test() == 0);
        if (self_test_ok) {
            ps2_send_cmd(0xAE);
        }
    }
    while ((inb(KEYBOARD_STATUS_PORT) & 0x01) && flush_count < 256) {
        inb(KEYBOARD_DATA_PORT);
        flush_count++;
    }
    keyboard_set_leds(0);
    if (ps2_ports > 0 && self_test_ok) {
        irq_register(1, keyboard_irq_handler);
        screen_puts("[ps2] keyboard ready\n");
    } else {
        screen_puts("[ps2] no keyboard\n");
    }
}

int keyboard_has_data(void)
{
    int r;
    unsigned long long flags;
    asm volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    r = keybuf_head != keybuf_tail;
    asm volatile ("pushq %0; popfq" : : "r"(flags) : "memory");
    return r;
}

unsigned char keyboard_poll(void)
{
    unsigned char scancode;
    unsigned long long flags;
    asm volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    if (keybuf_head == keybuf_tail) {
        asm volatile ("pushq %0; popfq" : : "r"(flags) : "memory");
        return 0;
    }
    scancode = keybuf[keybuf_head];
    keybuf_head = (keybuf_head + 1) % KEYBUF_SIZE;
    asm volatile ("pushq %0; popfq" : : "r"(flags) : "memory");
    return scancode;
}

int keyboard_overflow_count(void)
{
    return keybuf_overflow;
}

void keyboard_flush(void)
{
    unsigned long long flags;
    asm volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    keybuf_head = 0;
    keybuf_tail = 0;
    keybuf_overflow = 0;
    asm volatile ("pushq %0; popfq" : : "r"(flags) : "memory");
}

u8 keyboard_get_state(void)
{
    u8 state = 0;
    if (shift_count) state |= KEY_STATE_SHIFT;
    if (ctrl_count)  state |= KEY_STATE_CTRL;
    if (alt_count)   state |= KEY_STATE_ALT;
    if (caps_lock)   state |= KEY_STATE_CAPS;
    if (num_lock)    state |= KEY_STATE_NUM;
    if (scroll_lock) state |= KEY_STATE_SCROLL;
    return state;
}

void keyboard_set_leds(u8 leds)
{
    keyboard_wait_input();
    outb(KEYBOARD_CMD_PORT, 0xED);
    keyboard_wait_input();
    outb(KEYBOARD_DATA_PORT, leds);
}

int keyboard_set_scancode_set(u8 set)
{
    if (set < 1 || set > 3)
        return -1;
    keyboard_wait_input();
    outb(KEYBOARD_DATA_PORT, 0xF0);
    keyboard_wait_input();
    outb(KEYBOARD_DATA_PORT, set);
    keyboard_wait_output();
    if (inb(KEYBOARD_DATA_PORT) != 0xFA)
        return -1;
    return 0;
}

char keyboard_scancode_to_ascii(unsigned char scancode)
{
    int shifted;
    if (scancode == SC_LSHIFT || scancode == SC_RSHIFT) {
        return 0;
    }
    if (scancode == SC_LSHIFT_REL || scancode == SC_RSHIFT_REL) {
        return 0;
    }
    if (scancode == SC_LCTRL || scancode == SC_LCTRL_REL ||
        scancode == SC_LALT || scancode == SC_LALT_REL) {
        return 0;
    }
    if (scancode >= 128) return 0;
    shifted = (shift_count > 0) ^ (caps_lock != 0);
    if (shifted)
        return scancode_to_ascii_shift[scancode];
    return scancode_to_ascii[scancode];
}
