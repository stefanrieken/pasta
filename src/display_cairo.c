#include <stdbool.h>

#include <gtk/gtk.h>

#include "pasta.h"
#include "tricolore.h"

#define SCALE 2

static cairo_surface_t * surface = NULL;
static cairo_pattern_t * pattern;
GtkWidget * drawing_area;

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

// Due to the nature of GTK, we end up housing the main function
// inside the display code
int main (int argc, char ** argv) {
    pasta_init();
    tricolore_init();
    display_init(argc, argv);

    pthread_t worker_thread;
    pthread_create(&worker_thread, NULL, mainloop, &argv[1]);

    gtk_main();

    pthread_join(worker_thread, NULL);
}
