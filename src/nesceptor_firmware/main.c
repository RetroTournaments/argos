#include <stdio.h>
#include "pico/stdlib.h"

int main() {
    stdio_init_all();
    while (true) {
        int c = getchar();
        printf("%c\n", c);
    }
}
