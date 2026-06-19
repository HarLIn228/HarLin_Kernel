#include "io.h"
#include "interrupt.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
#define SC_LSHIFT_REL 0xAA
#define SC_RSHIFT_REL 0xB6

#define KEYBUF_SIZE 256

static int shift_pressed = 0;
static volatile unsigned char keybuf[KEYBUF_SIZE];
static volatile int keybuf_head = 0;
static volatile int keybuf_tail = 0;

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

static void keyboard_irq_handler(void)
{
    unsigned char scancode = inb(KEYBOARD_DATA_PORT);
    int next = (keybuf_tail + 1) % KEYBUF_SIZE;
    if (next != keybuf_head) {
        keybuf[keybuf_tail] = scancode;
        keybuf_tail = next;
    }
}

void keyboard_init(void)
{
    shift_pressed = 0;
    keybuf_head = 0;
    keybuf_tail = 0;
    while (inb(KEYBOARD_STATUS_PORT) & 0x01) {
        inb(KEYBOARD_DATA_PORT);
    }
    irq_register(1, keyboard_irq_handler);
}

int keyboard_has_data(void)
{
    int r;
    asm volatile ("cli");
    r = keybuf_head != keybuf_tail;
    asm volatile ("sti");
    return r;
}

unsigned char keyboard_poll(void)
{
    unsigned char scancode;
    asm volatile ("cli");
    if (keybuf_head == keybuf_tail) {
        asm volatile ("sti");
        return 0;
    }
    scancode = keybuf[keybuf_head];
    keybuf_head = (keybuf_head + 1) % KEYBUF_SIZE;
    asm volatile ("sti");
    return scancode;
}

char keyboard_scancode_to_ascii(unsigned char scancode)
{
    if (scancode == SC_LSHIFT || scancode == SC_RSHIFT) {
        shift_pressed = 1;
        return 0;
    }
    if (scancode == SC_LSHIFT_REL || scancode == SC_RSHIFT_REL) {
        shift_pressed = 0;
        return 0;
    }
    if (scancode >= 128) return 0;
    if (shift_pressed) {
        return scancode_to_ascii_shift[scancode];
    }
    return scancode_to_ascii[scancode];
}
