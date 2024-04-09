#include <stdbool.h>
#include <stdint.h>

bool quickread_2bitmap(const char * filename, uint8_t * buffer, uint32_t * palette);
void quickwrite_2bitmap(const char * filename, uint8_t * buffer, uint32_t * palette);
