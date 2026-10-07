/* install.c - driver package install / state check.
 *
 * Mirrors what `devcon install vhidflt.inf root\vhidev` does, using the
 * SetupAPI primitives directly so we don't ship a devcon copy. */
#include "install.h"

#include <setupapi.h>
#include <newdev.h>
#include <cfgmgr32.h>
#include <string.h>
#include <stdlib.h>

/* The hardware id we create the devnode with. The INF matches on this. */
static const char kHardwareId[] = "root\\vhidev";

/* ------------------------------------------------------------------ *
 *  State check
 * ------------------------------------------------------------------ */

bool vhidev_is_installed(void) {
    /* Look up any present devnode whose id starts with ROOT\VHIDEV\.
     * CM_Locate_DevNode on the hardware id is the quickest check. */
    DEVINST dev = 0;
    char hwid[] = "root\\vhidev";
    CONFIGRET cr = CM_Locate_DevNodeA(&dev, hwid, CM_LOCATE_DEVNODE_NORMAL);
    return cr == CR_SUCCESS;
}

/* ------------------------------------------------------------------ *
 *  Install
 * ------------------------------------------------------------------ */

bool vhidev_install(const char *inf_path) {
    if (vhidev_is_installed()) {
        LOG_INFO("vhidev already installed - skipping");
        return true;
    }

    /* Resolve the setup class the INF declares (HIDClass). */
    GUID classGuid;
    char className[MAX_CLASS_NAME_LEN] = {0};
    if (!SetupDiGetINFClassA(inf_path, &classGuid, className, sizeof(className), NULL)) {
        LOG_ERR("SetupDiGetINFClass failed: %lu (inf=%s)", GetLastError(), inf_path);
        return false;
    }
    LOG_INFO("inf class: %s", className);

    HDEVINFO set = SetupDiCreateDeviceInfoList(&classGuid, NULL);
    if (set == INVALID_HANDLE_VALUE) {
        LOG_ERR("SetupDiCreateDeviceInfoList failed: %lu", GetLastError());
        return false;
    }

    bool ok = false;
    SP_DEVINFO_DATA devInfo = { .cbSize = sizeof(SP_DEVINFO_DATA) };

    /* Create an empty devnode in our in-memory set, then stamp its hardware
     * id. DICD_GENERATE_ID lets the OS pick the instance number (\0000). */
    if (!SetupDiCreateDeviceInfoA(set, className, &classGuid, NULL, NULL,
                                   DICD_GENERATE_ID, &devInfo)) {
        LOG_ERR("SetupDiCreateDeviceInfo failed: %lu", GetLastError());
        goto done;
    }

    /* SPDRP_HARDWAREID is a REG_MULTI_SZ (double-null-terminated). */
    char multi[64] = {0};
    size_t idlen = strlen(kHardwareId);
    memcpy(multi, kHardwareId, idlen);
    /* multi[idlen] and multi[idlen+1] are already 0 - that's our terminator. */

    if (!SetupDiSetDeviceRegistryPropertyA(set, &devInfo, SPDRP_HARDWAREID,
                                            (const BYTE *)multi, (DWORD)(idlen + 2))) {
        LOG_ERR("SetupDiSetDeviceRegistryProperty failed: %lu", GetLastError());
        goto done;
    }

    /* Commit the empty devnode to the real PnP tree. */
    if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, set, &devInfo)) {
        LOG_ERR("DIF_REGISTERDEVICE failed: %lu", GetLastError());
        goto done;
    }
    LOG_OK("registered devnode root\\vhidev");

    /* Now bind the INF to it. INSTALLFLAG_FORCE lets this overwrite a worse
     * driver match if PnP already picked something else for the same hwid. */
    BOOL rebootRequired = FALSE;
    if (!UpdateDriverForPlugAndPlayDevicesA(NULL, kHardwareId, inf_path,
                                             INSTALLFLAG_FORCE, &rebootRequired)) {
        DWORD err = GetLastError();
        LOG_ERR("UpdateDriverForPlugAndPlayDevices failed: %lu", err);
        /* Roll back the empty devnode so a retry starts clean. */
        SetupDiCallClassInstaller(DIF_REMOVE, set, &devInfo);
        goto done;
    }

    LOG_OK("driver bound (reboot_required=%s)", rebootRequired ? "yes" : "no");
    ok = true;

done:
    SetupDiDestroyDeviceInfoList(set);
    return ok;
}

/* ------------------------------------------------------------------ *
 *  Uninstall (not wired to the UI, kept for teardown scripting)
 * ------------------------------------------------------------------ */

bool vhidev_uninstall(void) {
    /* Easiest path from C: shell out to pnputil. Doing it through SetupAPI
     * requires enumerating the devnode, removing it, then deleting the OEM
     * INF - all of which pnputil wraps in two commands. */
    int rc1 = system("pnputil /remove-device \"ROOT\\VHIDEV\\0000\" >nul 2>&1");
    int rc2 = system("for /f \"tokens=1\" %i in ('pnputil /enum-drivers ^| findstr /i vhidflt') do pnputil /delete-driver %i /uninstall /force >nul 2>&1");
    return rc1 == 0 && rc2 == 0;
}
