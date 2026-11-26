#include "fujinet_boot_config.h"

#include <stdio.h>
#include <string.h>

static unsigned failures;

#define CHECK(name, expression) do {                                      \
    if (!(expression)) {                                                  \
        fprintf(stderr, "FAIL: %s (line %d)\n", name, __LINE__);          \
        ++failures;                                                       \
    }                                                                     \
} while (0)

#define GET(text, bp) fujinet_boot_config_get_baud((text), (uint16_t)strlen(text), (bp))
#define USES(text) fujinet_boot_config_startup_uses((text), (uint16_t)strlen(text))
#define SET(text, baud, out, cap) \
    fujinet_boot_config_set_baud((text), (uint16_t)strlen(text), (baud), (out), (cap))

int main(void)
{
    uint32_t b = 0;
    char out[256];
    uint16_t n;

    CHECK("get-basic", GET("BAUD=38400\n", &b) == 0 && b == 38400UL);
    CHECK("get-crlf-trailing-space-no-eol",
          GET("; x\r\nBAUD=57600  \r\n", &b) == 0 && b == 57600UL);
    CHECK("get-no-final-newline", GET("BAUD=9600", &b) == 0 && b == 9600UL);
    CHECK("get-tab-and-indent", GET("  BAUD=115200\t\n", &b) == 0 && b == 115200UL);
    CHECK("get-first-wins", GET("BAUD=19200\nBAUD=9600\n", &b) == 0 && b == 19200UL);
    CHECK("get-comment-ignored", GET(";BAUD=9600\n", &b) == FUJINET_BOOT_CONFIG_NO_KEY);
    CHECK("get-missing", GET("SERIAL=x\n", &b) == FUJINET_BOOT_CONFIG_NO_KEY);
    CHECK("get-empty", fujinet_boot_config_get_baud("", 0, &b) == FUJINET_BOOT_CONFIG_NO_KEY);
    CHECK("get-bad", GET("BAUD=fast\n", &b) == FUJINET_BOOT_CONFIG_BAD_VALUE);
    CHECK("get-empty-value", GET("BAUD=\n", &b) == FUJINET_BOOT_CONFIG_BAD_VALUE);
    CHECK("get-trailing-junk", GET("BAUD=9600x\n", &b) == FUJINET_BOOT_CONFIG_BAD_VALUE);
    CHECK("get-low", GET("BAUD=299\n", &b) == FUJINET_BOOT_CONFIG_BAD_VALUE);
    CHECK("get-min", GET("BAUD=300\n", &b) == 0 && b == 300UL);
    CHECK("get-max", GET("BAUD=230400\n", &b) == 0 && b == 230400UL);
    CHECK("get-high", GET("BAUD=230401\n", &b) == FUJINET_BOOT_CONFIG_BAD_VALUE);
    CHECK("get-overflow", GET("BAUD=99999999999999\n", &b) == FUJINET_BOOT_CONFIG_BAD_VALUE);

    n = SET("; c\nSERIAL=x\nBAUD=38400\nBAUD=1\n", 57600UL, out, sizeof(out));
    CHECK("set-replace-keep-others",
          n == strlen("; c\nSERIAL=x\nBAUD=57600\n") &&
          strncmp(out, "; c\nSERIAL=x\nBAUD=57600\n", n) == 0);
    n = SET("SERIAL=x", 9600UL, out, sizeof(out));
    CHECK("set-append",
          n == strlen("SERIAL=x\nBAUD=9600\n") &&
          strncmp(out, "SERIAL=x\nBAUD=9600\n", n) == 0);
    n = SET("", 38400UL, out, sizeof(out));
    CHECK("set-empty-gives-default", n == strlen(FUJINET_BOOT_CONFIG_DEFAULT) &&
          strncmp(out, FUJINET_BOOT_CONFIG_DEFAULT, n) == 0);
    n = SET("a=1\r\nBAUD=1  \r\nb=2\r\n", 9600UL, out, sizeof(out));
    CHECK("set-crlf-normalised",
          n == strlen("a=1\nBAUD=9600\nb=2\n") &&
          strncmp(out, "a=1\nBAUD=9600\nb=2\n", n) == 0);
    n = SET(";BAUD=1\n", 9600UL, out, sizeof(out));
    CHECK("set-comment-kept-append",
          n == strlen(";BAUD=1\nBAUD=9600\n") &&
          strncmp(out, ";BAUD=1\nBAUD=9600\n", n) == 0);
    CHECK("set-too-small", SET("BAUD=38400\n", 115200UL, out, 8) == 0);
    CHECK("set-exact-fit", SET("", 9600UL, out,
          (uint16_t)strlen("; FujiNet boot settings, written by Config\nBAUD=9600\n")) != 0);
    CHECK("set-one-short", SET("", 9600UL, out,
          (uint16_t)(strlen("; FujiNet boot settings, written by Config\nBAUD=9600\n") - 1U)) == 0);

    CHECK("startup-yes", USES(
          "C:fujinet-load-resident x\nC:fujinet-nio-baud FROM=S:FujiNet-Config\n") == 1);
    CHECK("startup-case-noprefix", USES(
          "  FujiNet-NIO-Baud  from=s:fujinet-config\n") == 1);
    CHECK("startup-sys-prefix-crlf", USES(
          "SYS:C/fujinet-nio-baud FROM=S:FujiNet-Config\r\n") == 1);
    CHECK("startup-commented", USES(
          "; C:fujinet-nio-baud FROM=S:FujiNet-Config\n") == 0);
    CHECK("startup-fixed-rate", USES("C:fujinet-nio-baud 38400\n") == 0);
    CHECK("startup-other-file", USES(
          "C:fujinet-nio-baud FROM=S:Other\n") == 0);
    CHECK("startup-other-command", USES(
          "C:echo FROM=S:FujiNet-Config\n") == 0);

    if (failures) {
        fprintf(stderr, "%u failure(s)\n", failures);
        return 1;
    }
    puts("test_fujinet_boot_config: ok");
    return 0;
}
