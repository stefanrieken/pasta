#include <stdbool.h>
#include <stdint.h>

bool quickread_2bitmap(const char * filename, char * buffer, uint32_t * palette);
void quickwrite_2bitmap(const char * filename, char * buffer, uint32_t * palette);
