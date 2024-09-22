#include "pasta.h"

#ifdef PICO_SDK
#include "pico/stdlib.h"
#endif

int main (int argc, char ** argv) {
#ifdef PICO_SDK
    stdio_init_all();
    pasta_init();
    parse(stdin, true);
#else
    pasta_init();
    mainloop(&argv[1]);
#endif
}
