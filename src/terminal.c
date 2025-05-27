#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#ifndef PICO_SDK
#include <termios.h>

#define ENTER '\n'
#else
#define ENTER '\r'
#endif

/**
 * Make Ansi compatible terminals, including Unix shells, nicer to work with.
 * It is an optional extension, enabled in the main code by defining ANSI_TERM.
 */

// ascii code EOT is sent on Ctrl+D
#define EOT 4

#ifndef PICO_SDK
static struct termios oldflags, newflags;
#endif

void set_input_unbuffered_no_echo() {
#ifndef PICO_SDK
  tcgetattr(STDIN_FILENO, &oldflags);
  newflags = oldflags;
  newflags.c_lflag &= ~(ICANON|ECHO);
  newflags.c_cc[VMIN]=1;
  tcsetattr(STDIN_FILENO, TCSANOW, &newflags);
#endif
}

static char buffer[1024];
static int idx, max;
static int saved_first_char;
static bool just_restored_buffer;

void reset_buffer() {
    if (idx > 0) saved_first_char = buffer[0];
    idx = 0; max = 0;
    buffer[idx]=0;
}

void readline() {
    int ch = fgetc(stdin);
    while (ch != EOF && ch != EOT) {
      if (ch == ENTER || ch == EOT) {
        idx = max;
        buffer[idx]= (ch == ENTER) ? '\n' : ch;
        idx++; max++;
        buffer[idx+1]='\0';
        if (ch == ENTER) printf("\n");
        break;
      } else if (ch == '\e') {
        ch = fgetc(stdin); // expect: '['
        ch = fgetc(stdin);
        while (ch >= '0' && ch <='9') ch = fgetc(stdin);
        if (ch == 'D' && idx > 0) { // arrow left
            printf("\e[D");
            idx--;
        } else if (ch == 'C' && idx < max) {
            printf("\e[C");
            idx++;
        } else if (ch == 'A') {
          // Arrow up: show saved buffer, or preprint
          if (saved_first_char != -1 && max == 0) {
            buffer[idx] = saved_first_char;
            saved_first_char = -1;
            idx = strlen(buffer);
            max = idx;
            printf("%s", buffer);
            just_restored_buffer = true;
          }
        } else if (ch == 'B') {
          // Arrow down: reset buffer
          if (idx > 0) printf("\e[%dD\e[K", idx); // clear line
          reset_buffer();
        }
      } else if (ch == 127) {
        if (idx > 0) {
            for (int i=idx-1;i<=max;i++) buffer[i]=buffer[i+1];
            max--;
            idx--;

            printf("\b");

            printf("%s ", &buffer[idx]); // re-write a portion if needed
            //printf("max: %d idx: %d\n", max, idx);
            printf("\e[%dD", max-idx+1); // but keep buffer at position
        }
      } else {
        // Perform insertion, at least for the terminating \0
        for (int i=max+2; i>idx;i--) buffer[i]=buffer[i-1];
        buffer[idx]=ch;

        printf("%s", &buffer[idx]); // re-write a portion if needed
        if (max != idx) printf("\e[%dD", max-idx); // but keep buffer at position
        max++;
        idx++;
        saved_first_char = -1;
      }

      ch = fgetc(stdin);
    }
}

static int read_idx;

int getch() {

    if(idx == 0) { readline(); read_idx = 0; }

    int ch = buffer[read_idx];
    if (ch == EOT || ch == 0) return EOF;
    if (ch == '\n') {
        buffer[read_idx] = 0; // remove newline for use in history buffer
        reset_buffer();
    }
    
    read_idx++;

    return ch;
}

void init_terminal() {
    set_input_unbuffered_no_echo();
    reset_buffer();
    saved_first_char = -1;
    just_restored_buffer = false;
}

void reset_terminal() {
#ifndef PICO_SDK
printf("\n");
//  oldflags.c_lflag |= ECHO|ICANON;
  tcsetattr(STDIN_FILENO, TCSANOW, &oldflags);
#endif
}

