#include "pasta.h"

int main (int argc, char ** argv) {
    pasta_init();
    mainloop(&argv[1]);
}
