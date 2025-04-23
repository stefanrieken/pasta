#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#include "../pasta.h"
#include "../ssd1306.h"

#include "../ascii1.xbm"
#include "../ascii2.xbm"

#ifdef PICO_RP2350
#define AUDIO_ENABLE_PIN 20
#define AUDIO_PWM_PIN 23
#else
#define AUDIO_PWM_PIN 28
#endif

// We don't currently use tiles,
// but default the display frame buffer to the same location
#define TILE_MEM 0x6000

#define SCREEN_WIDTH_BYTES 9
// Thumby primitive defs
enum {
    PRIM_BEEP,
    PRIM_WRITE,
    PRIM_CLS
};

// speaker pwm = gpio28
uint slice_num;
void beep(int frequency, int duration) {
  if (frequency == 0) { sleep_ms(duration + (duration / 2)); return; }

  pwm_config cfg = pwm_get_default_config();
  pwm_set_chan_level(slice_num,PWM_CHAN_A,0);
  pwm_set_enabled(slice_num, false);

  // This calculation copied verbatim from "Simple Sound with C"
  // https://forums.raspberrypi.com/viewtopic.php?t=310320
  uint count = 125000000 * 16 / frequency;
  uint div =  count / 60000;  // to be lower than 65535*15/16 (rounding error)
  if(div < 16)
      div=16;
  cfg.div = div;
  cfg.top = count / div;

  pwm_init(slice_num, &cfg, true);
  pwm_set_chan_level(slice_num,PWM_CHAN_A,cfg.top / 2);

  pwm_set_enabled(slice_num, true);
  sleep_ms(duration);
  pwm_set_enabled(slice_num, false);
  sleep_ms(duration / 2);
}

// As the screen is arranged in column bytes,
// the input xbms are rotated to match.
// The math does become a bit fiddly because of this.
void write_char(int ch, unsigned char * framebuffer, int x, int y) {
  unsigned char * src = ascii1_bits;
  if (ch >= 64) { src = ascii2_bits; ch -= 64; }
  
  for (int i=0; i<8;i++) {
    unsigned char data = src[((8-(ch % 8))*64 - (i+1)*8) + (ch / 8)];
    framebuffer[(y*SCREEN_WIDTH_BYTES) +x+i] = ~data;
  }
}

void write_string(char * string, unsigned char * framebuffer, int x, int y) {
  for (int i=0; i<strlen(string);i++) write_char(string[i], framebuffer, x+(i*8), y);
}

void cls() {
    write_string("         ", &memory[TILE_MEM], 0, 0);
    write_string("         ", &memory[TILE_MEM], 0, 8);
    write_string("         ", &memory[TILE_MEM], 0, 16);
    write_string("         ", &memory[TILE_MEM], 0, 24);
    display_write_buffer(&memory[TILE_MEM], 72*40 / 8);
}

uint16_t thumby_prim_group_cb(uint8_t prim) {
    uint16_t n = peek(&argstack);

    int frequency = 440;
    int duration = 500;
    
    int result = 0;

    switch(prim) {
        case PRIM_BEEP:
            if(n > 1) frequency = item(&argstack, n--);
            if(n > 1) duration  = item(&argstack, n--);
            beep(frequency, duration);
            break;
        case PRIM_WRITE:;
            if (n < 2) return 0;
            int str = item(&argstack, n--);
            int x = (n > 1) ? item(&argstack, n--) : 0;
            int y = (n > 1) ? item(&argstack, n--) : 0;
            write_string(&memory[str], &memory[TILE_MEM], x, y);
            display_write_buffer(&memory[TILE_MEM], 72*40 / 8);
            break;
        case PRIM_CLS:;
            cls();
            break;
    }

    return result;
}

void register_thumby_prims() {
    display_init();
    display_set_brightness(120);
    write_string("PASTA V1", &memory[TILE_MEM], 0, 0); 
    write_string("READY.", &memory[TILE_MEM], 0, 16); 
    write_string(">", &memory[TILE_MEM], 0, 24); 
    display_write_buffer(&memory[TILE_MEM], 72*40 / 8);

    // init buzzer PWM = gpio 28 (thumby) / gpio 23 (thumby color)
    slice_num = pwm_gpio_to_slice_num(AUDIO_PWM_PIN);
    gpio_set_function(AUDIO_PWM_PIN, GPIO_FUNC_PWM);

    // Play a starting melody :)
    beep(440, 100);
    beep(660, 100);

    getchar(); // await first input on serial

    cls();

    uint8_t group = add_primitive_group(thumby_prim_group_cb);
    add_variable("beep", add_primitive(group | PRIM_BEEP));
    add_variable("write", add_primitive(group | PRIM_WRITE));
    add_variable("cls", add_primitive(group | PRIM_CLS));
}


// These are linker symbols. Rather than pointing to memory containing a value,
// their 'address' itself is the value. Their type is kind of bogus, but makes
// the C compiler happy.
extern void * _binary_recipes_lib_pasta_start;
extern void * _binary_recipes_lib_pasta_size;

// Opens only known, linked files
FILE * open_file (const char * filename, const char * mode) {
    if (strcmp("recipes/lib.pasta", filename) == 0) {
        return fmemopen((void *) &_binary_recipes_lib_pasta_start, (ptrdiff_t) &_binary_recipes_lib_pasta_size, mode);
    }
    return NULL;
}

// Entry point for the Thumby port
int main (int argc, char ** argv) {
    stdio_init_all();
    pasta_init();
    register_thumby_prims();
//    parse(stdin, true);
    mainloop(NULL);
}
