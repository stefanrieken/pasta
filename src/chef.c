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

uint16_t offsets[] = {
    0,      // General
    0x5000 // Tile memory (TODO: this list should depend on machine type, or at least Pasta types should come first!)
};

FILE * file = NULL;
char identifier[5];

void write_little_endian(FILE * file, uint16_t num) {
     fputc(num & 0xFF, file);
     fputc(num >> 8, file);
}

uint16_t read_little_endian() {
    uint16_t result = 0;
    result |= fgetc(file);
    result |= (fgetc(file) << 8);
    return result;
}

uint16_t absolutize(uint16_t address, uint8_t addressing, uint8_t chunkType) {
    if (addressing == 2) return offsets[chunkType] + address; // relative
    return address; // none, absolute, append or other; need not or will not convert
}

uint16_t relativate(uint16_t address, uint8_t addressing, uint8_t chunkType) {
    if (addressing == 2) return address - offsets[chunkType]; // relative
    return address; // none, absolute, append or other; need not or will not convert
}

uint16_t chef_prim_group_cb(uint8_t prim) {
    uint16_t n = peek(&argstack);
    uint16_t temp, func;
    uint16_t result = 0;

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
                uint8_t machineType = item(&argstack, n--);
                uint8_t chunkType = item(&argstack, n--);
                uint8_t chunkVersion = item(&argstack, n--);
                uint8_t addressing = item(&argstack, n--); 
                uint16_t address = item(&argstack, n--);
                uint16_t size = item(&argstack, n--);
                fputc(machineType, file);
                fputc(chunkType, file);
                fputc(chunkVersion, file);
                fputc(addressing, file);
                write_little_endian(file, relativate(address, addressing, chunkType));
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
            if (n < 2 || n > 4) { // try to fail early when it comes to file access
                printf("Wrong number of args to read_chunkf\n");
            }
            temp = item(&argstack, n--);
            file = fopen((char *) &memory[temp], "r");
            if (file == NULL) { printf("Error opening file.\n"); break; }
            for (int i=0; i<4; i++) identifier[i] = fgetc(file);
            if (strcmp(identifier, "ChEF") != 0) { printf("File has wrong identifier.\n"); fclose(file); break; }
            int version = fgetc(file);
            int machineType = fgetc(file);
            int fileType = fgetc(file);
            int details = fgetc(file);
            if (n > 1) {
                // Process general header through callback
                func = item(&argstack, n--);
                push(&argstack, version);
                push(&argstack, machineType);
                push(&argstack, fileType);
                push(&argstack, details);
                push(&argstack, 4); // 4 args
                run_func(func);
                result = pop(&argstack);
                if (result == 0) { printf("General file header not accepted; stopping read.\n"); fclose(file); break; }
            }

            machineType = fgetc(file);
            if (n > 1) func = item(&argstack, n);
            while (machineType != EOF) {
                int chunkType = fgetc(file);
                int chunkVersion = fgetc(file);
                int addressing = fgetc(file);
                uint16_t address = absolutize(read_little_endian(file), addressing, chunkType);
                uint16_t size = read_little_endian(file);

                if (n > 1) {
                    // Process chunk (header)
                    push(&argstack, machineType);
                    push(&argstack, chunkType);
                    push(&argstack, chunkVersion);
                    push(&argstack, addressing);
                    push(&argstack, 4); // 4 args
                    run_func(func);
                    result = pop(&argstack);
                    if (result == 2) { printf("Chunk not accepted; stopping read.\n"); fclose(file); break; }
                    // if result == 0, we just skip the read instead.
                } else result = 1;

                for (int i=0; i<size; i++) {
                    int ch = fgetc(file);
                    // Only write the data if the callback allowed it.
                    if (result == 1) memory[address+i] = ch;
                }

                machineType = fgetc(file);
            }
            fclose(file);
            n = 1;
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

