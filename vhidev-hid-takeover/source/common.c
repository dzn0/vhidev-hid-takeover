/* common.c - helpers declared in common.h. */
#include "common.h"
#include <stdlib.h>

void pause_exit(int code) {
    printf("\n    press enter to exit...");
    (void)getchar();
    exit(code);
}

bool is_elevated(void) {
    /* GetTokenInformation(TokenElevation) is the canonical check. */
    HANDLE token = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }
    TOKEN_ELEVATION elev = {0};
    DWORD cb = 0;
    bool elevated = false;
    if (GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &cb)) {
        elevated = (elev.TokenIsElevated != 0);
    }
    CloseHandle(token);
    return elevated;
}
