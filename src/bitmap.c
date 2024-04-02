#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <cairo.h>

uint8_t bitmapheader[54];
//uint32_t palette[4];

// Read big endian data into little endian int
uint32_t read_data(FILE * file) {
  uint32_t result = 0;
  result |= fgetc(file);
  result |= fgetc(file) << 8;
  result |= fgetc(file) << 16;
  result |= fgetc(file) << 24;
  return result;
}

// The inverse
static void write_data(FILE * file, uint32_t data) {
  fputc(data & 0xFF, file); fputc((data >> 8) & 0xFF, file);
  fputc((data >> 16) & 0xFF, file); fputc((data >> 24) & 0xFF, file);
}

/**
 * Read a bitmap file the quick-and-dirty way:
 * expect 14-byte main header, 40-byte bitmap info header
 * then 4*4=16 byte color table (2-bit color)
 * then 64x64x2-bit (= 256 words) data.
 *
 * Note: not all programs can read (or write) 2-bit color
 * bitmaps (they expect 1, 4 or 8).
 */
bool quickread_2bitmap(const char * filename, char * buffer, uint32_t * palette) {
    FILE * file = fopen(filename, "rb");
    if (file == NULL) {
        printf("file not found\n");
        return false;
    }

    for (int i=0; i<54; i++) {
        // Save header data so we can equally quick-and-dirty re-use it for writing.
        bitmapheader[i] = fgetc(file);
    }

    // Read palette
    for(int i=0; i<4;i++) {
        palette[i] = read_data(file);
    }

    // Read data
    for(int i=0; i<256;i++) {
        ((uint32_t*) buffer)[i] = read_data(file);
    }

    fclose(file);
    return true;
}

void quickwrite_2bitmap(const char * filename, char * buffer, uint32_t * palette) {
    FILE * file = fopen(filename, "wb");
    
    for (int i=0; i<54; i++) {
        // Re-use read bitmap header
        fputc(bitmapheader[i], file);
    }

    // Write palette
    for(int i=0; i<4;i++) {
        write_data(file, palette[i]);
    }

    // Write data
    for(int i=0; i<256;i++) {
        write_data(file, ((uint32_t*) buffer)[i]);
    }

    fclose(file);
}

