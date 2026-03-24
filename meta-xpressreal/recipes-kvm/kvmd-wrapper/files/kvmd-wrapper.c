#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SET_LUN_SCRIPT "/usr/local/bin/kvmd-set-lun-file.py"
#define USBIP_SCRIPT   "/usr/local/bin/kvmd-usbip.py"

static void become_root() {
    if (setuid(0) != 0) {
        perror("setuid");
        exit(1);
    }
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        fprintf(stderr,
            "Usage:\n"
            "  kvmd-wrapper <backing_file>\n"
            "  kvmd-wrapper usbip attach <IP>\n"
            "  kvmd-wrapper usbip detach\n"
        );
        return 1;
    }

    /* Elevate privileges */
    become_root();

    /* -----------------------------
     * Mode A: Set LUN backing file
     * ----------------------------- */
    if (argc == 2 && strcmp(argv[1], "usbip") != 0) {
        execl(SET_LUN_SCRIPT, SET_LUN_SCRIPT, argv[1], (char *)NULL);
        perror("execl SET_LUN_SCRIPT");
        return 1;
    }

    /* -----------------------------
     * Mode B: USBIP control
     * ----------------------------- */
    if (strcmp(argv[1], "usbip") == 0) {

        /* usbip attach <IP> */
        if (argc == 4 && strcmp(argv[2], "attach") == 0) {
            execl(USBIP_SCRIPT, USBIP_SCRIPT, "attach", argv[3], (char *)NULL);
            perror("execl USBIP_SCRIPT (attach)");
            return 1;
        }

        /* usbip detach */
        if (argc == 3 && strcmp(argv[2], "detach") == 0) {
            execl(USBIP_SCRIPT, USBIP_SCRIPT, "detach", (char *)NULL);
            perror("execl USBIP_SCRIPT (detach)");
            return 1;
        }

        fprintf(stderr,
            "USBIP usage:\n"
            "  kvmd-wrapper usbip attach <IP>\n"
            "  kvmd-wrapper usbip detach\n"
        );
        return 1;
    }

    fprintf(stderr, "Invalid parameters.\n");
    return 1;
}
