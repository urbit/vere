#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ent.h"

int main(void) {
    char buf[256] = {0};

    if (0 != ent_getentropy(buf, sizeof(buf))) {
        int errno_ = errno;
        perror("getentropy");
        exit(errno_);
    }

    for (int i = 0; i < sizeof buf; i++) {
        printf("%02hhx", buf[i]);
    }

    puts("");
}
