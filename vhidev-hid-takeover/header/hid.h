/* hid.h - the HID I/O layer.
 *
 * Everything goes through one 64-byte Output report written to the vhidev
 * vendor-defined collection (COL05). The driver's internal dispatcher looks
 * at byte [1] (the "selector") and routes the remaining bytes to the right
 * virtual device:
 *
 *     [0] = 5   vendor report id (fixed)
 *     [1] = 1   -> keyboard  (bytes 2..34  = modifiers + 32-byte NKRO bitmap)
 *           2   -> mouse     (bytes 2..5   = buttons, X, Y, wheel)
 *           3   -> consumer  (bytes 2..3   = 16-bit usage code, LE)
 *           4   -> system    (bytes 2..3   = 16-bit usage code, LE)
 *     [2..63]   = payload (unused tail zeroed)
 *
 * All senders build the 64-byte report, zero the tail, and WriteFile it. */
#ifndef HID_H
#define HID_H

#include "common.h"

/* HID keyboard modifier bits (byte 2 of the keyboard report). */
#define HID_MOD_LCTRL   0x01
#define HID_MOD_LSHIFT  0x02
#define HID_MOD_LALT    0x04
#define HID_MOD_LGUI    0x08
#define HID_MOD_RCTRL   0x10
#define HID_MOD_RSHIFT  0x20
#define HID_MOD_RALT    0x40
#define HID_MOD_RGUI    0x80

/* HID mouse button bits (byte 2 of the mouse report). */
#define HID_BTN_LEFT    0x01
#define HID_BTN_RIGHT   0x02
#define HID_BTN_MIDDLE  0x04

/* A few consumer usage codes (HID Consumer page 0x0C), see hid.c for more. */
#define HID_CC_MUTE          0x00E2
#define HID_CC_VOL_UP        0x00E9
#define HID_CC_VOL_DOWN      0x00EA
#define HID_CC_PLAY_PAUSE    0x00CD
#define HID_CC_NEXT_TRACK    0x00B5
#define HID_CC_PREV_TRACK    0x00B6
#define HID_CC_AL_CALC       0x0192

/* System control usage codes (HID Generic Desktop page). */
#define HID_SC_POWER_DOWN    0x0081
#define HID_SC_SLEEP         0x0082
#define HID_SC_WAKE          0x0083

/* Open the vhidev COL05 interface for read+write. INVALID_HANDLE_VALUE on
 * failure. Caller closes with CloseHandle(). */
HANDLE hid_open(void);

/* Low-level: send one 64-byte report (what the driver expects on COL05). */
bool hid_write_report(HANDLE h, const uint8_t *report64);

/* High-level senders. Each builds the right report shape and writes it.
 * They do not add inter-event delay - the caller paces as needed. */

/* Mouse: one relative X/Y delta plus button/wheel state.
 * dx, dy, wheel are int8_t (-127..127); buttons is a mask of HID_BTN_*. */
bool hid_send_mouse(HANDLE h, int dx, int dy, uint8_t buttons, int wheel);

/* Keyboard: one report with the given modifier mask and the given set of
 * usage codes (NKRO bitmap - up to 256 simultaneous keys).
 * usages=NULL / count=0 is a valid "release everything" report. */
bool hid_send_keyboard(HANDLE h, uint8_t modifiers, const uint8_t *usages, size_t count);

/* Consumer control / system control: single 16-bit usage code. Passing 0
 * releases the previously held code. */
bool hid_send_consumer(HANDLE h, uint16_t usage);
bool hid_send_system(HANDLE h, uint16_t usage);

#endif /* HID_H */
