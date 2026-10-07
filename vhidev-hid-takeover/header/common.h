/* common.h - shared types, log macros, small helpers.
 *
 * Log style (prints to stdout, no colors so it works in any console):
 *   [+] success    LOG_OK
 *   [-] error      LOG_ERR
 *   [!] warning    LOG_WARN
 *   [*] info       LOG_INFO
 *   (indented)     LOG         - plain follow-up line, no prefix
 */
#ifndef COMMON_H
#define COMMON_H

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Variadic log helpers. Each line is "<tag> <text>\n".
 * Avoid ##__VA_ARGS__ (GCC extension) so this builds cleanly on MSVC. */
#define LOG_OK(...)   do { printf("[+] "); printf(__VA_ARGS__); printf("\n"); } while (0)
#define LOG_ERR(...)  do { printf("[-] "); printf(__VA_ARGS__); printf("\n"); } while (0)
#define LOG_WARN(...) do { printf("[!] "); printf(__VA_ARGS__); printf("\n"); } while (0)
#define LOG_INFO(...) do { printf("[*] "); printf(__VA_ARGS__); printf("\n"); } while (0)
#define LOG(...)      do { printf("    "); printf(__VA_ARGS__); printf("\n"); } while (0)

/* Prints "press enter to exit..." and reads one line, then exits.
 * Use at terminal paths from main so a double-clicked exe doesn't vanish. */
void pause_exit(int code);

/* Returns true if the current process is running elevated (member of the
 * Administrators group with elevation). */
bool is_elevated(void);

#endif /* COMMON_H */
