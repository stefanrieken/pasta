#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <cairo.h>

uint8_t bitmapheader[54];
uint32_t palette[4];

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
bool quickread_2bitmap(const char * filename, char * buffer) {
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

void quickwrite_2bitmap(const char * filename, char * buffer) {
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

/*
// Test buffer
char buffer[1024];


// Test program
void * mainloop(void * args) {
  uint8_t * pixels = cairo_image_surface_get_data(surface);
  int rowstride = cairo_image_surface_get_stride(surface);

  uint32_t pixels_per_byte = 4;

  for (int i=0;i<64;i++) {
    for(int j=0; j<64;j++) {
      // gather indexed pixel data; find palette color
      uint32_t pixel_idx = (i * 64)+j;
      uint32_t pixel_byte_idx = pixel_idx / pixels_per_byte;
      uint32_t shift = (pixel_idx % pixels_per_byte) * 2;

      uint8_t px = (buffer[pixel_byte_idx] >> (6-shift)) & 0b11;
      uint32_t color = palette[px];

      // and copy to pixbuf
      uint8_t * pixel = &pixels[(i*rowstride) + (j*4)];
      *((uint32_t *) pixel) = color;
    }
  }

  cairo_surface_mark_dirty_rectangle(surface, 0, 0, 64*SCALE, 64*SCALE);
  gtk_widget_queue_draw_area(drawing_area, 0, 0, 64*SCALE, 64*SCALE);
  
  return NULL;
}

int main(int argc, char ** argv) {
    if(argc > 1 && quickread_2bitmap(argv[1], buffer)) {
        quickwrite_2bitmap("out.bmp", buffer);
    }

    display_init(argc, argv);
    
    mainloop(NULL);

//    pthread_t worker_thread;
//    pthread_create(&worker_thread, NULL, mainloop, NULL);

    gtk_main();

//    pthread_join(worker_thread, NULL);
}
*/
