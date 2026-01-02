// xcr.h - simple stub for xcr command
#ifndef XCR_H
#define XCR_H

#include <stdint.h>
#include "../text.h"

static inline void cmd_xcr(const char* arg) {
    if (!arg || !arg[0]) {
        printf("{FG(255,165,0)}xcr: usage: xcr <arg>\n");
        return;
    }
    printf("{FG(0,255,0)}xcr: called with '%s'\n", arg);
}

#endif
