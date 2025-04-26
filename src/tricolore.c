#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#ifndef PICO_SDK
#include <time.h>
#else
#include "pico/stdlib.h"
#endif

#include "pasta.h"
#include "tricolore.h"

#include "bitmap.h"

extern void * mainloop(void * arg);


bool screen_active = false;

Variable * direction;
Variable * pointer_x;
Variable * pointer_y;
Variable * click;
Variable * hires;

// This value is calculated during 'draw', but is also
// used by the pointer coordinate calculation function
int shift;

#ifndef PICO_SDK
struct timespec last_time;
struct timespec this_time;
#endif

enum {
    PRIM_WRITE,
    PRIM_DRAW,
    PRIM_CATCHUP,
    PRIM_SAVE_SHEET,
    PRIM_BEEP
};

uint16_t disp_prim_group_cb(uint8_t prim) {
  shift = !hires->value && (SCREEN_WIDTH == 32);

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
                  temp = (temp / (SCREEN_WIDTH >> shift)) * (SCREEN_WIDTH >> shift) + (SCREEN_WIDTH >> shift); // set cursor to new line (TODO does not compute end of screen!)
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
// automatically fall through to draw after write
//            break;
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
#ifndef PICO_SDK
            do {
                usleep(10000);
                clock_gettime(CLOCK_REALTIME, &this_time);
            } while(this_time.tv_sec == last_time.tv_sec && (this_time.tv_nsec >> 24) == (last_time.tv_nsec >> 24));
            last_time = this_time;
#else
            get_button_state();
//            sleep_us(5000);
#endif
            result = screen_active;
            break;
        case PRIM_SAVE_SHEET:
            // Utility function specific to the editor
            temp = item(&argstack, n--);
            quickwrite_2bitmap("out.bmp", &memory[temp], (uint32_t *) &memory[item(&argstack, n--)]);
            break;
        case PRIM_BEEP:;
            int frequency = 440;
            int duration = 500;
            if(n > 1) frequency = item(&argstack, n--);
            if(n > 1) duration  = item(&argstack, n--);
            beep(frequency, duration);
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
    hires = add_variable("hires", 0);
  shift = !hires->value && (SCREEN_WIDTH == 32);
    add_variable("write", add_primitive(group | PRIM_WRITE));
    add_variable("draw", add_primitive(group | PRIM_DRAW));
    add_variable("catchup", add_primitive(group | PRIM_CATCHUP));
    add_variable("save_sheet", add_primitive(group | PRIM_SAVE_SHEET));
    add_variable("beep", add_primitive(group | PRIM_BEEP));

#ifndef PICO_SDK
    clock_gettime(CLOCK_REALTIME, &last_time);
#endif
}

void tricolore_init() {
    register_display_prims();

#ifdef PICO_SDK
    shift = 0;
#else
    shift = 1;
#endif

    // Fill ascii
    quickread_2bitmap("assets/ascii1.bmp", &memory[TILE_MEM], (uint32_t *) &memory[PALETTE_MEM]);
    quickread_2bitmap("assets/ascii2.bmp", &memory[TILE_MEM+0x0400], (uint32_t *) &memory[PALETTE_MEM]);

    // Load Pasta + Tricolore libs
    FILE * infile;
    if ((infile = open_file("recipes/lib.trico", "rb"))) {
        parse(infile, true);
        fclose(infile);
    }

    screen_active = true; // well, almost
}

// Draw out sprite memory. This is implementation agnostic.
// Screen implementations must provide:
// - Their own initialization
// - The set_pixel callback
// - The redraw callback

void draw(int from_x, int from_y, int width, int height) {
  // May not need to clear screen when redrawing full background / character screen

  // If no hi res but screen width allows it, shift by one to get 'lo res'
  shift = !hires->value && (SCREEN_WIDTH == 32);
  uint32_t * palette = (uint32_t *) &memory[PALETTE_MEM];

  // 16 sprite structs of size 16 = 256 bytes
  for (int s =0; s<16; s++) {
    Sprite * sprite = (Sprite *) &memory[SPRITE_MEM + (16 * s)];
    int transparent = sprite->flags & 0x0F;
    int scalex = ((sprite->flags >> 4) & 0b11)+1; // Can scale 2,3,4 times; maybe rather 2,4,8?
    int scaley = ((sprite->flags >> 6) & 0b11)+1;

    scalex = scalex << shift; scaley = scaley << shift;

    if (sprite->width != 0 && sprite->height != 0) {
      int width_map = sprite->width; //(sprite->width + 7) / 8; // Even if width and height are not byte aligned, their map data is

      for (int i=0; i<sprite->height*8*scaley;i++) {
        if ((sprite->y<<shift)+i < (from_y<<shift) || (sprite->y<<shift)+i >(from_y+height) << shift) continue; // Some attempt to skip parts that don't need drawing

        // Try to get some (partial) calculations before the next for loop, to avoid repetition
        int map_idx_h = (i/(8*scaley))*width_map;

        for (int j=0; j<sprite->width*8*scalex; j++) {
          if ((sprite->x<<shift)+j < (from_x<<shift) || (sprite->x<<shift)+j >(from_x+width)<<shift) continue; // Some attempt to skip parts that don't need drawing

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

          if (!(transparent & (1 << pxdata))) {
              int color = (sprite->colors >> ((pxdata ^ colormask) *4)) & 0b1111;
              set_pixel((((sprite->x<<shift)+j)%(8*SCREEN_WIDTH<<shift)), (((sprite->y<<shift)+i)%(8*SCREEN_HEIGHT<<shift)), palette[color]); // The % allows for rotation
          }
        }
      }
    }
  }

  redraw(from_x<<shift, from_y<<shift, width<<shift, height<<shift);
}
