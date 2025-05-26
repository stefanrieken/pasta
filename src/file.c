#include <stdio.h>
#include <stdint.h>

#include "pasta.h"

enum {
    PRIM_SAVE,
    PRIM_LOAD
};

void save(char * filename, uint8_t bitmap, uint8_t append) {
    FILE * file = fopen(filename, "w");

    // write start-of / top-of registers first
    for (int i=0; i< 8; i++) {
        if ((bitmap & (1 << i)) != 0) {
            int from = mem[i];
//            if ((append & (1 << i)) != 0) from = 0; // write zero 'from' to indicate data may be read appended (fwiw)
            fputc(from & 0xFF, file);
            fputc((from >> 8) & 0xFF, file);            
        } else {
            fputc(0, file);
            fputc(0, file);
        }
    }
    for (int i=0; i< 8; i++) {
        if ((bitmap & (1 << i)) != 0) {
            int to = mem[8 + i];
            fputc(to & 0xFF, file);
            fputc((to >> 8) & 0xFF, file);
        } else {
            fputc(0, file);
            fputc(0, file);
        }
    }

    // Then write data
    for (int i=0; i< 8; i++) {
        if ((bitmap & (1 << i)) != 0) {
            int from = mem[i];
            int to = mem[8 + i];

            //printf("Section %d from %d to %d\n", i, from, to);
            for (int j=from;j<to;j++) {
                fputc(memory[j], file);
            }
        }
    }
    
    fclose(file);
}

void load(char * filename, uint8_t bitmap, uint8_t append) {
    FILE * file = open_file(filename, "r");

    uint16_t from[8];
    uint16_t to[8];
    // temp read offset registers first
    for (int i=0; i< 8; i++) {
        from[i] = fgetc(file);
        from[i] |= fgetc(file) << 8;
    }
    for (int i=0; i< 8; i++) {
        to[i] = fgetc(file);
        to[i] |= fgetc(file) << 8;
    }

    // Then update registers and read data
    for (int i=0; i< 8; i++) {
        if (from[i] == 0) {
            // TODO process append mode
        }
        // update registers
        if (from[i] != 0 && to[i] >= from[i]) {
            mem[i] = from[i];
            mem[8+i] = to[i];

            //printf("Section %d from %d to %d\n", i, from[i], to[i]);
        }
        // read data
        for (int j=from[i]; j<to[i]; j++) {
            memory[j] = fgetc(file);
        }
    }

    fclose(file);
}

uint16_t file_prim_group_cb(uint8_t prim) {
    uint16_t n = peek(&argstack);
    char * filename;
    uint8_t index, bitmap = 0, append = 0;
    uint16_t result = 0;

    switch(prim) {
        case PRIM_SAVE:
            filename = (char *) &memory[item(&argstack, n--)];
            while (n > 1) {
                index = item(&argstack, n--);
                bitmap |= 1 << (index >> 1); // 2-byte offset
                if (index & 0b01) append |= 1 << (index >> 1);
            }
            if (bitmap == 0) bitmap = 0b11111111; // default: write all
            save(filename, bitmap, append);
            result = 0;
            break;
        case PRIM_LOAD:
            filename = (char *) &memory[item(&argstack, n--)];
            while (n > 1) {
                index = item(&argstack, n--);
                bitmap |= 1 << (index >> 1); // 2-byte offset
                if (index & 0b01) append |= 1 << (index >> 1);
            }
            load(filename, bitmap, append);
            result = 0;
            break;
    }

    if (n != 1) printf("WARN: %d leftover args\n", n);

    return result;
}

void register_file_prims() {
    uint8_t group = add_primitive_group(file_prim_group_cb);
    add_variable("save", add_primitive(group | PRIM_SAVE));
    add_variable("load", add_primitive(group | PRIM_LOAD));
}

