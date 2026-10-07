/* hid.c - HID I/O against the vhidev COL05 vendor-defined interface. */
#include "hid.h"

#include <setupapi.h>
#include <hidsdi.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------ *
 *  Interface discovery
 * ------------------------------------------------------------------ */

/* Walks every HID device interface on the system and returns the one whose
 * device instance id contains "VHIDEV&COL05". Returns NULL if not found.
 * Caller frees with free(). */
static char *find_col05_path(void) {
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO set = SetupDiGetClassDevsA(&hidGuid, NULL, NULL,
                                        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    char *found = NULL;
    SP_DEVICE_INTERFACE_DATA ifd = { .cbSize = sizeof(ifd) };

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(set, NULL, &hidGuid, i, &ifd); i++) {
        /* First call gets required size; second fills the detail buffer. */
        DWORD need = 0;
        SP_DEVINFO_DATA info = { .cbSize = sizeof(info) };
        SetupDiGetDeviceInterfaceDetailA(set, &ifd, NULL, 0, &need, &info);
        if (need == 0) continue;

        SP_DEVICE_INTERFACE_DETAIL_DATA_A *detail = malloc(need);
        if (!detail) break;
        detail->cbSize = sizeof(*detail); /* note: struct, not buffer, size */

        if (SetupDiGetDeviceInterfaceDetailA(set, &ifd, detail, need, &need, &info)) {
            char instanceId[512] = {0};
            DWORD r2 = 0;
            if (SetupDiGetDeviceInstanceIdA(set, &info, instanceId, sizeof(instanceId), &r2)) {
                /* case-insensitive substring match */
                char up[512] = {0};
                for (DWORD j = 0; j < sizeof(up) - 1 && instanceId[j]; j++) {
                    up[j] = (char)toupper((unsigned char)instanceId[j]);
                }
                if (strstr(up, "VHIDEV&COL05")) {
                    size_t n = strlen(detail->DevicePath) + 1;
                    found = malloc(n);
                    if (found) memcpy(found, detail->DevicePath, n);
                    free(detail);
                    break;
                }
            }
        }
        free(detail);
    }

    SetupDiDestroyDeviceInfoList(set);
    return found;
}

HANDLE hid_open(void) {
    char *path = find_col05_path();
    if (!path) {
        LOG_ERR("could not find vhidev COL05 HID interface");
        return INVALID_HANDLE_VALUE;
    }
    LOG_INFO("interface: %s", path);

    HANDLE h = CreateFileA(path,
                           GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, 0, NULL);
    free(path);

    if (h == INVALID_HANDLE_VALUE) {
        LOG_ERR("CreateFile failed: %lu", GetLastError());
    }
    return h;
}

/* ------------------------------------------------------------------ *
 *  Report construction + write
 * ------------------------------------------------------------------ */

/* The driver's output report size for COL05 is 64 bytes. We always build the
 * full report (padded with zeros) because WriteFile on HID demands the exact
 * report-length the descriptor declared. */
#define REPORT_SIZE 64

bool hid_write_report(HANDLE h, const uint8_t *report64) {
    DWORD wrote = 0;
    if (!WriteFile(h, report64, REPORT_SIZE, &wrote, NULL)) {
        LOG_ERR("WriteFile failed: %lu", GetLastError());
        return false;
    }
    return wrote == REPORT_SIZE;
}

bool hid_send_mouse(HANDLE h, int dx, int dy, uint8_t buttons, int wheel) {
    uint8_t buf[REPORT_SIZE] = {0};
    buf[0] = 5;                           /* vendor report id */
    buf[1] = 2;                           /* mouse selector */
    buf[2] = buttons;                     /* button mask */
    buf[3] = (uint8_t)(dx & 0xFF);        /* signed int8, caller's responsibility to clamp */
    buf[4] = (uint8_t)(dy & 0xFF);
    buf[5] = (uint8_t)(wheel & 0xFF);
    return hid_write_report(h, buf);
}

bool hid_send_keyboard(HANDLE h, uint8_t modifiers, const uint8_t *usages, size_t count) {
    uint8_t buf[REPORT_SIZE] = {0};
    buf[0] = 5;                           /* vendor report id */
    buf[1] = 1;                           /* keyboard selector */
    buf[2] = modifiers;                   /* modifier byte */
    /* 256-bit NKRO bitmap at [3..34]: bit `k` set == usage `k` pressed. */
    for (size_t i = 0; i < count; i++) {
        uint8_t u = usages[i];
        buf[3 + (u >> 3)] |= (uint8_t)(1u << (u & 7));
    }
    return hid_write_report(h, buf);
}

bool hid_send_consumer(HANDLE h, uint16_t usage) {
    uint8_t buf[REPORT_SIZE] = {0};
    buf[0] = 5;                           /* vendor report id */
    buf[1] = 3;                           /* consumer selector */
    buf[2] = (uint8_t)(usage & 0xFF);
    buf[3] = (uint8_t)((usage >> 8) & 0xFF);
    return hid_write_report(h, buf);
}

bool hid_send_system(HANDLE h, uint16_t usage) {
    uint8_t buf[REPORT_SIZE] = {0};
    buf[0] = 5;                           /* vendor report id */
    buf[1] = 4;                           /* system control selector */
    buf[2] = (uint8_t)(usage & 0xFF);
    buf[3] = (uint8_t)((usage >> 8) & 0xFF);
    return hid_write_report(h, buf);
}
