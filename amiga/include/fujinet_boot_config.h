#ifndef FUJINET_BOOT_CONFIG_H
#define FUJINET_BOOT_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FUJINET_BOOT_CONFIG_PATH "S:FujiNet-Config"
#define FUJINET_BOOT_CONFIG_MAX  1024U   /* largest file the tools read */
#define FUJINET_BOOT_CONFIG_DEFAULT \
    "; FujiNet boot settings, written by Config\nBAUD=38400\n"

enum {
    FUJINET_BOOT_CONFIG_OK = 0,
    FUJINET_BOOT_CONFIG_NO_KEY,     /* no BAUD= line */
    FUJINET_BOOT_CONFIG_BAD_VALUE   /* not digits, or outside 300..230400 */
};

/* Reads the first BAUD= line. Lines end at LF; a trailing CR and trailing
 * spaces/tabs are ignored; leading spaces/tabs are skipped; lines whose first
 * non-blank character is ';' are comments. */
int fujinet_boot_config_get_baud(const char *text, uint16_t len, uint32_t *baud);

/* Writes `in` with the first BAUD= line replaced (later BAUD= lines dropped,
 * every other line copied unchanged, line endings normalised to LF), or
 * BAUD= appended if none; empty input gives FUJINET_BOOT_CONFIG_DEFAULT's
 * comment line then BAUD=<baud>. Returns bytes written, 0 if it does not fit
 * `cap` (or baud is outside 300..230400). Output always ends in "\n". */
uint16_t fujinet_boot_config_set_baud(const char *in, uint16_t in_len,
                                      uint32_t baud, char *out, uint16_t cap);

/* 1 if a non-comment line runs fujinet-nio-baud with FROM=S:FujiNet-Config
 * (case-insensitive, optional C: prefix, any spacing); else 0. */
int fujinet_boot_config_startup_uses(const char *text, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
