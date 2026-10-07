/* ui.h - very small console menu loop. */
#ifndef UI_H
#define UI_H

#include "common.h"

/* Prints the action menu and dispatches the user's choice. Returns when the
 * user picks "exit". `h` must be an open vhidev COL05 handle. */
void ui_main_loop(HANDLE h);

#endif /* UI_H */
