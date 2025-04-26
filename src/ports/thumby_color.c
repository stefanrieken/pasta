#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#include "../pasta.h"
#include "../tricolore.h"

int SCREEN_WIDTH=16; // retained caps from earlier define
int SCREEN_HEIGHT=16;

extern void engine_display_gc9107_update(uint16_t *screen_buffer_to_render);
extern void engine_display_gc9107_init();

uint16_t * pixels;

// Source color is 32-bit RGB(a), order (a), r, g, b (msb -> lsb)
// Target color is r, g, b (msb -> lsb),
// packed into 5, 6, 5 bits

// Keep 5-6 most significant bits of each color
#define MASK_R (0b11111000 << 16)
#define MASK_G (0b11111100 << 8)
#define MASK_B (0b11111000)

// ........ ........ ........
// rrrrr000 gggggg00 bbbbb000
//     |         |       |
//     `--------.`-----. `--.
//              |      |    |
//              v      v    v
//          rrrrrggg gggbbbbb

void set_pixel(int x, int y, uint32_t color) {

  // Prevent problems from assuming a 256x256 screen...
  if (x > 127 || y > 127) return;

  uint16_t pixel = ((color & MASK_R) >> 8) | ((color & MASK_G) >> 5) | ((color & MASK_B) >> 3);
  pixels[y * 128 + x] = pixel;
}

void redraw(int from_x, int from_y, int width, int height) {
    engine_display_gc9107_update(pixels);
}

// Thumby Color requires a slightly different approach to sound than Thumby
// speaker pwm = gpio23
uint slice_num;

#define AUDIO_ENABLE_PIN 20
#define AUDIO_PWM_PIN 23

void beep(int frequency, int duration) {
  if (frequency == 0) { sleep_ms(duration + (duration / 2)); return; }

  pwm_config cfg = pwm_get_default_config();
  pwm_set_chan_level(slice_num,PWM_CHAN_A,0);
  pwm_set_enabled(slice_num, false);

  float div = (float)clock_get_hz(clk_sys) / (frequency * 10000);

  pwm_config_set_clkdiv(&cfg, div);
  pwm_config_set_wrap(&cfg, 10000);

  pwm_init(slice_num, &cfg, true);
  pwm_set_gpio_level(AUDIO_PWM_PIN, 5000);

  pwm_set_enabled(slice_num, true);
  sleep_ms(duration);
  pwm_set_enabled(slice_num, false);
  sleep_ms(duration / 2);
}

#define BTN_DPAD_L 0
#define BTN_DPAD_U 1
#define BTN_DPAD_R 2
#define BTN_DPAD_D 3
#define BTN_B 21
#define BTN_A 25

void setup_button(int gpio) {
    gpio_init(gpio);
    gpio_pull_up(gpio);
    gpio_set_dir(gpio, GPIO_IN);
}

void get_button_state() {
    direction->value = 0;
    // active low
    if (!gpio_get(BTN_DPAD_L)) direction->value = ((direction->value & PLANE_VERT) | DIR_LEFT);
    if (!gpio_get(BTN_DPAD_U)) direction->value = ((direction->value & PLANE_HOR) | DIR_UP);
    if (!gpio_get(BTN_DPAD_R)) direction->value = ((direction->value & PLANE_VERT) | DIR_RIGHT);
    if (!gpio_get(BTN_DPAD_D)) direction->value = ((direction->value & PLANE_HOR) | DIR_DOWN);

    if (!gpio_get(BTN_A)) beep(440, 100);
    if (!gpio_get(BTN_B)) beep(660, 100);
}

// These are linker symbols. Rather than pointing to memory containing a value,
// their 'address' itself is the value. Their type is kind of bogus, but makes
// the C compiler happy.
extern void * _binary_recipes_lib_pasta_start;
extern void * _binary_recipes_lib_pasta_size;
extern void * _binary_recipes_lib_trico_start;
extern void * _binary_recipes_lib_trico_size;
extern void * _binary_assets_ascii1_bmp_start;
extern void * _binary_assets_ascii1_bmp_size;
extern void * _binary_assets_ascii2_bmp_start;
extern void * _binary_assets_ascii2_bmp_size;

// Opens only known, linked files
FILE * open_file (const char * filename, const char * mode) {
    if (strcmp("recipes/lib.pasta", filename) == 0) {
        return fmemopen((void *) &_binary_recipes_lib_pasta_start, (ptrdiff_t) &_binary_recipes_lib_pasta_size, mode);
    }
    if (strcmp("recipes/lib.trico", filename) == 0) {
        return fmemopen((void *) &_binary_recipes_lib_trico_start, (ptrdiff_t) &_binary_recipes_lib_trico_size, mode);
    }
    if (strcmp("assets/ascii1.bmp", filename) == 0) {
        return fmemopen((void *) &_binary_assets_ascii1_bmp_start, (ptrdiff_t) &_binary_assets_ascii1_bmp_size, mode);
    }
    if (strcmp("assets/ascii2.bmp", filename) == 0) {
        return fmemopen((void *) &_binary_assets_ascii2_bmp_start, (ptrdiff_t) &_binary_assets_ascii2_bmp_size, mode);
    }
    return NULL;
}

void thumby_color_init() {
    gpio_init(AUDIO_ENABLE_PIN);
    gpio_set_dir(AUDIO_ENABLE_PIN, GPIO_OUT);
    gpio_put(AUDIO_ENABLE_PIN, 0);

    engine_display_gc9107_init();

    pixels = malloc(128 * 128 * sizeof(uint16_t));

    // write a little demo gradient
    for (int i=0; i<128*128; i++) {
      // It just so happens that 128*128 = 2^14; keep most significant bits

      // blue
      pixels[i]=i>> 9;
      // green
      //pixels[i]=(i >> 9) << 6;
      // red
      //pixels[i]=(i >> 9) << 12;
    }
    engine_display_gc9107_update(pixels);

    // init buzzer PWM = gpio 28 (thumby) / gpio 23 (thumby color)
    slice_num = pwm_gpio_to_slice_num(AUDIO_PWM_PIN);
    gpio_set_function(AUDIO_PWM_PIN, GPIO_FUNC_PWM);

    pwm_config audio_pwm_pin_config = pwm_get_default_config();
    pwm_config_set_clkdiv_int(&audio_pwm_pin_config, 1);
    pwm_config_set_wrap(&audio_pwm_pin_config, 512);   // 150MHz / 512 = 292kHz
    pwm_init(slice_num, &audio_pwm_pin_config, true);

    gpio_put(AUDIO_ENABLE_PIN, 1);

    setup_button(BTN_DPAD_L);
    setup_button(BTN_DPAD_U);
    setup_button(BTN_DPAD_R);
    setup_button(BTN_DPAD_D);
    setup_button(BTN_A);
    setup_button(BTN_B);

    // Play a starting melody :)
    beep(440, 100);
    beep(660, 100);

    getchar(); // await first input on serial
}

int main (int argc, char ** argv) {
    stdio_init_all();
    thumby_color_init();
    pasta_init();
    tricolore_init();

    mainloop(NULL);
}
