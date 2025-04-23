#include "pasta.h"

FILE * open_file (const char * filename, const char * mode) {
  return fopen(filename, mode);
}

// This is the entry point for plain Pasta only;
// other implementations have their own main.
int main (int argc, char ** argv) {
    pasta_init();
    mainloop(&argv[1]);
}
