/* install.h - manages the one-time install of the vhidev driver package.
 *
 * The driver persists across reboots once installed. These helpers detect that
 * state and only touch the system when they must. */
#ifndef INSTALL_H
#define INSTALL_H

#include "common.h"

/* True when the vhidev root devnode is present (and therefore the HID
 * children are enumerated). Does not require the driver to currently be
 * running (PnP starts it on demand). */
bool vhidev_is_installed(void);

/* Imports resource/vhidflt.inf into the driver store, creates the
 * root\vhidev devnode, and binds the driver. Needs admin.
 * Safe to call when already installed (returns true without re-installing). */
bool vhidev_install(const char *inf_path);

/* Removes the devnode and deletes the published OEM INF. Needs admin.
 * Currently unused by the PoC but handy for teardown scripting. */
bool vhidev_uninstall(void);

#endif /* INSTALL_H */
