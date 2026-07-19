#include "../lib/libc.h"

int main(void) {
    puts("malloc test\n");

    char *a = malloc(128);
    char *b = malloc(64);

    if (!a || !b) {
        puts("malloc failed\n");
        return 1;
    }

    memset(a, 'A', 127); a[127] = 0;
    memset(b, 'B', 63);  b[63]  = 0;

    printf("a[0]=%c a[126]=%c\n", a[0], a[126]);
    printf("b[0]=%c b[62]=%c\n",  b[0], b[62]);

    puts("ok\n");
    return 0;
}
