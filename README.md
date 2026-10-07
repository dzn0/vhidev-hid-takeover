<p align="center">
  <h1 align="center">vhidev-hid-takeover</h1>
  <p align="center">
    <strong>Full hardware-level input injection via a virtual HID kernel driver on Windows</strong>
  </p>
  <p align="center">
    <a href="#how-it-works">How It Works</a> &middot;
    <a href="#capabilities">Capabilities</a> &middot;
    <a href="#the-vulnerability">The Vulnerability</a> &middot;
    <a href="#building">Building</a> &middot;
    <a href="#usage">Usage</a>
  </p>
</p>

---

> **Research use only.** This is a proof-of-concept for security research and education. The author is not responsible for any misuse of this software.

## What Is This?

**vhidev-hid-takeover** is a proof-of-concept that demonstrates a Windows input takeover attack using a kernel-mode virtual HID (Human Interface Device) driver. A single unprivileged-looking executable installs a signed-format driver into the HID stack, then injects keyboard, mouse, media, and system control events that are **indistinguishable from real hardware** to the operating system, applications, and anti-cheat engines.

The injected input travels through the same kernel path as a physical USB keyboard or mouse. Nothing in usermode can tell the difference.

## How It Works

```
 usermode app                 kernel
 +-----------------+         +------------------+         +------------------+
 | vhidev-hid-     |  Write  |   vhidflt.sys    |  HID    |   Windows HID    |
 | takeover.exe    |-------->|  (KMDF filter    |-------->|   subsystem       |
 | (64-byte report)|  File   |   driver)        | reports | (hidclass, etc.) |
 +-----------------+         +------------------+         +------------------+
                                                                  |
                                                          appears as real
                                                          hardware input
                                                                  |
                                                                  v
                                                          OS / apps / games
```

1. **Elevation check** - the app requires Administrator to install the driver (one-time only).
2. **Driver install** - creates a `root\vhidev` devnode via SetupAPI and binds the INF + `vhidflt.sys`. This is the equivalent of `devcon install`, done purely through the API. **The driver persists across reboots.**
3. **COL05 interface open** - locates the vendor-defined HID collection (`VHIDEV&COL05`) among all HID device interfaces and opens it with `CreateFile`.
4. **Report injection** - builds 64-byte output reports and writes them. The kernel driver dispatches byte `[1]` (the selector) to the appropriate virtual device:

| Selector | Device           | Payload layout                          |
|----------|------------------|-----------------------------------------|
| `1`      | Keyboard (NKRO)  | `[2]` modifiers + `[3..34]` 256-bit bitmap |
| `2`      | Mouse            | `[2]` buttons, `[3]` X, `[4]` Y, `[5]` wheel |
| `3`      | Consumer Control | `[2..3]` 16-bit usage code (LE)         |
| `4`      | System Control   | `[2..3]` 16-bit usage code (LE)         |

## Capabilities

### Keyboard
- Full **N-Key Rollover** (NKRO) - 256 simultaneous keys via bitmap
- All 8 modifier keys (LCtrl, LShift, LAlt, LGui, RCtrl, RShift, RAlt, RGui)
- Arbitrary string typing with ASCII-to-HID-usage mapping
- Press, hold, and release with per-key precision

```c
/* Press Ctrl+Shift+Esc (open Task Manager) and release. */
uint8_t esc = 0x29;
hid_send_keyboard(h, HID_MOD_LCTRL | HID_MOD_LSHIFT, &esc, 1);
hid_send_keyboard(h, 0, NULL, 0);  /* release everything */
```

### Mouse
- Relative X/Y movement (int8 per axis per report)
- Left, right, and middle click
- Scroll wheel (up/down)
- Click-and-drag patterns

```c
/* Move cursor 50 pixels right and click. */
hid_send_mouse(h, 50, 0, 0,               0);  /* X+50 */
hid_send_mouse(h, 0,  0, HID_BTN_LEFT,    0);  /* button down */
hid_send_mouse(h, 0,  0, 0,               0);  /* button up   */
```

### Consumer Control (Media Keys)
- Volume up / down / mute
- Play / pause, next track, previous track
- **Application launch** (e.g. Calculator via usage `0x0192`)

```c
/* Open Calculator via the AL Consumer Control Config usage. */
hid_send_consumer(h, HID_CC_AL_CALC);
hid_send_consumer(h, 0);  /* release */
```

### System Control
- **Sleep** the machine
- **Power down** (shutdown) the machine
- Wake from standby

```c
/* Put the machine to sleep. */
hid_send_system(h, HID_SC_SLEEP);
hid_send_system(h, 0);  /* release */
```

## The Vulnerability

This PoC highlights a fundamental weakness in the Windows HID input model:

| Attack property | Detail |
|---|---|
| **Kernel-level injection** | Reports flow through `hidclass.sys` exactly like a real USB device. The OS has no metadata to distinguish virtual from physical. |
| **Persistence** | The driver installs once and **survives reboots**. The devnode (`root\vhidev`) stays in the PnP tree and PnP starts the driver on demand. No re-installation needed. |
| **Stealth** | After the initial install, the usermode component needs no special privileges to open the COL05 handle and inject input. There is no visible window, tray icon, or notification. |
| **Bypasses software-level protections** | Anti-cheat systems, screen lockers, UAC prompts, and input sanitizers that filter `SendInput`/`mouse_event`/`keybd_event` **cannot detect this** because those APIs are usermode - this is kernel. |
| **Filter driver capability** | The INF can also attach `vhidflt.sys` as a lower filter on **real USB HID devices**, enabling input interception and injection on actual hardware device stacks. |
| **System control** | Can force the machine to sleep or power down, bypassing any "are you sure?" prompt since the HID system control path is unconditional. |
| **No hooking required** | Unlike traditional input injection (SetWindowsHookEx, raw input hooks, DLL injection), this approach installs a legitimate driver. No process injection, no API hooking, no memory patching. |

### Attack surface

```
Attacker (admin, once)           Attacker (any user, forever)
        |                                   |
        v                                   v
 [install vhidflt.sys]            [open COL05 handle]
 [create root\vhidev]             [WriteFile 64-byte reports]
        |                                   |
        +-----------------------------------+
                        |
                        v
              Windows HID subsystem
              (trusts all reports equally)
                        |
                        v
              Target machine fully controlled
              keyboard + mouse + media + power
```

### Why this matters

Most input-injection detection focuses on usermode API calls (`SendInput`, `PostMessage`, `keybd_event`). A virtual HID driver completely sidesteps that layer. The input is born inside the kernel, with the same provenance as a physical device. Any security boundary that relies on "is this input from real hardware?" is defeated.

## Building

**Requirements:**
- Visual Studio 2022 (v143 toolset)
- Windows SDK
- The bundled `vhidflt.sys` and `vhidflt.inf` in `resource/`

Open `vhidev-hid-takeover.sln` and build for **x64 Release**. The output lands in `x64/Release/`.

Ensure `resource/vhidflt.sys` and `resource/vhidflt.inf` are next to the built executable.

## Usage

> Run inside a **disposable VM only**.

```
1. Right-click vhidev-hid-takeover.exe -> Run as administrator
2. The driver installs automatically on first run (persists after reboot)
3. Select an action from the menu:
```

```
=== HID Takeover - Action Menu ===

[1]  Mouse: trace a square
[2]  Mouse: left click (3 s warning)
[3]  Mouse: right click (3 s warning)
[4]  Mouse: scroll down x 5
[5]  Mouse: scroll up x 5
[6]  Keyboard: type 'hello from vhidev' (3 s warning)
[7]  Consumer: volume up x 5
[8]  Consumer: volume down x 5
[9]  Consumer: mute toggle
[10] Consumer: media play/pause
[11] Consumer: launch Calculator
[12] System: sleep (5 s warning)
[13] System: power down (5 s warning)
[0]  Exit
```

## Cleanup

The driver persists across reboots. To remove it:

```powershell
pnputil /remove-device "ROOT\VHIDEV\0000"
# Then remove the OEM INF from the driver store
pnputil /enum-drivers | findstr /i vhidflt
pnputil /delete-driver oem<N>.inf /uninstall /force
```

## Project Structure

```
vhidev-hid-takeover/
  header/
    common.h        - log macros, elevation check, shared types
    hid.h           - HID report protocol, constants, sender API
    install.h       - driver install/uninstall interface
    ui.h            - console menu loop
  source/
    main.c          - entry point: elevate -> install -> open -> menu
    common.c        - pause_exit, is_elevated
    hid.c           - COL05 discovery, report construction, WriteFile
    install.c       - SetupAPI devnode creation, driver binding
    ui.c            - action handlers, ASCII-to-HID mapper, menu
  resource/
    vhidflt.sys     - the kernel-mode KMDF virtual HID filter driver
    vhidflt.inf     - INF for driver installation (HIDClass)
```

## Driver Provenance

The bundled `vhidflt.sys` is the WHQL-signed *Virtual HID Provider - HIDClass - 18.13.46.429* package, unmodified from the Microsoft Update Catalog:

[`75861fae-...9036634770.cab`](https://catalog.s.download.windowsupdate.com/d/msdownload/update/driver/drvs/2024/05/75861fae-a035-43ab-90eb-887f3b81f87c_a4dfc2cd21c4b831e22d64d959c5099036634770.cab)

## License

[MIT](LICENSE)

## Disclaimer

This software is provided for **authorized security research and educational purposes only**. The author is not responsible for any misuse. Always obtain proper authorization before testing on any system you do not own.
