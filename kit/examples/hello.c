#include "../lib/libc.h"

int main(void) {
    uint32_t pid = sys_getpid();
    printf("Hello from Ring 3! PID=%u\n", pid);

    int fd = sys_open("/hello.txt");
    if (fd >= 0) {
        char buf[64];
        int n = sys_read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = 0;
            printf("file: %s\n", buf);
        }
        sys_close(fd);
    }

    sys_sleep(500);
    puts("bye!\n");
    return 0;
}
