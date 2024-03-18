#include <pthread.h>

#include <gtk/gtk.h>

#include "selfless.h"
#include "bitmap.h"

extern void * mainloop(void * arg);

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

extern uint32_t palette[4];

// This does not work the screen like envisioned,
// rather it demonstrates that we can add screen primitives in general
void demo() {
  uint8_t * buffer = &memory[0x5000];

  quickread_2bitmap("assets/ascii2.bmp", (char *) buffer);
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
}

enum {
    PRIM_DEMO
};

uint16_t disp_prim_group_cb(uint8_t prim) {
//    uint16_t n = peek(&argstack);
    uint16_t result = 0;

    switch(prim) {
        case PRIM_DEMO:
            demo();
            break;
    }

    return result;
}

void register_display_prims() {
    uint8_t group = add_primitive_group(disp_prim_group_cb);

    add_variable("demo", add_primitive(group | PRIM_DEMO));
}

int main (int argc, char ** argv) {
    pasta_init();
    register_display_prims();

    pthread_t worker_thread;
    pthread_create(&worker_thread, NULL, mainloop, &argv[1]);

    display_init(argc, argv);
    gtk_main();

    pthread_join(worker_thread, NULL);
}
