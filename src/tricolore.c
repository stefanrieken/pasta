#include <string.h>
#include <pthread.h>
#include <stdio.h>

#include <gtk/gtk.h>

#include "pasta.h"
#include "bitmap.h"

#define SCREEN_WIDTH 32
#define SCREEN_HEIGHT 32

extern void * mainloop(void * arg);

static cairo_surface_t * surface = NULL;
static cairo_pattern_t * pattern;
GtkWidget * drawing_area;
#define SCALE 2

// palette register is placed after 16 sprites
#define PALETTE_MEM 12 * 1024 + 16*16

void delete_cb(GtkWidget *widget, GdkEventType *event, gpointer userdata) {
  // destroy any global drawing state here,
  // like the cairo surface (if we make it global)
  gtk_main_quit();
  cairo_surface_destroy(surface);
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

  cairo_paint(cr);
}

Variable * direction;

struct timespec last_time;
struct timespec this_time;

// We assign the horizontal and vertical planes both their own 2-bit groups,
// so that they can be activated in combination.
// As for left <-> right / up <-> down, we could assign each their own bit as well,
// but alternatively we can say bit 1 = active plane; bit 2 = direction.
// This way, we can derive a signed direction using number - 2.
#define DIR_LEFT   0b0100
#define DIR_RIGHT  0b1100
#define DIR_UP     0b0001
#define DIR_DOWN   0b0011
#define PLANE_HOR  0b1100
#define PLANE_VERT 0b0011

void key_press_cb(GtkWidget *widget, GdkEventKey * event, gpointer userdata) {
    switch (event->keyval) {
        case (GDK_KEY_Left):
            direction->value = ((direction->value & PLANE_VERT) | DIR_LEFT);
            break;
        case (GDK_KEY_Up):
            direction->value = ((direction->value & PLANE_HOR) | DIR_UP);
            break;
        case (GDK_KEY_Right):
            direction->value = ((direction->value & PLANE_VERT) | DIR_RIGHT);
            break;
        case (GDK_KEY_Down):
            direction->value = ((direction->value & PLANE_HOR) | DIR_DOWN);
            break;
        case (GDK_KEY_Return):
            printf("Return key\n");
            break;
        case (GDK_KEY_BackSpace):
            printf("Backspace key\n");
            break;
        case (GDK_KEY_Escape):
            printf("Escape key\n");
            break;
        case (GDK_KEY_Tab):
            printf("Escape key\n");
            break;
        default:
            if (event->keyval < 0xff00) {
              // Filters out any modifiers etc. that we do not explicitly catch,
              // leaving mainly basic ascii, and perhaps a few special chars
              printf("Key press: %c (%x)\n", event->keyval, event->keyval);
            }
            break;
    }
}

void key_release_cb(GtkWidget *widget, GdkEventKey * event, gpointer userdata) {
    switch (event->keyval) {
        case (GDK_KEY_Left):
        case (GDK_KEY_Right):
            direction->value = direction->value & PLANE_VERT;
            break;
        case (GDK_KEY_Up):
        case (GDK_KEY_Down):
            direction->value = direction->value & PLANE_HOR;
            break;
        default:
            direction->value = 0;
            break;
    }
}

void display_init(int argc, char ** argv) {

  gtk_init (&argc, &argv);

  GtkWindow * window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
  gtk_window_set_title(window, "tricolore on Cairo");
  gtk_window_set_default_size(window, SCREEN_WIDTH*8*SCALE, SCREEN_HEIGHT*8*SCALE);
  gtk_window_set_resizable(window, FALSE);

  gtk_widget_add_events(GTK_WIDGET(window), GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

  g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(delete_cb), NULL);
  g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(delete_cb), NULL);
  g_signal_connect(G_OBJECT(window), "key-press-event", G_CALLBACK (key_press_cb), NULL);
  g_signal_connect(G_OBJECT(window), "key-release-event", G_CALLBACK (key_release_cb), NULL);

  drawing_area = gtk_drawing_area_new();
  gtk_widget_set_size_request(drawing_area, SCREEN_WIDTH*8*SCALE, SCREEN_HEIGHT*8*SCALE);
  gtk_container_add(GTK_CONTAINER(window), drawing_area);

  g_signal_connect(G_OBJECT(drawing_area),"configure-event", G_CALLBACK (configure_cb), NULL);
  g_signal_connect(G_OBJECT(drawing_area), "draw", G_CALLBACK(draw_screen_cb), NULL);

  gtk_widget_show_all(GTK_WIDGET(window));
}

typedef struct Sprite {
  uint8_t x;
  uint8_t y;
  uint8_t width;
  uint8_t height;
  uint16_t map; // location of map in memory. Must be 16 bit, otherwise granularity is way off
  uint8_t tiles; // location of tiles. Could be 8 bit to indicate a 1k (64 tile) aligned space
  uint8_t flags;
  uint16_t colors; // 4x index into 4-bit color palette
} Sprite;

// Draw out sprite memory
void draw() {
  uint8_t * pixels = cairo_image_surface_get_data(surface);
  int rowstride = cairo_image_surface_get_stride(surface);

  // May not need to clear screen when redrawing full background / character screen

  int spritemem = 12 * 1024; // just any location
  uint32_t * palette = (uint32_t *) &memory[PALETTE_MEM];

  // 16 sprite structs of size 16 = 256 bytes
  for (int s =0; s<16; s++) {
    Sprite * sprite = (Sprite *) &memory[spritemem + (16 * s)];
    int transparent = sprite->flags & 0x0F;

    if (sprite->width != 0 && sprite->height != 0) {
      int width_map = sprite->width; //(sprite->width + 7) / 8; // Even if width and height are not byte aligned, their map data is

      for (int i=0; i<sprite->height*8;i++) {
        // Try to get some (partial) calculations before the next for loop, to avoid repetition
        int map_idx_h = sprite->tiles + (i/8)*width_map;
        for (int j=0; j<sprite->width*8; j++) {
          //if(sprite->x+j > 255) { continue; }
          uint8_t tile_idx = memory[sprite->map + map_idx_h + j/8];
          // Say tile idx = 50; i = 25; j = 30
          // Tile 50 starts at tiles + (50 / 8 tiles per line) * 16*8 bytes per tile + (50%8)*2 bytes
          // Also need to get to the right line of this tile for the current pixel; add (i%8) * 16 bytes per line
          // Then we need to pick out the right byte depending on the current bit written:
          uint8_t byte_idx = (j % 8) < 4 ? 0 : 1;
          uint8_t tile_data = memory[(20+sprite->tiles)*1024 + (tile_idx/8)*128+(i%8)*16 + ((tile_idx%8))*2 + byte_idx];
          int pxdata = (tile_data >> ((3-(j%4))*2)) & 0b11;

          int pixel_idx = (((sprite->y+i)%256)*rowstride) + (((sprite->x+j)%256)*4); // The %256 rotates pixels on a 32x8 wide display
          uint8_t * pixel = &pixels[pixel_idx];
          // TODO palette
          if (!(transparent & (1 << pxdata))) {
              int color = (sprite->colors >> (pxdata*4)) & 0b1111;
              *((uint32_t *) pixel) = palette[color]; // TODO get palette into user mem registers
          }
        }
      }
    }
  }

  // Redraw full stcreen
  cairo_surface_mark_dirty_rectangle(surface, 0, 0, SCREEN_WIDTH*8*SCALE, SCREEN_WIDTH*8*SCALE);
  gtk_widget_queue_draw_area(drawing_area, 0, 0, SCREEN_HEIGHT*8*SCALE, SCREEN_HEIGHT*8*SCALE);
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
                temp = (temp / SCREEN_WIDTH) * SCREEN_WIDTH + (SCREEN_WIDTH-1); // set cursor to new line (TODO does not compute end of screen!)
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
            // Make sure we don't exceed refresh rate, but if we lagged, we actually won't catch up on that
            // Dividing nsecs by 16 * 1024 * 1024 (== shift 24 bits) yields a refresh rate of ~59.6 Hz
            // The calculation should be cheap on any hardware, if the hardware provision of the nsecs value is so too
            // Of course we can always allow for multiples of this amount (e.g. shift 23 bits) with a similar performance
            do {
                usleep(10000);
                clock_gettime(CLOCK_REALTIME, &this_time);
            } while(this_time.tv_sec == last_time.tv_sec && (this_time.tv_nsec >> 24) == (last_time.tv_nsec >> 24));
            last_time = this_time;
            break;
    }

    if (n != 1) printf("WARN: %d leftover args\n", n);

    return result;
}

void register_display_prims() {
    uint8_t group = add_primitive_group(disp_prim_group_cb);

    direction = add_variable("direction", 0);

    add_variable("write", add_primitive(group | PRIM_WRITE));
    add_variable("draw", add_primitive(group | PRIM_DRAW));
    add_variable("catchup", add_primitive(group | PRIM_CATCHUP));

    clock_gettime(CLOCK_REALTIME, &last_time);
}

int main (int argc, char ** argv) {
    pasta_init();
    register_display_prims();

    // Fill ascii
    quickread_2bitmap("assets/ascii1.bmp", (char *) &memory[0x5000], (uint32_t *) &memory[PALETTE_MEM]);
    quickread_2bitmap("assets/ascii2.bmp", (char *) &memory[0x5400], (uint32_t *) &memory[PALETTE_MEM]);

    display_init(argc, argv);

    // Load Tricolore lib
    FILE * infile;
    if ((infile = fopen("recipes/lib.trico", "r"))) {
        parse(infile, true);
        fclose(infile);
    }

    pthread_t worker_thread;
    pthread_create(&worker_thread, NULL, mainloop, &argv[1]);

    gtk_main();

    // pthread_join(worker_thread, NULL);
}
