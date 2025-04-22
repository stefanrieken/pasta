#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "pasta.h"
#include "tricolore.h"
#include "cairo.h"

#include "bitmap.h"

extern void * mainloop(void * arg);


bool screen_active = false;

Variable * direction;
Variable * pointer_x;
Variable * pointer_y;
Variable * click;

struct timespec last_time;
struct timespec this_time;

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
                  temp = (temp / SCREEN_WIDTH) * SCREEN_WIDTH + SCREEN_WIDTH; // set cursor to new line (TODO does not compute end of screen!)
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

void tricolore_init() {
    register_display_prims();

    // Fill ascii
    quickread_2bitmap("assets/ascii1.bmp", &memory[TILE_MEM], (uint32_t *) &memory[PALETTE_MEM]);
    quickread_2bitmap("assets/ascii2.bmp", &memory[TILE_MEM+0x0400], (uint32_t *) &memory[PALETTE_MEM]);

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
}
