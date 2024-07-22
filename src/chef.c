#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pasta.h"

// Chunky Extensible FS primitive defs
enum {
    PRIM_MAKE_CHUNKF,
    PRIM_CLOSE_CHUNKF,
    PRIM_WRITE_CHUNK,
    PRIM_READ_CHUNKF
};

FILE * file = NULL;
char identifier[5];

void write_little_endian(FILE * file, uint16_t num) {
     fputc(num & 0xFF, file);
     fputc(num >> 8, file);
}

int16_t read_little_endian() {
    uint16_t result = 0;
    result |= fgetc(file);
    result |= (fgetc(file) << 8);
    return result;
}

uint16_t chef_prim_group_cb(uint8_t prim) {
    uint16_t n = peek(&argstack);
    uint16_t temp, func;
    uint16_t result = 0;
    char ch = 0;

    switch(prim) {
        case PRIM_MAKE_CHUNKF:
            if (n != 6) {
                printf("Wrong number of args to make_chunkf\n");
            } else {
                temp = item(&argstack, n--);
                file = fopen((char *) &memory[temp], "w");
                fwrite("ChEF", 4, 1, file);
                fputc(item(&argstack, n--), file); // Version
                fputc(item(&argstack, n--), file); // Pasta machine type
                fputc(item(&argstack, n--), file); // File type
                fputc(item(&argstack, n--), file); // Details
            }

            result = 0; // File handle; but there's only one file pointer
            break;
        case PRIM_WRITE_CHUNK:
            if (file != NULL && n == 8) {
                temp = item(&argstack, n--); // That's the file arg
                fputc(item(&argstack, n--), file); // Machine type
                fputc(item(&argstack, n--), file); // Chunk type
                fputc(item(&argstack, n--), file); // Chunk version
                fputc(item(&argstack, n--), file); // Addressing
                uint16_t address = item(&argstack, n--);
                write_little_endian(file, address); // TODO translate from absolute input to suit addressing type
                uint16_t size = item(&argstack, n--);
                write_little_endian(file, size);
                for (int i=0;i<size;i++) {
                    fputc(memory[address+i], file);
                }
            } else printf("No file open or wrong number of args.\n");
            break;
        case PRIM_CLOSE_CHUNKF:
            temp = item(&argstack, n--); // That's the file arg
            if (file != NULL) fclose(file);
            result = 0;
            break;
        case PRIM_READ_CHUNKF:
            if (n != 4) {
                printf("Wrong number of args to read_chunkf (also: TODO add default callbacks)\n");
            }
            temp = item(&argstack, n--);
            file = fopen((char *) &memory[temp], "r");
            if (file == NULL) printf("Error opening file.\n");
            else {
                for (int i=0; i<4; i++) identifier[i] = fgetc(file);
                if (strcmp(identifier, "ChEF") != 0) printf("File has wrong identifier.\n");
                else {
                    // Process general header
                    func = item(&argstack, n--); // Get callback func
                    push(&argstack, fgetc(file)); // Version
                    push(&argstack, fgetc(file)); // Pasta machine type
                    push(&argstack, fgetc(file)); // File type
                    push(&argstack, fgetc(file)); // Details
                    push(&argstack, 4); // 4 args
                    run_func(func);
                    result = pop(&argstack);

                    func = item(&argstack, n--); // Get callback func
                    ch = fgetc(file);
                    while (ch != EOF) {
                        // Process chunk (header)
                        push(&argstack, ch);          // Machine type
                        push(&argstack, fgetc(file)); // Chunk type
                        push(&argstack, fgetc(file)); // Chunk version
                        push(&argstack, fgetc(file)); // Addressing
                        push(&argstack, 4); // 4 args
                        run_func(func);
                        result = pop(&argstack); // TODO warn or stop if result is false??
                        uint16_t address = read_little_endian(file); // TODO interpret other than absolute, if applicable
                        uint16_t size = read_little_endian(file);

                        for (int i=0; i<size; i++) {
                            char ch = fgetc(file);
                            // Only write the data if the callback allowed it.
                            if (result != 0) memory[address+i] = ch;
                        }

                        ch = fgetc(file);
                    }
                }
            }
            result = 0;
            break;
    }
    
    if (n != 1) printf("WARN: %d leftover args\n", n);

    return result;
}

void register_chef_prims() {
    file = NULL;
    identifier[4] = 0;

    uint8_t group = add_primitive_group(chef_prim_group_cb);
    add_variable("make_chunkf", add_primitive(group | PRIM_MAKE_CHUNKF));
    add_variable("write_chunk", add_primitive(group | PRIM_WRITE_CHUNK));
    add_variable("close_chunkf", add_primitive(group | PRIM_CLOSE_CHUNKF));
    add_variable("read_chunkf", add_primitive(group | PRIM_READ_CHUNKF));
}

