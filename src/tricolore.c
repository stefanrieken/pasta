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
#define SPRITE_MEM 0x0200
#define PALETTE_MEM (SPRITE_MEM + 16*16)

#define TILE_MEM 0x6000

bool screen_active = false;

bool delete_cb(GtkWidget *widget, GdkEventType *event, gpointer userdata) {
  // destroy any global drawing state here,
  // like the cairo surface (if we make it global)
  gtk_main_quit();
  cairo_surface_destroy(surface);
  screen_active = false;
  return false;
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
Variable * pointer_x;
Variable * pointer_y;
Variable * click;

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

void motion_cb(GtkWidget *widget, GdkEventMotion * event, gpointer userdata) {
    pointer_x->value = event->x / SCALE;
    pointer_y->value = event->y / SCALE;
}

void button_cb(GtkWidget *widget, GdkEventButton * event, gpointer userdata) {
    // Presently only detecting left button
    if (event->type == GDK_BUTTON_PRESS) {
        click->value++; // User must clear
    } else if (event->type == GDK_BUTTON_RELEASE) {
        click->value = 0;
    }
}

void display_init(int argc, char ** argv) {

  gtk_init (&argc, &argv);

  GtkWindow * window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
  gtk_window_set_title(window, "tricolore on Cairo");
  gtk_window_set_default_size(window, SCREEN_WIDTH*8*SCALE, SCREEN_HEIGHT*8*SCALE);
  gtk_window_set_resizable(window, FALSE);

  drawing_area = gtk_drawing_area_new();
  gtk_widget_set_size_request(drawing_area, SCREEN_WIDTH*8*SCALE, SCREEN_HEIGHT*8*SCALE);
  gtk_container_add(GTK_CONTAINER(window), drawing_area);

  gtk_widget_add_events(GTK_WIDGET(window), GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
  gtk_widget_add_events(GTK_WIDGET(drawing_area), GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

  g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(delete_cb), NULL);

  g_signal_connect(G_OBJECT(window), "key-press-event", G_CALLBACK (key_press_cb), NULL);
  g_signal_connect(G_OBJECT(window), "key-release-event", G_CALLBACK (key_release_cb), NULL);
  g_signal_connect(G_OBJECT(drawing_area), "motion-notify-event", G_CALLBACK (motion_cb), NULL);
  g_signal_connect(G_OBJECT(drawing_area), "button-press-event", G_CALLBACK (button_cb), NULL);
  g_signal_connect(G_OBJECT(drawing_area), "button-release-event", G_CALLBACK (button_cb), NULL);

  g_signal_connect(G_OBJECT(drawing_area),"configure-event", G_CALLBACK (configure_cb), NULL);
  g_signal_connect(G_OBJECT(drawing_area), "draw", G_CALLBACK(draw_screen_cb), NULL);

  gtk_widget_show_all(GTK_WIDGET(window));
}

// TODO: thought we already were cramped for space here, but we only use 10 of the 16 bytes available!
typedef struct Sprite {
  uint8_t x;
  uint8_t y;
  uint8_t width;
  uint8_t height;
  uint16_t map; // location of map in memory. Must be 16 bit, otherwise granularity is way off
  uint8_t mode; // 6-bit (MSB) location of tiles (presently offset from 24k = 0x6000) + 2-bit mode
  uint8_t flags; // msb->lsb: scaley (2b) scalex (2b), transparent (4b);
  uint16_t colors; // 4x index into 4-bit color palette
} Sprite;

// Draw out sprite memory
void draw(int from_x, int from_y, int width, int height) {
  uint8_t * pixels = cairo_image_surface_get_data(surface);
  int rowstride = cairo_image_surface_get_stride(surface);

  // May not need to clear screen when redrawing full background / character screen

  uint32_t * palette = (uint32_t *) &memory[PALETTE_MEM];

  // 16 sprite structs of size 16 = 256 bytes
  for (int s =0; s<16; s++) {
    Sprite * sprite = (Sprite *) &memory[SPRITE_MEM + (16 * s)];
    int transparent = sprite->flags & 0x0F;
    int scalex = ((sprite->flags >> 4) & 0b11)+1; // Can scale 2,3,4 times; maybe rather 2,4,8?
    int scaley = ((sprite->flags >> 6) & 0b11)+1;
    if (sprite->width != 0 && sprite->height != 0) {
      int width_map = sprite->width; //(sprite->width + 7) / 8; // Even if width and height are not byte aligned, their map data is

      for (int i=0; i<sprite->height*8*scaley;i++) {
        if (sprite->y+i < from_y || sprite->y+i >from_y+height) continue; // Some attempt to skip parts that don't need drawing

        // Try to get some (partial) calculations before the next for loop, to avoid repetition
        int map_idx_h = (i/(8*scaley))*width_map;
        for (int j=0; j<sprite->width*8*scalex; j++) {
          if (sprite->x+j < from_x || sprite->x+j >from_x+width) continue; // Some attempt to skip parts that don't need drawing

          // Normal mode: 8-bit tiles
          uint8_t tile_idx = memory[sprite->map + map_idx_h + j/(8*scalex)];
          uint8_t colormask = 0;

          switch (sprite->mode & 0b11) {
              case 1: // Invertible mode: 7-bit tile addressing with 1-bit inverse marker
                  colormask = (tile_idx & (1 << 7)) ? 0b01 : 0b00; // Only invert the MSB
                  tile_idx &= 0b01111111;
                  break;
              case 2: // Colormask mode: 6-bit tile addressing with 2-bit XOR color mask; this allows for different colorings of the same tiles (within the given 4 colors)
                  colormask = tile_idx >> 6;
                  tile_idx &= 0b111111;
                  break;
              case 3: // TBD (proposal: sprite struct selection mode, to allow for more than 4 colors in a game map by letting different sprites draw different parts)
                  break;
          }

          // Say tile idx = 50; i = 25; j = 30
          // Tile 50 starts at tiles + (50 / 8 tiles per line) * 16*8 bytes per tile + (50%8)*2 bytes
          // Also need to get to the right line of this tile for the current pixel; add (i%8) * 16 bytes per line
          // Then we need to pick out the right byte depending on the current bit written:
          uint8_t byte_idx = ((j/scalex) % 8) < 4 ? 0 : 1;
          uint8_t tile_data = memory[TILE_MEM + (sprite->mode & 0b11111100)*1024 + (tile_idx/8)*128+((i/scaley)%8)*16 + ((tile_idx%8))*2 + byte_idx];


          int pxdata = (tile_data >> ((3-((j/scalex)%4))*2)) & 0b11;

          int pixel_idx = (((sprite->y+i)%256)*rowstride) + (((sprite->x+j)%256)*4); // The %256 rotates pixels on a 32x8 wide display
          uint8_t * pixel = &pixels[pixel_idx];
          if (!(transparent & (1 << pxdata))) {
              int color = (sprite->colors >> ((pxdata ^ colormask) *4)) & 0b1111;
              *((uint32_t *) pixel) = palette[color];
          }
        }
      }
    }
  }

  // Redraw selected area (NOTE: we actually still ignored the area selection in the sprite drawing just before!)
//  cairo_surface_mark_dirty(surface);
  cairo_surface_mark_dirty_rectangle(surface, from_x*SCALE, from_y*SCALE, width*SCALE, height*SCALE);
//  gtk_widget_queue_draw_area(drawing_area, 0, 0, 256*SCALE, 256*SCALE);
  gtk_widget_queue_draw_area(drawing_area, from_x*SCALE, from_y*SCALE, width*SCALE, height*SCALE);
}

enum {
    PRIM_WRITE,
    PRIM_DRAW,
    PRIM_CATCHUP,
    PRIM_SAVE_SHEET
};

uint16_t disp_prim_group_cb(uint8_t prim) {
    uint16_t n = peek(&argstack);
    uint16_t temp = 0, str = 0;
    uint16_t result = 0;

    uint16_t from_x = 0, from_y = 0, to_x = 255, to_y = 255;

    switch(prim) {
        case PRIM_WRITE:
            temp = item(&argstack, n--); // target screen data location
            uint8_t inverse = 0;
            while (n > 1) {
              str = item(&argstack, n--);
              while(memory[str] != 0) {
                if(memory[str] == '\n') {
                  temp = (temp / SCREEN_WIDTH) * SCREEN_WIDTH + (SCREEN_WIDTH); // set cursor to new line (TODO does not compute end of screen!)
                } else if (memory[str] == 0x0f) { // 'shift out': used for inverse (may also be used to end reverse)
                  inverse ^= 0b10000000;
                } else if (memory[str] == 0x10) { // 'shift in': used to end reverse
                  inverse = 0;
                } else {
                  memory[temp] = memory[str] | inverse;
                  temp++;
                }
                str++;
              }
            }
            result = temp; // return cursor position
            break;
        case PRIM_DRAW:
            if (n > 1) from_x = item(&argstack, n--);
            if (n > 1) from_y = item(&argstack, n--);
            if (n > 1) to_x = item(&argstack, n--);
            if (n > 1) to_y = item(&argstack, n--);
            draw(from_x, from_y, to_x, to_y);
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
            result = screen_active;
            break;
        case PRIM_SAVE_SHEET:
            // Utility function specific to the editor
            temp = item(&argstack, n--);
            quickwrite_2bitmap("out.bmp", &memory[temp], (uint32_t *) &memory[item(&argstack, n--)]);
            break;
    }

    if (n != 1) printf("WARN: %d leftover args\n", n);

    return result;
}

void register_display_prims() {
    uint8_t group = add_primitive_group(disp_prim_group_cb);

    direction = add_variable("direction", 0);
    pointer_x = add_variable("pointer_x", 0);
    pointer_y = add_variable("pointer_y", 0);
    click = add_variable("click", 0);

    add_variable("write", add_primitive(group | PRIM_WRITE));
    add_variable("draw", add_primitive(group | PRIM_DRAW));
    add_variable("catchup", add_primitive(group | PRIM_CATCHUP));
    add_variable("save_sheet", add_primitive(group | PRIM_SAVE_SHEET));

    clock_gettime(CLOCK_REALTIME, &last_time);
}

int main (int argc, char ** argv) {
    pasta_init();
    register_display_prims();

    // Fill ascii
    quickread_2bitmap("assets/ascii1.bmp", &memory[TILE_MEM], (uint32_t *) &memory[PALETTE_MEM]);
    quickread_2bitmap("assets/ascii2.bmp", &memory[TILE_MEM+0x0400], (uint32_t *) &memory[PALETTE_MEM]);

    display_init(argc, argv);

    // Load Pasta + Tricolore libs
    FILE * infile;
    if ((infile = fopen("recipes/lib.pasta", "r"))) {
        parse(infile, true);
        fclose(infile);
    }
    if ((infile = fopen("recipes/lib.trico", "r"))) {
        parse(infile, true);
        fclose(infile);
    }

    screen_active = true; // well, almost

    pthread_t worker_thread;
    pthread_create(&worker_thread, NULL, mainloop, &argv[1]);

    gtk_main();

    pthread_join(worker_thread, NULL);
}
