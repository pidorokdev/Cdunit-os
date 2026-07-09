/*
 * Dunit OS - Virtio MMIO Mouse/Tablet Driver
 *
 * Based on DunitOS implementation for QEMU virt machine.
 * Uses virtio-tablet for absolute positioning (EV_ABS events).
 */

#include "printk.h"
#include "types.h"
#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH 1024
#endif
#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT 768
#endif

#if defined(ARCH_X86_64) || defined(ARCH_X86)
#include "arch/arch.h"
#endif

/* ===================================================================== */
/* Virtio MMIO registers (QEMU virt machine) */
/* ===================================================================== */

#define VIRTIO_MMIO_BASE 0x0a000000
#define VIRTIO_MMIO_STRIDE 0x200

#define VIRTIO_MMIO_MAGIC 0x000
#define VIRTIO_MMIO_VERSION 0x004
#define VIRTIO_MMIO_DEVICE_ID 0x008
#define VIRTIO_MMIO_VENDOR_ID 0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_QUEUE_SEL 0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034
#define VIRTIO_MMIO_QUEUE_NUM 0x038
#define VIRTIO_MMIO_QUEUE_READY 0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY 0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060
#define VIRTIO_MMIO_INTERRUPT_ACK 0x064
#define VIRTIO_MMIO_STATUS 0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW 0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH 0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW 0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH 0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW 0x0a0
#define VIRTIO_MMIO_QUEUE_USED_HIGH 0x0a4

#define VIRTIO_STATUS_ACK 1
#define VIRTIO_STATUS_DRIVER 2
#define VIRTIO_STATUS_DRIVER_OK 4
#define VIRTIO_STATUS_FEATURES_OK 8

#define VIRTIO_DEV_INPUT 18

/* Linux input event types */
#define EV_SYN 0x00
#define EV_KEY 0x01
#define EV_REL 0x02
#define EV_ABS 0x03

/* Absolute axis codes */
#define ABS_X 0x00
#define ABS_Y 0x01

/* Relative axis codes */
#define REL_WHEEL 0x08

/* Button codes */
#define BTN_LEFT 0x110
#define BTN_RIGHT 0x111
#define BTN_MIDDLE 0x112

/* Virtio input config */
#define VIRTIO_INPUT_CFG_SELECT 0x100
#define VIRTIO_INPUT_CFG_SUBSEL 0x101
#define VIRTIO_INPUT_CFG_SIZE 0x102
#define VIRTIO_INPUT_CFG_DATA 0x108
#define VIRTIO_INPUT_CFG_ID_NAME 0x01

/* Virtqueue structures */
typedef struct __attribute__((packed)) {
  uint64_t addr;
  uint32_t len;
  uint16_t flags;
  uint16_t next;
} virtq_desc_t;

typedef struct __attribute__((packed)) {
  uint16_t flags;
  uint16_t idx;
  uint16_t ring[16];
} virtq_avail_t;

typedef struct __attribute__((packed)) {
  uint32_t id;
  uint32_t len;
} virtq_used_elem_t;

typedef struct __attribute__((packed)) {
  uint16_t flags;
  uint16_t idx;
  virtq_used_elem_t ring[16];
} virtq_used_t;

/* Input event structure */
typedef struct __attribute__((packed)) {
  uint16_t type;
  uint16_t code;
  uint32_t value;
} virtio_input_event_t;

#define QUEUE_SIZE 16
#define DESC_F_WRITE 2

/* ===================================================================== */
/* State */
/* ===================================================================== */

static volatile uint32_t *mouse_base = 0;
static virtq_desc_t *desc = 0;
static virtq_avail_t *avail = 0;
static virtq_used_t *used = 0;
static virtio_input_event_t *events = 0;
static uint16_t last_used_idx = 0;

/* Queue memory - must be 4K aligned */
static uint8_t queue_mem[4096] __attribute__((aligned(4096)));
static virtio_input_event_t event_bufs[QUEUE_SIZE] __attribute__((aligned(16)));

/* Mouse state */
static int mouse_x = 16384; /* Raw 0-32767 */
static int mouse_y = 16384;
static uint8_t mouse_buttons = 0;
static int mouse_scroll_delta = 0;

/* Keyboard state */
static volatile uint32_t *kbd_base = 0;
static virtq_desc_t *kbd_desc = 0;
static virtq_avail_t *kbd_avail = 0;
static virtq_used_t *kbd_used = 0;
static virtio_input_event_t *kbd_events = 0;
static uint16_t kbd_last_used_idx = 0;
static uint8_t kbd_queue_mem[4096] __attribute__((aligned(4096)));
static virtio_input_event_t kbd_event_bufs[QUEUE_SIZE]
    __attribute__((aligned(16)));

/* Keyboard callback */
static void (*gui_key_callback)(int key) = 0;
static void (*key_callback)(int key) = 0;

/* Modifier key states */
static int shift_held = 0;
static int ctrl_held = 0;

#if defined(ARCH_X86_64) || defined(ARCH_X86)
#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64
#define PS2_STATUS_OUTPUT_FULL 0x01
#define PS2_STATUS_INPUT_FULL 0x02
#define PS2_STATUS_AUX_DATA 0x20

static int ps2_enabled = 0;
static int ps2_shift_held = 0;
static int ps2_extended = 0;
static int ps2_mouse_x = SCREEN_WIDTH / 2;
static int ps2_mouse_y = SCREEN_HEIGHT / 2;
static int ps2_mouse_buttons = 0;
static int ps2_mouse_scroll_delta = 0;
static int ps2_mouse_has_wheel = 0;
static uint8_t ps2_mouse_packet[4];
static int ps2_mouse_packet_pos = 0;

static const char ps2_scancode_ascii[128] = {
    [0x01] = 27,   [0x02] = '1',  [0x03] = '2',  [0x04] = '3',
    [0x05] = '4',  [0x06] = '5',  [0x07] = '6',  [0x08] = '7',
    [0x09] = '8',  [0x0A] = '9',  [0x0B] = '0',  [0x0C] = '-',
    [0x0D] = '=',  [0x0E] = '\b', [0x0F] = '\t', [0x10] = 'q',
    [0x11] = 'w',  [0x12] = 'e',  [0x13] = 'r',  [0x14] = 't',
    [0x15] = 'y',  [0x16] = 'u',  [0x17] = 'i',  [0x18] = 'o',
    [0x19] = 'p',  [0x1A] = '[',  [0x1B] = ']',  [0x1C] = '\n',
    [0x1E] = 'a',  [0x1F] = 's',  [0x20] = 'd',  [0x21] = 'f',
    [0x22] = 'g',  [0x23] = 'h',  [0x24] = 'j',  [0x25] = 'k',
    [0x26] = 'l',  [0x27] = ';',  [0x28] = '\'', [0x29] = '`',
    [0x2B] = '\\', [0x2C] = 'z',  [0x2D] = 'x',  [0x2E] = 'c',
    [0x2F] = 'v',  [0x30] = 'b',  [0x31] = 'n',  [0x32] = 'm',
    [0x33] = ',',  [0x34] = '.',  [0x35] = '/',  [0x39] = ' ',
};

static const char ps2_scancode_ascii_shifted[128] = {
    [0x01] = 27,   [0x02] = '!',  [0x03] = '@',  [0x04] = '#',
    [0x05] = '$',  [0x06] = '%',  [0x07] = '^',  [0x08] = '&',
    [0x09] = '*',  [0x0A] = '(',  [0x0B] = ')',  [0x0C] = '_',
    [0x0D] = '+',  [0x0E] = '\b', [0x0F] = '\t', [0x10] = 'Q',
    [0x11] = 'W',  [0x12] = 'E',  [0x13] = 'R',  [0x14] = 'T',
    [0x15] = 'Y',  [0x16] = 'U',  [0x17] = 'I',  [0x18] = 'O',
    [0x19] = 'P',  [0x1A] = '{',  [0x1B] = '}',  [0x1C] = '\n',
    [0x1E] = 'A',  [0x1F] = 'S',  [0x20] = 'D',  [0x21] = 'F',
    [0x22] = 'G',  [0x23] = 'H',  [0x24] = 'J',  [0x25] = 'K',
    [0x26] = 'L',  [0x27] = ':',  [0x28] = '"',  [0x29] = '~',
    [0x2B] = '|',  [0x2C] = 'Z',  [0x2D] = 'X',  [0x2E] = 'C',
    [0x2F] = 'V',  [0x30] = 'B',  [0x31] = 'N',  [0x32] = 'M',
    [0x33] = '<',  [0x34] = '>',  [0x35] = '?',  [0x39] = ' ',
};

static int ps2_wait_input_clear(void) {
  for (int i = 0; i < 100000; i++) {
    if (!(inb(PS2_STATUS) & PS2_STATUS_INPUT_FULL))
      return 0;
  }
  return -1;
}

static int ps2_wait_output_full(void) {
  for (int i = 0; i < 100000; i++) {
    if (inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL)
      return 0;
  }
  return -1;
}

static void ps2_write_cmd(uint8_t cmd) {
  if (ps2_wait_input_clear() == 0)
    outb(PS2_COMMAND, cmd);
}

static void ps2_write_data(uint8_t data) {
  if (ps2_wait_input_clear() == 0)
    outb(PS2_DATA, data);
}

static void ps2_write_mouse(uint8_t data) {
  ps2_write_cmd(0xD4);
  ps2_write_data(data);
  if (ps2_wait_output_full() == 0)
    (void)inb(PS2_DATA);
}

static int ps2_mouse_get_id(void) {
  ps2_write_cmd(0xD4);
  ps2_write_data(0xF2);
  if (ps2_wait_output_full() == 0)
    (void)inb(PS2_DATA); /* ACK */
  if (ps2_wait_output_full() == 0)
    return inb(PS2_DATA);
  return -1;
}

static void ps2_send_key(int key) {
  if (key_callback)
    key_callback(key);
  if (gui_key_callback)
    gui_key_callback(key);
}

static void ps2_handle_keyboard(uint8_t scancode) {
  if (scancode == 0xE0) {
    ps2_extended = 1;
    return;
  }

  int released = scancode & 0x80;
  scancode &= 0x7F;

  if (scancode == 0x2A || scancode == 0x36) {
    ps2_shift_held = !released;
    ps2_extended = 0;
    return;
  }

  if (released) {
    ps2_extended = 0;
    return;
  }

  if (ps2_extended) {
    if (scancode == 0x48)
      ps2_send_key(0x100);
    else if (scancode == 0x50)
      ps2_send_key(0x101);
    else if (scancode == 0x4B)
      ps2_send_key(0x102);
    else if (scancode == 0x4D)
      ps2_send_key(0x103);
    ps2_extended = 0;
    return;
  }

  char ascii = ps2_shift_held ? ps2_scancode_ascii_shifted[scancode]
                              : ps2_scancode_ascii[scancode];
  if (ascii)
    ps2_send_key(ascii);
}

static void ps2_handle_mouse(uint8_t data) {
  if (ps2_mouse_packet_pos == 0 && !(data & 0x08))
    return;

  ps2_mouse_packet[ps2_mouse_packet_pos++] = data;
  int packet_size = ps2_mouse_has_wheel ? 4 : 3;
  if (ps2_mouse_packet_pos < packet_size)
    return;

  ps2_mouse_packet_pos = 0;

  int dx = (int8_t)ps2_mouse_packet[1];
  int dy = (int8_t)ps2_mouse_packet[2];

  ps2_mouse_x += dx;
  ps2_mouse_y -= dy;

  if (ps2_mouse_x < 0)
    ps2_mouse_x = 0;
  if (ps2_mouse_y < 0)
    ps2_mouse_y = 0;
  if (ps2_mouse_x >= SCREEN_WIDTH)
    ps2_mouse_x = SCREEN_WIDTH - 1;
  if (ps2_mouse_y >= SCREEN_HEIGHT)
    ps2_mouse_y = SCREEN_HEIGHT - 1;

  ps2_mouse_buttons = ps2_mouse_packet[0] & 0x07;

  if (ps2_mouse_has_wheel) {
    int wheel = ps2_mouse_packet[3] & 0x0F;
    if (wheel & 0x08)
      wheel |= ~0x0F;
    ps2_mouse_scroll_delta -= wheel;
  }
}

static void ps2_poll(void) {
  if (!ps2_enabled)
    return;

  for (int i = 0; i < 64; i++) {
    uint8_t status = inb(PS2_STATUS);
    if (!(status & PS2_STATUS_OUTPUT_FULL))
      break;

    uint8_t data = inb(PS2_DATA);
    if (status & PS2_STATUS_AUX_DATA)
      ps2_handle_mouse(data);
    else
      ps2_handle_keyboard(data);
  }
}

static void ps2_init(void) {
  printk(KERN_INFO "PS2: Initializing keyboard/mouse fallback\n");

  while (inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL)
    (void)inb(PS2_DATA);

  ps2_write_cmd(0xA8); /* Enable auxiliary mouse port */

  ps2_write_cmd(0x20);
  uint8_t config = 0;
  if (ps2_wait_output_full() == 0)
    config = inb(PS2_DATA);
  config |= 0x03;  /* Enable keyboard and mouse IRQ delivery internally */
  config &= ~0x30; /* Enable keyboard and mouse clocks */
  ps2_write_cmd(0x60);
  ps2_write_data(config);

  ps2_write_mouse(0xF6); /* Defaults */
  ps2_write_mouse(0xF3); /* IntelliMouse wheel probe: sample rate 200 */
  ps2_write_mouse(200);
  ps2_write_mouse(0xF3); /* sample rate 100 */
  ps2_write_mouse(100);
  ps2_write_mouse(0xF3); /* sample rate 80 */
  ps2_write_mouse(80);

  int mouse_id = ps2_mouse_get_id();
  ps2_mouse_has_wheel = (mouse_id == 3 || mouse_id == 4);
  printk(KERN_INFO "PS2: Mouse ID %d%s\n", mouse_id,
         ps2_mouse_has_wheel ? " (wheel)" : "");

  ps2_write_mouse(0xF4); /* Enable streaming */

  ps2_enabled = 1;
  printk(KERN_INFO "PS2: Fallback input ready\n");
}
#endif

/* Linux keycode to ASCII mapping (not PS/2 scancodes!) */
/* virtio-keyboard sends Linux KEY_* codes, not PS/2 scancodes */
static const char keycode_to_ascii[128] = {
    0,    27,  '1',  '2',
    '3',  '4', '5',  '6', /* 0-7 */
    '7',  '8', '9',  '0',
    '-',  '=', '\b', '\t', /* 8-15: KEY_7..KEY_TAB */
    'q',  'w', 'e',  'r',
    't',  'y', 'u',  'i', /* 16-23: KEY_Q..KEY_I */
    'o',  'p', '[',  ']',
    '\n', 0,   'a',  's', /* 24-31: KEY_O..KEY_S */
    'd',  'f', 'g',  'h',
    'j',  'k', 'l',  ';', /* 32-39: KEY_D..KEY_SEMICOLON */
    '\'', '`', 0,    '\\',
    'z',  'x', 'c',  'v', /* 40-47: KEY_APOSTROPHE..KEY_V */
    'b',  'n', 'm',  ',',
    '.',  '/', 0,    '*', /* 48-55: KEY_B..KEY_KPASTERISK */
    0,    ' ', 0,    0,
    0,    0,   0,    0, /* 56-63: KEY_LEFTALT..KEY_F5 */
    0,    0,   0,    0,
    0,    0,   0,    '7', /* 64-71: KEY_F6..KEY_KP7 */
    '8',  '9', '-',  '4',
    '5',  '6', '+',  '1', /* 72-79: KEY_KP8..KEY_KP1 */
    '2',  '3', '0',  '.',
    0,    0,   0,    0, /* 80-87: KEY_KP2..KEY_F12 */
    0,    0,   0,    0,
    0,    0,   0,    0, /* 88-95 */
    0,    0,   0,    0,
    0,    0,   0,    0, /* 96-103 */
    0,    0,   0,    0,
    0,    0,   0,    0, /* 104-111 */
    0,    0,   0,    0,
    0,    0,   0,    0, /* 112-119 */
    0,    0,   0,    0,
    0,    0,   0,    0 /* 120-127 */
};

/* Shifted keycode to ASCII mapping for symbols */
static const char keycode_to_ascii_shifted[128] = {
    0,    27,  '!',  '@',
    '#',  '$', '%',  '^', /* 0-7: shift+1 = !, shift+2 = @, etc */
    '&',  '*', '(',  ')',
    '_',  '+', '\b', '\t', /* 8-15: shift+- = _, shift+= = + */
    'Q',  'W', 'E',  'R',
    'T',  'Y', 'U',  'I', /* 16-23: uppercase letters */
    'O',  'P', '{',  '}',
    '\n', 0,   'A',  'S', /* 24-31: shift+[ = {, shift+] = } */
    'D',  'F', 'G',  'H',
    'J',  'K', 'L',  ':', /* 32-39: shift+; = : */
    '"',  '~', 0,    '|',
    'Z',  'X', 'C',  'V', /* 40-47: shift+' = ", shift+` = ~, shift+\ = | */
    'B',  'N', 'M',  '<',
    '>',  '?', 0,    '*', /* 48-55: shift+, = <, shift+. = >, shift+/ = ? */
    0,    ' ', 0,    0,
    0,    0,   0,    0, /* 56-63 */
    0,    0,   0,    0,
    0,    0,   0,    '7', /* 64-71 */
    '8',  '9', '-',  '4',
    '5',  '6', '+',  '1', /* 72-79 */
    '2',  '3', '0',  '.',
    0,    0,   0,    0, /* 80-87 */
    0,    0,   0,    0,
    0,    0,   0,    0, /* 88-95 */
    0,    0,   0,    0,
    0,    0,   0,    0, /* 96-103 */
    0,    0,   0,    0,
    0,    0,   0,    0, /* 104-111 */
    0,    0,   0,    0,
    0,    0,   0,    0, /* 112-119 */
    0,    0,   0,    0,
    0,    0,   0,    0 /* 120-127 */
};

/* Screen dimensions */

/* ===================================================================== */
/* MMIO Helpers */
/* ===================================================================== */

static void mmio_barrier(void) {
#ifdef ARCH_ARM64
  asm volatile("dsb sy" ::: "memory");
#elif defined(ARCH_X86_64) || defined(ARCH_X86)
  asm volatile("mfence" ::: "memory");
#endif
}

static uint32_t mmio_read32(volatile uint32_t *addr) {
  uint32_t val = *addr;
  mmio_barrier();
  return val;
}

static void mmio_write32(volatile uint32_t *addr, uint32_t val) {
  mmio_barrier();
  *addr = val;
  mmio_barrier();
}

/* ===================================================================== */
/* Find Virtio Tablet Device */
/* ===================================================================== */

static volatile uint32_t *find_virtio_tablet(void) {
  for (int i = 0; i < 32; i++) {
    volatile uint32_t *base =
        (volatile uint32_t *)(uintptr_t)(VIRTIO_MMIO_BASE +
                                         i * VIRTIO_MMIO_STRIDE);
    volatile uint8_t *base8 =
        (volatile uint8_t *)(uintptr_t)(VIRTIO_MMIO_BASE +
                                        i * VIRTIO_MMIO_STRIDE);

    uint32_t magic = mmio_read32(base + VIRTIO_MMIO_MAGIC / 4);
    uint32_t device_id = mmio_read32(base + VIRTIO_MMIO_DEVICE_ID / 4);

    if (magic != 0x74726976 || device_id != VIRTIO_DEV_INPUT) {
      continue;
    }

    /* Check device name for "Tablet" */
    base8[VIRTIO_INPUT_CFG_SELECT] = VIRTIO_INPUT_CFG_ID_NAME;
    base8[VIRTIO_INPUT_CFG_SUBSEL] = 0;
    mmio_barrier();

    uint8_t size = base8[VIRTIO_INPUT_CFG_SIZE];
    char name[32] = {0};
    for (int j = 0; j < 31 && j < size; j++) {
      name[j] = base8[VIRTIO_INPUT_CFG_DATA + j];
    }

    printk(KERN_INFO "MOUSE: Found input device: %s\n", name);

    /* Look for "Tablet" */
    if (name[0] == 'Q' && name[5] == 'V' && name[12] == 'T') {
      return base;
    }
  }

  return 0;
}

/* ===================================================================== */
/* Mouse Polling */
/* ===================================================================== */

void mouse_poll(void) {
  if (!mouse_base || !used) {
    return;
  }

  mmio_barrier();
  uint16_t current_used = used->idx;

  while (last_used_idx != current_used) {
    uint16_t idx = last_used_idx % QUEUE_SIZE;
    uint32_t desc_idx = used->ring[idx].id;

    virtio_input_event_t *ev = &events[desc_idx];

    /* Process event */
    if (ev->type == EV_ABS) {
      if (ev->code == ABS_X) {
        mouse_x = ev->value;
      } else if (ev->code == ABS_Y) {
        mouse_y = ev->value;
      }
    } else if (ev->type == EV_REL) {
      if (ev->code == REL_WHEEL)
        mouse_scroll_delta += (int32_t)ev->value;
    } else if (ev->type == EV_KEY) {
      int pressed = (ev->value != 0);
      if (ev->code == BTN_LEFT) {
        if (pressed)
          mouse_buttons |= 1;
        else
          mouse_buttons &= ~1;
      } else if (ev->code == BTN_RIGHT) {
        if (pressed)
          mouse_buttons |= 2;
        else
          mouse_buttons &= ~2;
      }
    }

    /* Re-add descriptor to available ring */
    uint16_t avail_idx = avail->idx % QUEUE_SIZE;
    avail->ring[avail_idx] = desc_idx;
    avail->idx++;

    last_used_idx++;
  }

  /* Notify device */
  mmio_write32(mouse_base + VIRTIO_MMIO_QUEUE_NOTIFY / 4, 0);
  mmio_write32(mouse_base + VIRTIO_MMIO_INTERRUPT_ACK / 4,
               mmio_read32(mouse_base + VIRTIO_MMIO_INTERRUPT_STATUS / 4));
}

/* ===================================================================== */
/* Mouse API */
/* ===================================================================== */

void mouse_get_position(int *x, int *y) {
#if defined(ARCH_X86_64) || defined(ARCH_X86)
  ps2_poll();
  if (ps2_enabled && !mouse_base) {
    if (x)
      *x = ps2_mouse_x;
    if (y)
      *y = ps2_mouse_y;
    return;
  }
#endif

  mouse_poll();

  /* Scale from 0-32767 to screen dimensions */
  if (x)
    *x = (mouse_x * SCREEN_WIDTH) / 32768;
  if (y)
    *y = (mouse_y * SCREEN_HEIGHT) / 32768;
}

int mouse_get_buttons(void) {
#if defined(ARCH_X86_64) || defined(ARCH_X86)
  ps2_poll();
  if (ps2_enabled && !mouse_base)
    return ps2_mouse_buttons;
#endif

  mouse_poll();
  return mouse_buttons;
}

int mouse_get_scroll_delta(void) {
  int delta;
#if defined(ARCH_X86_64) || defined(ARCH_X86)
  ps2_poll();
  if (ps2_enabled && !mouse_base) {
    delta = ps2_mouse_scroll_delta;
    ps2_mouse_scroll_delta = 0;
    return delta;
  }
#endif

  mouse_poll();
  delta = mouse_scroll_delta;
  mouse_scroll_delta = 0;
  return delta;
}

/* ===================================================================== */
/* Initialization */
/* ===================================================================== */

int mouse_init(void) {
  printk(KERN_INFO "MOUSE: Initializing virtio-tablet...\n");

  mouse_base = find_virtio_tablet();
  if (!mouse_base) {
    printk(KERN_WARNING "MOUSE: No virtio tablet found\n");
    return -1;
  }

  /* Reset device */
  mmio_write32(mouse_base + VIRTIO_MMIO_STATUS / 4, 0);
  while (mmio_read32(mouse_base + VIRTIO_MMIO_STATUS / 4) != 0) {
    asm volatile("nop");
  }

  /* Acknowledge */
  mmio_write32(mouse_base + VIRTIO_MMIO_STATUS / 4, VIRTIO_STATUS_ACK);
  mmio_write32(mouse_base + VIRTIO_MMIO_STATUS / 4,
               VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);

  /* Accept no special features */
  mmio_write32(mouse_base + VIRTIO_MMIO_DRIVER_FEATURES / 4, 0);
  mmio_write32(mouse_base + VIRTIO_MMIO_STATUS / 4,
               VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER |
                   VIRTIO_STATUS_FEATURES_OK);

  /* Setup queue 0 */
  mmio_write32(mouse_base + VIRTIO_MMIO_QUEUE_SEL / 4, 0);

  uint32_t max_queue = mmio_read32(mouse_base + VIRTIO_MMIO_QUEUE_NUM_MAX / 4);
  if (max_queue < QUEUE_SIZE) {
    printk(KERN_WARNING "MOUSE: Queue too small\n");
    return -1;
  }

  mmio_write32(mouse_base + VIRTIO_MMIO_QUEUE_NUM / 4, QUEUE_SIZE);

  /* Setup queue memory */
  desc = (virtq_desc_t *)queue_mem;
  avail = (virtq_avail_t *)(queue_mem + QUEUE_SIZE * sizeof(virtq_desc_t));
  used = (virtq_used_t *)(queue_mem + 2048);
  events = event_bufs;

  /* Set queue addresses */
  uint64_t desc_addr = (uint64_t)(uintptr_t)desc;
  uint64_t avail_addr = (uint64_t)(uintptr_t)avail;
  uint64_t used_addr = (uint64_t)(uintptr_t)used;

  mmio_write32(mouse_base + VIRTIO_MMIO_QUEUE_DESC_LOW / 4,
               (uint32_t)desc_addr);
  mmio_write32(mouse_base + VIRTIO_MMIO_QUEUE_DESC_HIGH / 4,
               (uint32_t)(desc_addr >> 32));
  mmio_write32(mouse_base + VIRTIO_MMIO_QUEUE_AVAIL_LOW / 4,
               (uint32_t)avail_addr);
  mmio_write32(mouse_base + VIRTIO_MMIO_QUEUE_AVAIL_HIGH / 4,
               (uint32_t)(avail_addr >> 32));
  mmio_write32(mouse_base + VIRTIO_MMIO_QUEUE_USED_LOW / 4,
               (uint32_t)used_addr);
  mmio_write32(mouse_base + VIRTIO_MMIO_QUEUE_USED_HIGH / 4,
               (uint32_t)(used_addr >> 32));

  /* Initialize descriptors */
  for (int i = 0; i < QUEUE_SIZE; i++) {
    desc[i].addr = (uint64_t)(uintptr_t)&events[i];
    desc[i].len = sizeof(virtio_input_event_t);
    desc[i].flags = DESC_F_WRITE;
    desc[i].next = 0;
  }

  /* Fill available ring */
  avail->flags = 0;
  for (int i = 0; i < QUEUE_SIZE; i++) {
    avail->ring[i] = i;
  }
  avail->idx = QUEUE_SIZE;

  /* Queue ready */
  mmio_write32(mouse_base + VIRTIO_MMIO_QUEUE_READY / 4, 1);

  /* Driver OK */
  mmio_write32(mouse_base + VIRTIO_MMIO_STATUS / 4,
               VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER |
                   VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);

  /* Notify device */
  mmio_write32(mouse_base + VIRTIO_MMIO_QUEUE_NOTIFY / 4, 0);

  /* Check status */
  uint32_t status = mmio_read32(mouse_base + VIRTIO_MMIO_STATUS / 4);
  if (status & 0x40) {
    printk(KERN_WARNING "MOUSE: Device reported failure!\n");
    return -1;
  }

  printk(KERN_INFO "MOUSE: Virtio tablet initialized!\n");
  return 0;
}

/* ===================================================================== */
/* Keyboard Functions */
/* ===================================================================== */

static volatile uint32_t *find_virtio_keyboard(void) {
  for (int i = 0; i < 32; i++) {
    volatile uint32_t *base =
        (volatile uint32_t *)(uintptr_t)(VIRTIO_MMIO_BASE +
                                         i * VIRTIO_MMIO_STRIDE);
    volatile uint8_t *base8 =
        (volatile uint8_t *)(uintptr_t)(VIRTIO_MMIO_BASE +
                                        i * VIRTIO_MMIO_STRIDE);

    uint32_t magic = mmio_read32(base + VIRTIO_MMIO_MAGIC / 4);
    uint32_t device_id = mmio_read32(base + VIRTIO_MMIO_DEVICE_ID / 4);

    if (magic != 0x74726976 || device_id != VIRTIO_DEV_INPUT) {
      continue;
    }

    /* Check device name for "Keyboard" */
    base8[VIRTIO_INPUT_CFG_SELECT] = VIRTIO_INPUT_CFG_ID_NAME;
    base8[VIRTIO_INPUT_CFG_SUBSEL] = 0;
    mmio_barrier();

    uint8_t size = base8[VIRTIO_INPUT_CFG_SIZE];
    char name[32] = {0};
    for (int j = 0; j < 31 && j < size; j++) {
      name[j] = base8[VIRTIO_INPUT_CFG_DATA + j];
    }

    printk(KERN_INFO "KEYBOARD: Checking device: %s\n", name);

    /* Look for "Keyboard" or "keyboard" anywhere in name */
    int found_kbd = 0;
    for (int j = 0; name[j] && name[j + 7]; j++) {
      if ((name[j] == 'K' || name[j] == 'k') &&
          (name[j + 1] == 'e' || name[j + 1] == 'E') &&
          (name[j + 2] == 'y' || name[j + 2] == 'Y')) {
        found_kbd = 1;
        break;
      }
    }

    if (found_kbd) {
      printk(KERN_INFO "KEYBOARD: Found: %s\n", name);
      return base;
    }
  }

  return 0;
}

static void keyboard_poll(void) {
  if (!kbd_base || !kbd_used) {
    return;
  }

  mmio_barrier();
  uint16_t current_used = kbd_used->idx;

  while (kbd_last_used_idx != current_used) {
    uint16_t idx = kbd_last_used_idx % QUEUE_SIZE;
    uint32_t desc_idx = kbd_used->ring[idx].id;

    virtio_input_event_t *ev = &kbd_events[desc_idx];

    /* Process keyboard event */
    if (ev->type == EV_KEY) {
      /* Track shift key state */
      if (ev->code == 42 || ev->code == 54) { /* Left or Right Shift */
        shift_held = (ev->value != 0);        /* 1 = pressed, 0 = released */
      }

      /* Track Ctrl key state */
      if (ev->code == 29 || ev->code == 97) { /* Left or Right Ctrl */
        ctrl_held = (ev->value != 0);         /* 1 = pressed, 0 = released */
      }

      if (ev->value == 1) { /* Key press only */
        int processed = 0;
        int dunit_key = 0;

        /* Manual mapping for Special Keys */
        if (ev->code == 103)
          dunit_key = 0x100; /* KEY_UP */
        else if (ev->code == 108)
          dunit_key = 0x101; /* KEY_DOWN */
        else if (ev->code == 105)
          dunit_key = 0x102; /* KEY_LEFT */
        else if (ev->code == 106)
          dunit_key = 0x103; /* KEY_RIGHT */
        else if (ev->code == 29 || ev->code == 97)
          processed = 1; /* Don't send ctrl as a key, just track state */
        else if (ev->code == 42 || ev->code == 54)
          processed = 1; /* Don't send shift as a key, just track state */
        else if (ev->code == 28)
          dunit_key = '\n'; /* Enter */
        else if (ev->code == 57)
          dunit_key = ' '; /* Space */
        else if (ev->code == 1)
          dunit_key = 27; /* Esc */

        if (dunit_key) {
          if (key_callback)
            key_callback(dunit_key);
          if (gui_key_callback)
            gui_key_callback(dunit_key);
          processed = 1;
        }

        if (!processed && ev->code < 128) {
          char ascii;

          /* Handle Ctrl+key combinations */
          if (ctrl_held) {
            /* Ctrl+letter generates control character (1-26) */
            char base = keycode_to_ascii[ev->code];
            if (base >= 'a' && base <= 'z') {
              ascii = base - 'a' + 1; /* Ctrl+a=1, Ctrl+c=3, Ctrl+v=22, etc */
            } else if (base >= 'A' && base <= 'Z') {
              ascii = base - 'A' + 1;
            } else {
              ascii = 0; /* Don't process other Ctrl combinations */
            }
          } else if (shift_held) {
            ascii = keycode_to_ascii_shifted[ev->code];
          } else {
            ascii = keycode_to_ascii[ev->code];
          }

          /* Send ASCII to both callbacks */
          if (key_callback && ascii) {
            key_callback(ascii);
          }
          /* Send ASCII to GUI callback too (not raw keycode!) */
          if (gui_key_callback && ascii) {
            gui_key_callback(ascii);
          }
        }
      }
    }

    /* Re-add descriptor to available ring */
    uint16_t avail_idx = kbd_avail->idx % QUEUE_SIZE;
    kbd_avail->ring[avail_idx] = desc_idx;
    kbd_avail->idx++;

    kbd_last_used_idx++;
  }

  /* Notify device */
  mmio_write32(kbd_base + VIRTIO_MMIO_QUEUE_NOTIFY / 4, 0);
  mmio_write32(kbd_base + VIRTIO_MMIO_INTERRUPT_ACK / 4,
               mmio_read32(kbd_base + VIRTIO_MMIO_INTERRUPT_STATUS / 4));
}

static int keyboard_init(void) {
  printk(KERN_INFO "KEYBOARD: Initializing virtio-keyboard...\n");

  kbd_base = find_virtio_keyboard();
  if (!kbd_base) {
    printk(KERN_WARNING "KEYBOARD: No virtio keyboard found\n");
    return -1;
  }

  /* Reset device */
  mmio_write32(kbd_base + VIRTIO_MMIO_STATUS / 4, 0);
  while (mmio_read32(kbd_base + VIRTIO_MMIO_STATUS / 4) != 0) {
    asm volatile("nop");
  }

  /* Acknowledge */
  mmio_write32(kbd_base + VIRTIO_MMIO_STATUS / 4, VIRTIO_STATUS_ACK);
  mmio_write32(kbd_base + VIRTIO_MMIO_STATUS / 4,
               VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);

  /* Accept no special features */
  mmio_write32(kbd_base + VIRTIO_MMIO_DRIVER_FEATURES / 4, 0);
  mmio_write32(kbd_base + VIRTIO_MMIO_STATUS / 4,
               VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER |
                   VIRTIO_STATUS_FEATURES_OK);

  /* Setup queue 0 */
  mmio_write32(kbd_base + VIRTIO_MMIO_QUEUE_SEL / 4, 0);

  uint32_t max_queue = mmio_read32(kbd_base + VIRTIO_MMIO_QUEUE_NUM_MAX / 4);
  if (max_queue < QUEUE_SIZE) {
    printk(KERN_WARNING "KEYBOARD: Queue too small\n");
    return -1;
  }

  mmio_write32(kbd_base + VIRTIO_MMIO_QUEUE_NUM / 4, QUEUE_SIZE);

  /* Setup queue memory */
  kbd_desc = (virtq_desc_t *)kbd_queue_mem;
  kbd_avail =
      (virtq_avail_t *)(kbd_queue_mem + QUEUE_SIZE * sizeof(virtq_desc_t));
  kbd_used = (virtq_used_t *)(kbd_queue_mem + 2048);
  kbd_events = kbd_event_bufs;

  /* Set queue addresses */
  uint64_t desc_addr = (uint64_t)(uintptr_t)kbd_desc;
  uint64_t avail_addr = (uint64_t)(uintptr_t)kbd_avail;
  uint64_t used_addr = (uint64_t)(uintptr_t)kbd_used;

  mmio_write32(kbd_base + VIRTIO_MMIO_QUEUE_DESC_LOW / 4, (uint32_t)desc_addr);
  mmio_write32(kbd_base + VIRTIO_MMIO_QUEUE_DESC_HIGH / 4,
               (uint32_t)(desc_addr >> 32));
  mmio_write32(kbd_base + VIRTIO_MMIO_QUEUE_AVAIL_LOW / 4,
               (uint32_t)avail_addr);
  mmio_write32(kbd_base + VIRTIO_MMIO_QUEUE_AVAIL_HIGH / 4,
               (uint32_t)(avail_addr >> 32));
  mmio_write32(kbd_base + VIRTIO_MMIO_QUEUE_USED_LOW / 4, (uint32_t)used_addr);
  mmio_write32(kbd_base + VIRTIO_MMIO_QUEUE_USED_HIGH / 4,
               (uint32_t)(used_addr >> 32));

  /* Initialize descriptors */
  for (int i = 0; i < QUEUE_SIZE; i++) {
    kbd_desc[i].addr = (uint64_t)(uintptr_t)&kbd_events[i];
    kbd_desc[i].len = sizeof(virtio_input_event_t);
    kbd_desc[i].flags = DESC_F_WRITE;
    kbd_desc[i].next = 0;
  }

  /* Fill available ring */
  kbd_avail->flags = 0;
  for (int i = 0; i < QUEUE_SIZE; i++) {
    kbd_avail->ring[i] = i;
  }
  kbd_avail->idx = QUEUE_SIZE;

  /* Queue ready */
  mmio_write32(kbd_base + VIRTIO_MMIO_QUEUE_READY / 4, 1);

  /* Driver OK */
  mmio_write32(kbd_base + VIRTIO_MMIO_STATUS / 4,
               VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER |
                   VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);

  /* Notify device */
  mmio_write32(kbd_base + VIRTIO_MMIO_QUEUE_NOTIFY / 4, 0);

  printk(KERN_INFO "KEYBOARD: Virtio keyboard initialized!\n");
  return 0;
}

/* ===================================================================== */
/* Compatibility API for main.c */
/* ===================================================================== */

int input_init(void) {
  printk(KERN_INFO "INPUT: Initializing input system\n");
#if defined(ARCH_X86_64) || defined(ARCH_X86)
  ps2_init();
#endif
  mouse_init();
  keyboard_init();
  printk(KERN_INFO "INPUT: Ready\n");
  return 0;
}

void input_set_key_callback(void (*callback)(int key)) {
  key_callback = callback;
}

void input_set_gui_key_callback(void (*callback)(int key)) {
  gui_key_callback = callback;
}

void input_poll(void) {
#if defined(ARCH_X86_64) || defined(ARCH_X86)
  ps2_poll();
#endif

  /* Poll UART for keyboard input */
  extern int uart_getc_nonblock(void);
  int c = uart_getc_nonblock();
  if (c >= 0 && key_callback) {
    key_callback(c);
  }

  /* Poll virtio keyboard */
  keyboard_poll();

  /* Poll mouse */
  mouse_poll();
}
