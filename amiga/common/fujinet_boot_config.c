#include "fujinet_boot_config.h"

#include <stddef.h>
#include <string.h>

#define BAUD_MIN 300UL
#define BAUD_MAX 230400UL

static int is_blank(char c)
{
    return c == ' ' || c == '\t';
}

static char lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

/* Finds the next line at *pos. On return [*start,*end) is the line without LF,
 * with trailing CR/blanks removed. Returns 0 at end of input. */
static int next_line(const char *text, uint16_t len, uint16_t *pos,
                     uint16_t *raw_end, uint16_t *start, uint16_t *end)
{
    uint16_t p = *pos;
    uint16_t e;

    if (p >= len) {
        return 0;
    }
    *start = p;
    while (p < len && text[p] != '\n') {
        ++p;
    }
    e = p;
    *pos = (uint16_t)(p < len ? p + 1U : p);
    *raw_end = e;
    while (e > *start && (text[e - 1U] == '\r' || is_blank(text[e - 1U]))) {
        --e;
    }
    *end = e;
    return 1;
}

static uint16_t skip_blanks(const char *text, uint16_t s, uint16_t e)
{
    while (s < e && is_blank(text[s])) {
        ++s;
    }
    return s;
}

/* Returns 1 and sets *vs to the value start when the line is "BAUD=...". */
static int baud_line(const char *text, uint16_t s, uint16_t e, uint16_t *vs)
{
    s = skip_blanks(text, s, e);
    if ((uint16_t)(e - s) < 5U || memcmp(text + s, "BAUD=", 5) != 0) {
        return 0;
    }
    *vs = (uint16_t)(s + 5U);
    return 1;
}

int fujinet_boot_config_get_baud(const char *text, uint16_t len, uint32_t *baud)
{
    uint16_t pos = 0, raw, s, e, v;
    uint32_t value = 0;

    while (next_line(text, len, &pos, &raw, &s, &e)) {
        if (!baud_line(text, s, e, &v)) {
            continue;
        }
        if (v >= e) {
            return FUJINET_BOOT_CONFIG_BAD_VALUE;
        }
        for (; v < e; ++v) {
            if (text[v] < '0' || text[v] > '9') {
                return FUJINET_BOOT_CONFIG_BAD_VALUE;
            }
            value = value * 10UL + (uint32_t)(text[v] - '0');
            if (value > BAUD_MAX) {
                return FUJINET_BOOT_CONFIG_BAD_VALUE;
            }
        }
        if (value < BAUD_MIN) {
            return FUJINET_BOOT_CONFIG_BAD_VALUE;
        }
        *baud = value;
        return FUJINET_BOOT_CONFIG_OK;
    }
    return FUJINET_BOOT_CONFIG_NO_KEY;
}

/* Appends to out at *used; returns 0 if it does not fit. */
static int put(char *out, uint16_t cap, uint16_t *used,
               const char *src, uint16_t n)
{
    if (n > (uint16_t)(cap - *used)) {
        return 0;
    }
    memcpy(out + *used, src, n);
    *used = (uint16_t)(*used + n);
    return 1;
}

static int put_baud(char *out, uint16_t cap, uint16_t *used, uint32_t baud)
{
    char digits[12];
    char line[24];
    uint16_t nd = 0, n = 0;

    do {
        digits[nd++] = (char)('0' + (baud % 10UL));
        baud /= 10UL;
    } while (baud != 0UL);
    memcpy(line, "BAUD=", 5);
    n = 5;
    while (nd != 0U) {
        line[n++] = digits[--nd];
    }
    line[n++] = '\n';
    return put(out, cap, used, line, n);
}

uint16_t fujinet_boot_config_set_baud(const char *in, uint16_t in_len,
                                      uint32_t baud, char *out, uint16_t cap)
{
    uint16_t pos = 0, raw, s, e, v, used = 0;
    int replaced = 0;

    if (baud < BAUD_MIN || baud > BAUD_MAX) {
        return 0;
    }
    if (in_len == 0U) {
        static const char comment[] = "; FujiNet boot settings, written by Config\n";
        if (!put(out, cap, &used, comment, (uint16_t)(sizeof(comment) - 1U)) ||
            !put_baud(out, cap, &used, baud)) {
            return 0;
        }
        return used;
    }
    while (next_line(in, in_len, &pos, &raw, &s, &e)) {
        uint16_t copy_end = raw;

        if (baud_line(in, s, e, &v)) {
            if (!replaced) {
                if (!put_baud(out, cap, &used, baud)) {
                    return 0;
                }
                replaced = 1;
            }
            continue;
        }
        if (copy_end > s && in[copy_end - 1U] == '\r') {
            --copy_end;
        }
        if (!put(out, cap, &used, in + s, (uint16_t)(copy_end - s)) ||
            !put(out, cap, &used, "\n", 1U)) {
            return 0;
        }
    }
    if (!replaced && !put_baud(out, cap, &used, baud)) {
        return 0;
    }
    return used;
}

/* Case-insensitive compare of text[s,e) with a lower-case literal. */
static int token_is(const char *text, uint16_t s, uint16_t e, const char *lit)
{
    uint16_t n = (uint16_t)strlen(lit);
    uint16_t i;

    if ((uint16_t)(e - s) != n) {
        return 0;
    }
    for (i = 0; i < n; ++i) {
        if (lower(text[s + i]) != lit[i]) {
            return 0;
        }
    }
    return 1;
}

int fujinet_boot_config_startup_uses(const char *text, uint16_t len)
{
    uint16_t pos = 0, raw, s, e;

    while (next_line(text, len, &pos, &raw, &s, &e)) {
        uint16_t t, te, c;

        t = skip_blanks(text, s, e);
        if (t >= e || text[t] == ';') {
            continue;
        }
        te = t;
        while (te < e && !is_blank(text[te])) {
            ++te;
        }
        c = te;
        while (c > t && text[c - 1U] != ':' && text[c - 1U] != '/') {
            --c;
        }
        if (!token_is(text, c, te, "fujinet-nio-baud")) {
            continue;
        }
        t = te;
        while (t < e) {
            t = skip_blanks(text, t, e);
            te = t;
            while (te < e && !is_blank(text[te])) {
                ++te;
            }
            if (te > t && token_is(text, t, te, "from=s:fujinet-config")) {
                return 1;
            }
            t = te;
        }
    }
    return 0;
}
