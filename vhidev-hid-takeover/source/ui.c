/* ui.c - console menu loop. Keeps parameters out of the way: each menu entry
 * is a self-contained demo that fires immediately on selection. */
#include "ui.h"
#include "hid.h"

#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ *
 *  Small helpers
 * ------------------------------------------------------------------ */

static void sleep_ms(DWORD ms) { Sleep(ms); }

/* Read an int from stdin, discarding the rest of the line. Returns -1 on
 * malformed input. */
static int read_choice(void) {
    char line[64];
    if (!fgets(line, sizeof(line), stdin)) return -1;
    char *end = NULL;
    long v = strtol(line, &end, 10);
    if (end == line) return -1;
    return (int)v;
}

/* Press-and-release helper for a 16-bit single-field collection
 * (consumer / system). Caller provides the sender. */
static void press_release16(HANDLE h,
                            bool (*send)(HANDLE, uint16_t),
                            uint16_t usage,
                            DWORD down_ms) {
    send(h, usage);
    sleep_ms(down_ms);
    send(h, 0);
}

/* Map one ASCII char to its US HID usage code and whether shift is needed.
 * Returns 0 for unmappable chars (caller skips with a warning). */
static uint8_t char_to_usage(char c, bool *out_shift) {
    *out_shift = false;
    if (c >= 'a' && c <= 'z') return (uint8_t)(0x04 + (c - 'a'));
    if (c >= 'A' && c <= 'Z') { *out_shift = true; return (uint8_t)(0x04 + (c - 'A')); }
    if (c >= '1' && c <= '9') return (uint8_t)(0x1E + (c - '1'));
    if (c == '0') return 0x27;
    switch (c) {
        case ' ': return 0x2C;
        case '\t': return 0x2B;
        case '\n': case '\r': return 0x28; /* Enter */
        case '-': return 0x2D;
        case '_': *out_shift = true; return 0x2D;
        case '=': return 0x2E;
        case '+': *out_shift = true; return 0x2E;
        case '.': return 0x37;
        case ',': return 0x36;
        case '/': return 0x38;
        case '!': *out_shift = true; return 0x1E;
        case '?': *out_shift = true; return 0x38;
        default:  return 0;
    }
}

/* Type an ASCII string via NKRO reports. */
static void type_string(HANDLE h, const char *text) {
    for (const char *p = text; *p; p++) {
        bool shift = false;
        uint8_t u = char_to_usage(*p, &shift);
        if (u == 0) {
            LOG_WARN("skipping unmapped char: 0x%02X", (unsigned)(uint8_t)*p);
            continue;
        }
        uint8_t mods = shift ? HID_MOD_LSHIFT : 0;
        hid_send_keyboard(h, mods, &u, 1);
        sleep_ms(25);
        hid_send_keyboard(h, 0, NULL, 0); /* release */
        sleep_ms(35);
    }
}

/* ------------------------------------------------------------------ *
 *  Action handlers
 * ------------------------------------------------------------------ */

static void action_mouse_square(HANDLE h) {
    LOG_INFO("mouse: drawing a square (right / down / left / up, 10 steps each)");
    const int steps = 10;
    const int d = 15;
    for (int i = 0; i < steps; i++) { hid_send_mouse(h, +d, 0, 0, 0); sleep_ms(20); }
    for (int i = 0; i < steps; i++) { hid_send_mouse(h, 0, +d, 0, 0); sleep_ms(20); }
    for (int i = 0; i < steps; i++) { hid_send_mouse(h, -d, 0, 0, 0); sleep_ms(20); }
    for (int i = 0; i < steps; i++) { hid_send_mouse(h, 0, -d, 0, 0); sleep_ms(20); }
    LOG_OK("done");
}

static void action_mouse_click(HANDLE h, uint8_t button, const char *label) {
    LOG_INFO("mouse: %s click (3 s to park cursor on target)", label);
    sleep_ms(3000);
    hid_send_mouse(h, 0, 0, button, 0); /* down */
    sleep_ms(60);
    hid_send_mouse(h, 0, 0, 0,      0); /* up */
    LOG_OK("done");
}

static void action_mouse_scroll(HANDLE h, int delta, const char *label) {
    LOG_INFO("mouse: scroll %s x 5", label);
    for (int i = 0; i < 5; i++) {
        hid_send_mouse(h, 0, 0, 0, delta);
        sleep_ms(60);
        hid_send_mouse(h, 0, 0, 0, 0);
        sleep_ms(80);
    }
    LOG_OK("done");
}

static void action_type_hello(HANDLE h) {
    LOG_INFO("keyboard: typing 'hello from vhidev' (3 s to focus input field)");
    sleep_ms(3000);
    type_string(h, "hello from vhidev");
    LOG_OK("done");
}

static void action_consumer(HANDLE h, uint16_t usage, int count, const char *label) {
    LOG_INFO("consumer: %s x %d", label, count);
    for (int i = 0; i < count; i++) {
        press_release16(h, hid_send_consumer, usage, 50);
        sleep_ms(120);
    }
    LOG_OK("done");
}

static void action_calculator(HANDLE h) {
    LOG_INFO("consumer: launching Calculator (AL Calculator usage 0x192)");
    press_release16(h, hid_send_consumer, HID_CC_AL_CALC, 60);
    LOG_OK("done");
}

static void action_sleep(HANDLE h) {
    LOG_WARN("system: SLEEP in 5 s - CTRL+C now to abort");
    sleep_ms(5000);
    press_release16(h, hid_send_system, HID_SC_SLEEP, 60);
    LOG_OK("sleep report sent");
}

static void action_power_down(HANDLE h) {
    LOG_WARN("system: POWER DOWN in 5 s - CTRL+C now to abort");
    sleep_ms(5000);
    press_release16(h, hid_send_system, HID_SC_POWER_DOWN, 60);
    LOG_OK("power-down report sent");
}

/* ------------------------------------------------------------------ *
 *  Menu
 * ------------------------------------------------------------------ */

static void print_menu(void) {
    printf("\n");
    printf("    === HID Takeover - Action Menu ===\n");
    printf("\n");
    printf("    [1]  Mouse: trace a square\n");
    printf("    [2]  Mouse: left click (3 s warning)\n");
    printf("    [3]  Mouse: right click (3 s warning)\n");
    printf("    [4]  Mouse: scroll down x 5\n");
    printf("    [5]  Mouse: scroll up x 5\n");
    printf("    [6]  Keyboard: type 'hello from vhidev' (3 s warning)\n");
    printf("    [7]  Consumer: volume up x 5\n");
    printf("    [8]  Consumer: volume down x 5\n");
    printf("    [9]  Consumer: mute toggle\n");
    printf("    [10] Consumer: media play/pause\n");
    printf("    [11] Consumer: launch Calculator\n");
    printf("    [12] System: sleep (5 s warning)\n");
    printf("    [13] System: power down (5 s warning)\n");
    printf("    [0]  Exit\n");
    printf("\n");
    printf("    > ");
    fflush(stdout);
}

void ui_main_loop(HANDLE h) {
    for (;;) {
        print_menu();
        int c = read_choice();
        switch (c) {
            case 0:  return;
            case 1:  action_mouse_square(h);                        break;
            case 2:  action_mouse_click (h, HID_BTN_LEFT,  "left"); break;
            case 3:  action_mouse_click (h, HID_BTN_RIGHT, "right");break;
            case 4:  action_mouse_scroll(h, -1, "down");            break;
            case 5:  action_mouse_scroll(h, +1, "up");              break;
            case 6:  action_type_hello  (h);                        break;
            case 7:  action_consumer(h, HID_CC_VOL_UP,     5, "volume up");   break;
            case 8:  action_consumer(h, HID_CC_VOL_DOWN,   5, "volume down"); break;
            case 9:  action_consumer(h, HID_CC_MUTE,       1, "mute");        break;
            case 10: action_consumer(h, HID_CC_PLAY_PAUSE, 1, "play/pause");  break;
            case 11: action_calculator(h);                          break;
            case 12: action_sleep(h);                               break;
            case 13: action_power_down(h);                          break;
            default: LOG_WARN("invalid choice");                    break;
        }
    }
}
