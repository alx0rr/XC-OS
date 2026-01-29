#include "../include/text.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: banner <text>\n");
        return 1;
    }
    
    printf("\n");
    printf("  ╔══════════════════════════════════════╗\n");
    printf("  ║                                      ║\n");
    printf("  ║  %s", argv[1]);
    
    int len = 0;
    for (int i = 0; argv[1][i]; i++) len++;
    
    for (int i = len; i < 34; i++) {
        printf(" ");
    }
    
    printf("║\n");
    printf("  ║                                      ║\n");
    printf("  ╚══════════════════════════════════════╝\n");
    printf("\n");
    
    return 0;
}
