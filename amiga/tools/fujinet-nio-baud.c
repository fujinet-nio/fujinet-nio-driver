#include <exec/io.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fujinet_nio_device.h"
#include "fujinet_nio_endian.h"
#include "fujinet_boot_config.h"

static void init_request(struct FujiNetNIORequest *req, struct MsgPort *port,
                         UWORD command)
{
    memset(req, 0, sizeof(*req));
    req->fn_io.io_Message.mn_Node.ln_Type = NT_MESSAGE;
    req->fn_io.io_Message.mn_ReplyPort = port;
    req->fn_io.io_Message.mn_Length = sizeof(*req);
    req->fn_io.io_Command = command;
    req->fn_struct_size = FUJINET_NIO_REQUEST_SIZE;
}

static int has_from_prefix(const char *arg)
{
    static const char prefix[] = "FROM=";
    int i;

    for (i = 0; prefix[i] != '\0'; i++) {
        char c = arg[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if (c != prefix[i]) return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    struct MsgPort *port;
    struct IORequest open_request;
    struct FujiNetNIORequest req;
    UBYTE data[4];
    ULONG baud = 0;
    char *end = NULL;
    int from_file = 0;

    if (argc > 2) {
        fprintf(stderr, "Usage: fujinet-nio-baud [300..230400 | FROM=<file>]\n");
        return RETURN_ERROR;
    }
    if (argc == 2 && has_from_prefix(argv[1])) {
        static char text[FUJINET_BOOT_CONFIG_MAX];
        const char *name = argv[1] + (sizeof("FROM=") - 1);
        BPTR fh;
        LONG got;

        /* A missing file is not an error: nothing was configured. */
        fh = Open((STRPTR)name, MODE_OLDFILE);
        if (fh == 0) {
            LONG err = IoErr();
            if (err == ERROR_OBJECT_NOT_FOUND || err == ERROR_DEVICE_NOT_MOUNTED)
                return RETURN_OK;
            fprintf(stderr, "fujinet-nio-baud: cannot read %s\n", name);
            return RETURN_WARN;
        }
        got = Read(fh, text, FUJINET_BOOT_CONFIG_MAX);
        Close(fh);
        if (got < 0) {
            fprintf(stderr, "fujinet-nio-baud: cannot read %s\n", name);
            return RETURN_WARN;
        }
        if (fujinet_boot_config_get_baud(text, (uint16_t)got, &baud) !=
            FUJINET_BOOT_CONFIG_OK) {
            fprintf(stderr, "fujinet-nio-baud: no valid BAUD= in %s\n", name);
            return RETURN_WARN;
        }
        from_file = 1;
    }
    port = CreatePort(NULL, 0);
    if (port == NULL) {
        if (!from_file) return RETURN_FAIL;
        fprintf(stderr, "fujinet-nio-baud: cannot create port\n");
        return RETURN_WARN;
    }
    memset(&open_request, 0, sizeof(open_request));
    if (OpenDevice((CONST_STRPTR)FUJINET_NIO_DEVICE_NAME,
                   FUJINET_NIO_DEVICE_UNIT, &open_request, 0) != 0) {
        DeletePort(port);
        fprintf(stderr, "Cannot open %s\n", FUJINET_NIO_DEVICE_NAME);
        return from_file ? RETURN_WARN : RETURN_FAIL;
    }

    if (argc == 2) {
        if (!from_file) baud = strtoul(argv[1], &end, 10);
        if (!from_file &&
            (*argv[1] == '\0' || *end != '\0' || baud < 300UL || baud > 230400UL)) {
            fprintf(stderr, "Baud must be 300..230400\n");
            CloseDevice(&open_request);
            DeletePort(port);
            return RETURN_ERROR;
        }
        init_request(&req, port, FUJINET_NIO_CMD_SET_BAUD);
        req.fn_io.io_Device = open_request.io_Device;
        req.fn_io.io_Unit = open_request.io_Unit;
        fujinet_nio_put_le32(data, baud);
        req.fn_request_data = data;
        req.fn_request_length = sizeof(data);
        if (DoIO(&req.fn_io) != 0 || req.fn_nio_error != 0) {
            fprintf(stderr, "Cannot set baud (%ld/%u)\n", (long)req.fn_io.io_Error,
                    (unsigned)req.fn_nio_error);
            CloseDevice(&open_request);
            DeletePort(port);
            return from_file ? RETURN_WARN : RETURN_FAIL;
        }
    }

    init_request(&req, port, FUJINET_NIO_CMD_GET_BAUD);
    req.fn_io.io_Device = open_request.io_Device;
    req.fn_io.io_Unit = open_request.io_Unit;
    req.fn_response_data = data;
    req.fn_response_capacity = sizeof(data);
    if (DoIO(&req.fn_io) != 0 || req.fn_nio_error != 0 ||
        req.fn_response_length != sizeof(data)) {
        fprintf(stderr, "Cannot read baud (%ld/%u)\n", (long)req.fn_io.io_Error,
                (unsigned)req.fn_nio_error);
        CloseDevice(&open_request);
        DeletePort(port);
        return from_file ? RETURN_WARN : RETURN_FAIL;
    }
    printf("FujiNet NIO baud: %lu\n",
           (unsigned long)fujinet_nio_get_le32(data));
    CloseDevice(&open_request);
    DeletePort(port);
    return RETURN_OK;
}
