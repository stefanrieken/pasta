#include <stdbool.h>
#include <poll.h>
#include <gtk/gtk.h>

#include "../vars.h"
#include "../pasta.h"
#include "../tricolore.h"

// This value refers to Cairo's 'zoom level' only
#define SCALE 2

// These values refer to our internal idea of a max resolution,
// and are consulted by Tricolore's 'draw' routine.
int SCREEN_WIDTH=32;
int SCREEN_HEIGHT=32;

static cairo_surface_t * surface = NULL;
static cairo_pattern_t * pattern;
GtkWidget * drawing_area;

uint8_t * pixels;
int rowstride;

struct pollfd poll_stdin;

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

extern int shift; // see tricolore.c . To be further refined
void motion_cb(GtkWidget *widget, GdkEventMotion * event, gpointer userdata) {
    pointer_x->value = (((uint16_t) event->x) >> shift) / SCALE;
    pointer_y->value = (((uint16_t) event->y) >> shift) / SCALE;
}

void button_cb(GtkWidget *widget, GdkEventButton * event, gpointer userdata) {
    // Presently only detecting left button
    if (event->type == GDK_BUTTON_PRESS) {
        click->value++; // User must clear
    } else if (event->type == GDK_BUTTON_RELEASE) {
        click->value = 0;
    }
}

bool update_inputs() {
    // Input states are updated by the callbacks above

    // Only thing left is to check for activity on stdin
    // TODO it would be nice NOT to break on stdin if we
    // can detect that no "-" argument was given, as it
    // practically freezes the screen without offering a
    // command line as an alternative.
    if (do_repl && poll(&poll_stdin, 1, 0) != 0) {
        getchar(); // dismiss the character that breaks the run loop
        return false; // make any screen ("catchup") loop fall through so as to continue to REPL
    }
    return true;
}

void display_init(int argc, char ** argv) {

  gtk_init (&argc, &argv);

  GtkWindow * window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
  gtk_window_set_title(window, "tricolore on Cairo");
  gtk_window_set_default_size(window, SCREEN_WIDTH*8*SCALE, SCREEN_HEIGHT*8*SCALE);
  gtk_window_set_resizable(window, FALSE);
//  gtk_window_set_hide_on_close(window, TRUE);

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

  rowstride = cairo_image_surface_get_stride(surface);
  pixels = cairo_image_surface_get_data(surface); // probably want to renew this after every commplete draw

  poll_stdin.fd = fileno(stdin);
  poll_stdin.events = POLLIN;
}

void beep (int frequency_hz, int duration_ms) {
    // alas, no beep for this port
}
void redraw(int from_x, int from_y, int width, int height) {
  // Redraw selected area (NOTE: we actually still ignored the area selection in the sprite drawing just before!)
//  cairo_surface_mark_dirty(surface);
  cairo_surface_mark_dirty_rectangle(surface, from_x*SCALE, from_y*SCALE, width*SCALE, height*SCALE);
//  gtk_widget_queue_draw_area(drawing_area, 0, 0, 256*SCALE, 256*SCALE);
  gtk_widget_queue_draw_area(drawing_area, from_x*SCALE, from_y*SCALE, width*SCALE, height*SCALE);

  pixels = cairo_image_surface_get_data(surface); // probably want to renew this after every commplete draw
}

void set_pixel(int x, int y, uint32_t color) {
  int pixel_idx = (y*rowstride) + (x*4);
  uint8_t * pixel = &pixels[pixel_idx];
  *((uint32_t *) pixel) = color;
}

FILE * open_file (const char * filename, const char * mode) {
  return fopen(filename, mode);
}

// Entry point for the GTK / Cairo port
int main (int argc, char ** argv) {
    pasta_init();
    tricolore_init();
    display_init(argc, argv);

    pthread_t worker_thread;
    pthread_create(&worker_thread, NULL, mainloop, &argv[1]);

    gtk_main();

    pthread_join(worker_thread, NULL);
}
