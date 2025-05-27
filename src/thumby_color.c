uint16_t * pixels;

void redraw(int from_x, int from_y, int width, int height) {
  // Redraw selected area (NOTE: we actually still ignored the area selection in the sprite drawing just before!)
//  cairo_surface_mark_dirty(surface);
  cairo_surface_mark_dirty_rectangle(surface, from_x*SCALE, from_y*SCALE, width*SCALE, height*SCALE);
//  gtk_widget_queue_draw_area(drawing_area, 0, 0, 256*SCALE, 256*SCALE);
  gtk_widget_queue_draw_area(drawing_area, from_x*SCALE, from_y*SCALE, width*SCALE, height*SCALE);

  pixels = cairo_image_surface_get_data(surface); // probably want to renew this after every commplete draw
}

// Source color is 32-bit RGB(a), order ??
// Target color is r, g, b (msb -> lsb),
// likely packed into 5, 6, 5 bits

// Keep 5-6 most significant bits of each color
#define MASK_R (0b11111000)
#define MASK_G (0b11111100 << 8)
#define MASK_B (0b11111000 << 16)

// ........ ........ ........
// rrrrr000 gggggg00 bbbbb000
//     |         |       |
//     `--------.`-----. `--.
//              |      |    |
//              v      v    v
//          rrrrrggg gggbbbbb

void set_pixel(int x, int y, uint32_t color) {
  uint16_t pixel = ((color & MASK_R) >> 8) | ((color & MASK_G) >> 5) | ((color & MASK_B) >> 3);
  pixels[y * 128 + x] = pixel;
}

void redraw(int from_x, int from_y, int width, int height) {
    engine_display_gc9107_update(buffer);
}

int main (int argc, char ** argv) {
    pasta_init();
    tricolore_init();
    display_init(argc, argv);

    pixels = malloc(128 * 128 * sizeof(uint16_t));
    mainloop(&argv[1]);
}
