<p align="center">
  <h1 align="center">vhidev-hid-takeover</h1>
  <p align="center">
    <strong>Full hardware-level input injection via a virtual HID kernel driver on Windows</strong>
  </p>
  <p align="center">
    <a href="#how-it-works">How It Works</a> &middot;
    <a href="#capabilities">Capabilities</a> &middot;
    <a href="#the-vulnerability">The Vulnerability</a> &middot;
    <a href="#usage">Usage</a>
  </p>
</p>

---

> **Research use only.** The author is not responsible for any misuse.

> **How this driver was found:** surfaced by [`dzn0/hid-driver-triage`](https://github.com/dzn0/hid-driver-triage), an LLM-operated research pipeline that catalogs Windows HID kernel drivers from public sources and promotes candidates through a six-criterion static + dynamic validation. `vhidev.sys` is its first confirmed target.

## What Is This?

A single `.exe` installs a **WHQL-signed Microsoft driver** into the Windows HID stack and uses it to inject keyboard, mouse, media, and system-control events that are **indistinguishable from a real USB device** to the OS, applications, and anti-cheat engines. Input is born in the kernel, same provenance as physical hardware. No API hooks, no DLL injection, no `SendInput`.

## How It Works

```
 usermode              kernel
 +-----------------+   +-------------+   +------------------+
 | takeover.exe    |-->| vhidflt.sys |-->| Windows HID      |
 | WriteFile 64 B  |   | (KMDF)      |   | subsystem        |
 +-----------------+   +-------------+   +------------------+
                                                |
                                                v
                                      OS / apps / anti-cheat
                                      (sees a real device)
```

The exe writes a 64-byte output report to the driver's vendor collection (`VHIDEV&COL05`). The driver dispatches byte `[1]` (the selector) to the right virtual device:

| Selector | Device           | Payload                                       |
|:--------:|------------------|-----------------------------------------------|
| `1`      | Keyboard (NKRO)  | 1-byte modifiers + 32-byte 256-key bitmap     |
| `2`      | Mouse            | buttons, X, Y, wheel (int8 per axis)          |
| `3`      | Consumer Control | 16-bit usage code (volume, media, launchers)  |
| `4`      | System Control   | 16-bit usage code (sleep, power down, wake)   |

The driver installs **once with admin** (persists across reboots). After that, any process that can open the COL05 HID handle injects input — no further elevation.

## Capabilities

| Device              | What it can do                                                  |
|---------------------|-----------------------------------------------------------------|
| **Keyboard (NKRO)** | 256 simultaneous keys, 8 modifier keys, arbitrary string typing |
| **Mouse**           | Relative X/Y, left/right/middle click, scroll, click-and-drag   |
| **Consumer**        | Volume / mute / play-pause / next-track / launch Calculator     |
| **System**          | Sleep, power down, wake                                         |

Example (full API in [`hid.h`](vhidev-hid-takeover/header/hid.h)):

```c
hid_send_mouse(h, 50, 0, HID_BTN_LEFT, 0);                      /* move +50 X and click */
hid_send_keyboard(h, HID_MOD_LCTRL | HID_MOD_LSHIFT, &esc, 1);  /* Ctrl+Shift+Esc */
hid_send_consumer(h, HID_CC_AL_CALC);                           /* launch Calculator */
hid_send_system(h, HID_SC_SLEEP);                               /* sleep the machine */
```

## The Vulnerability

| Attack property | Detail |
|---|---|
| **Kernel-origin input** | Reports flow through `hidclass.sys` exactly like a real USB device. The OS has no metadata to distinguish virtual from physical. |
| **Persistence** | Install once, survives reboots. PnP starts the driver on demand. |
| **Non-elevated use** | After the one-time install, the usermode component needs no special privileges to inject. |
| **Bypasses usermode detection** | Anti-cheat / screen locker / input sanitizer filters on `SendInput` / `keybd_event` / raw-input hooks **cannot see this** — it is kernel. |
| **No hooking required** | No process injection, no API hooks, no memory patching. The driver is a legitimate signed binary. |

Any security boundary that relies on *"is this input from real hardware?"* is defeated.

## Usage

> **Disposable VM only.**

1. Right-click `vhidev-hid-takeover.exe` → **Run as administrator** (one time).
2. The driver installs automatically (persists after reboot).
3. Pick an action from the menu — mouse square, left click, type `hello from vhidev`, volume up, launch Calculator, sleep, power down.

## Building

Open `vhidev-hid-takeover.sln` in Visual Studio 2022 and build **x64 Release**. The exe lands in `x64/Release/` next to its `resource/` folder, which must contain **four files**:

| File                      | Role                                             |
|---------------------------|--------------------------------------------------|
| `vhidflt.inf`             | install directives                               |
| `vhidflt.sys`             | the KMDF driver                                  |
| `wudf.cat`                | WHQL signature catalog (referenced by `CatalogFile=` in the INF) |
| `WdfCoinstaller01009.dll` | KMDF 1.9 coinstaller (required by `KmdfLibraryVersion=1.9`) |

If `wudf.cat` or `WdfCoinstaller01009.dll` is missing, install fails with `UpdateDriverForPlugAndPlayDevices` error **`3758096943` / `0xE000022F` (SPAPI_E_NO_CATALOG_FOR_OEM_INF)** — the system refuses an unsigned third-party INF. All four come from the [driver cab](#driver-provenance); re-extract if you have deleted them.

## Cleanup

```powershell
pnputil /remove-device "ROOT\VHIDEV\0000"
pnputil /enum-drivers | findstr /i vhidflt
pnputil /delete-driver oem<N>.inf /uninstall /force
```

## Driver Provenance

The bundled `vhidflt.sys` is the WHQL-signed *Virtual HID Provider — HIDClass — 18.13.46.429* package, unmodified from the Microsoft Update Catalog:

[`75861fae-...9036634770.cab`](https://catalog.s.download.windowsupdate.com/d/msdownload/update/driver/drvs/2024/05/75861fae-a035-43ab-90eb-887f3b81f87c_a4dfc2cd21c4b831e22d64d959c5099036634770.cab)

Not hand-picked: pulled, triaged, and promoted to confirmed-target status by [`dzn0/hid-driver-triage`](https://github.com/dzn0/hid-driver-triage). Full per-criterion verdict under its `reports/c8819dbd...414de9f5c/` folder.

## License & Disclaimer

[MIT](LICENSE). Provided for **authorized security research and educational purposes only**. The author is not responsible for any misuse. Always obtain proper authorization before testing on any system you do not own.
