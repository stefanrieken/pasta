#include <string.h>
#include <pthread.h>

#include <gtk/gtk.h>

#include "pasta.h"
#include "bitmap.h"

extern void * mainloop(void * arg);

extern uint32_t palette[4];

typedef struct Sprite {
  uint8_t x;
  uint8_t y;
  uint8_t width;
  uint8_t height;
  uint16_t map; // location of map in memory. Must be 16 bit, otherwise granularity is way off
  uint8_t tiles; // location of tiles. Could be 8 bit to indicate a 1k (64 tile) aligned space
  uint8_t flags;
} Sprite;


static cairo_surface_t * surface = NULL;
static cairo_pattern_t * pattern;
GtkWidget * drawing_area;
#define SCALE 2

void delete_cb(GtkWidget *widget, GdkEventType *event, gpointer userdata) {
  // destroy any global drawing state here,
  // like the cairo surface (if we make it global)
  cairo_surface_destroy(surface);
  gtk_main_quit();
}

void configure_cb(GtkWidget * widget, GdkEventConfigure * event, gpointer data) {
  //GdkWindow * window = gtk_widget_get_window(widget);
  surface = cairo_image_surface_create(
    CAIRO_FORMAT_RGB24,
    gtk_widget_get_allocated_width (widget),
    gtk_widget_get_allocated_height (widget)
  );

  pattern = cairo_pattern_create_for_surface(surface);
  cairo_pattern_set_filter(pattern, CAIRO_FILTER_NEAREST);
}

void draw_screen_cb(GtkWidget *widget, cairo_t * cr, gpointer userdata) {
  cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
  cairo_scale(cr, SCALE, SCALE);
  cairo_set_source(cr, pattern);

  uint8_t * pixels = cairo_image_surface_get_data(surface);
  int rowstride = cairo_image_surface_get_stride(surface);

  // Draw out sprite memory
  // No need to clear screen when redrawing fully opaque background / character screen
  int spritemem = 12 * 1024; // just any location
  // 16 sprite structs of size 16 = 256 bytes
  for (int s =0; s<16; s++) {
    Sprite * sprite = (Sprite *) &memory[spritemem + (16 * s)];
    int transparent = sprite->flags & 0x0F;

    if (sprite->width != 0 && sprite->height != 0) {
      int width_map = (sprite->width + 7) / 8; // Even if width and height are not byte aligned, their map data is

      for (int i=0; i<sprite->height;i++) {
        // Try to get some (partial) calculations before the next for loop, to avoid repetition
        int map_idx_h = sprite->tiles + (i/8)*width_map;
        for (int j=0; j<sprite->width; j++) {
          uint8_t tile_idx = memory[sprite->map + map_idx_h + j/8];
          // Say tile idx = 50; i = 25; j = 30
          // Tile 50 starts at tiles + (50 / 8 tiles per line) * 16*8 bytes per tile + (50%8)*2 bytes
          // Also need to get to the right line of this tile for the current pixel; add (i%8) * 16 bytes per line
          // Then we need to pick out the right byte depending on the current bit written:
          uint8_t byte_idx = (j % 8) < 4 ? 0 : 1;
          uint8_t tile_data = memory[(20+sprite->tiles)*1024 + (tile_idx/8)*128+(i%8)*16 + ((tile_idx%8))*2 + byte_idx];
          int pxdata = (tile_data >> ((3-(j%4))*2)) & 0b11;

          uint8_t * pixel = &pixels[((sprite->y+i)*rowstride) + ((sprite->x+j)*4)];
          // TODO palette
          if (pxdata)
              *((uint32_t *) pixel) = palette[pxdata]; // TODO get palette into user mem registers
          else if (pxdata != transparent)
              *((uint32_t *) pixel) = 0;
        }
      }
    }
  }

  cairo_paint(cr);
}

void display_init(int argc, char ** argv) {

  gtk_init (&argc, &argv);

  GtkWindow * window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
  gtk_window_set_title(window, "tricolore on Cairo");
  gtk_window_set_default_size(window, 256*SCALE, 256*SCALE);
  gtk_window_set_resizable(window, FALSE);

  g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(delete_cb), NULL);

  drawing_area = gtk_drawing_area_new();
  gtk_widget_set_size_request(drawing_area, 256*SCALE, 256*SCALE);
  gtk_container_add(GTK_CONTAINER(window), drawing_area);

  g_signal_connect(G_OBJECT(drawing_area),"configure-event", G_CALLBACK (configure_cb), NULL);
  g_signal_connect(G_OBJECT(drawing_area), "draw", G_CALLBACK(draw_screen_cb), NULL);

  /*
  if (_event_cb != NULL) {
    gtk_widget_set_events(GTK_WIDGET(drawing_area),
      GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);

    g_signal_connect(drawing_area, "button-press-event", G_CALLBACK(pressed_cb), NULL);
    g_signal_connect(drawing_area, "button-release-event", G_CALLBACK(released_cb), NULL);
    g_signal_connect (G_OBJECT (drawing_area), "motion-notify-event",G_CALLBACK (motion_cb), NULL);
  }
  */

  gtk_widget_show_all(GTK_WIDGET(window));
}

void draw() {
  // Redraw full stcreen
  cairo_surface_mark_dirty_rectangle(surface, 0, 0, 32*8*SCALE, 32*8*SCALE);
  gtk_widget_queue_draw_area(drawing_area, 0, 0, 32*8*SCALE, 32*8*SCALE);
}

enum {
    PRIM_WRITE,
    PRIM_DRAW,
    PRIM_CATCHUP
};

uint16_t disp_prim_group_cb(uint8_t prim) {
    uint16_t n = peek(&argstack);
    uint16_t temp = 0, str = 0;
    uint16_t result = 0;

    switch(prim) {
        case PRIM_WRITE:
            temp = item(&argstack, n--); // target screen data location
            str = item(&argstack, n--);
            while(memory[str] != 0) {
              if(memory[str] == '\n') {
                temp = (temp / 32) * 32 + 31; // set cursor to new line (TODO does not compute end of screen!)
              } else {
                memory[temp] = memory[str];
              }
              temp++;
              str++;
            }
            result = temp; // return cursor position
            break;
        case PRIM_DRAW:
            draw();
            break;
        case PRIM_CATCHUP:
            usleep(10000);
            break;
    }

    if (n != 1) printf("WARN: %d leftover args\n", n);

    return result;
}

void register_display_prims() {
    uint8_t group = add_primitive_group(disp_prim_group_cb);

    add_variable("write", add_primitive(group | PRIM_WRITE));
    add_variable("draw", add_primitive(group | PRIM_DRAW));
    add_variable("catchup", add_primitive(group | PRIM_CATCHUP));
}

int main (int argc, char ** argv) {
    pasta_init();
    register_display_prims();

    // Fill ascii
    quickread_2bitmap("assets/ascii1.bmp", (char *) &memory[0x5000]);
    quickread_2bitmap("assets/ascii2.bmp", (char *) &memory[0x5400]);

    display_init(argc, argv);

    pthread_t worker_thread;
    pthread_create(&worker_thread, NULL, mainloop, &argv[1]);

    gtk_main();

    // pthread_join(worker_thread, NULL);
}
