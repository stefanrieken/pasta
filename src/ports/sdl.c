#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>

#include <SDL2/SDL.h>

#include "../pasta.h"
#include "../tricolore.h"

// This value refers to the window's 'zoom level' only
#define SCALE 2

int SCREEN_WIDTH=32;
int SCREEN_HEIGHT=32;

uint32_t * pixels;

struct pollfd poll_stdin;

SDL_AudioSpec desired_spec, obtained_spec;
SDL_AudioDeviceID audio_device;

unsigned int sine_phase;

float frequency; // Hz frequency divided by sample frequency
unsigned int duration; // Duration in samples

void get_data_callback(void * userdata, Uint8 * stream, int len) {
    // This is really just a multiplier to express a <1 float as an int
    float volume = 127; //32000;
    //int16_t * fstream = (int16_t *) stream; // real type changes based on spec'd format
    for (int i=0; i<(len / 1); i++) {
        stream[i] = volume * SDL_sinf(2.0 * 3.141593 * sine_phase * frequency)+127; // Put in unsinged range. Change value to get choppy waveforms!
        sine_phase++;
        // Try to end the wave neatly in the neutral position to reduce clicky sounds
        if ((stream[i] == 127)  && sine_phase > duration) frequency = 0;
    }
}

void beep (int frequency_hz, int duration_ms) {
    sine_phase = 0;
    frequency = frequency_hz / (float) obtained_spec.freq;
    duration = duration_ms * desired_spec.freq / 1000;

    SDL_PauseAudioDevice(audio_device, false);
    SDL_Delay(duration_ms * 1.5);
//    frequency = 0;
//    SDL_PauseAudioDevice(audio_device, true);
//    SDL_Delay(duration_ms / 2);
}

void init_sdl_audio() {
    desired_spec.format = AUDIO_U8; // If we ask for signed 8-bit, we fall back to unsigned
    desired_spec.channels = 1;
    desired_spec.freq = 11025; // samples per second
    desired_spec.samples=512;
    desired_spec.callback = get_data_callback;
//    desired_spec.silence = 0;
//    desired_spec.padding = 0;

    audio_device = SDL_OpenAudioDevice(NULL, false, &desired_spec, &obtained_spec, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (audio_device == 0) {
        printf("Error opening sound device: %s\n", SDL_GetError());
//    } else {
//        printf("Obtained format: %d (asked %d) sample rate: %d buffer size: %d\n", obtained_spec.format, desired_spec.format, obtained_spec.freq, obtained_spec.samples);
//        printf("Sizeof float: %ld\n", sizeof(float));
    }

    SDL_PauseAudioDevice(audio_device, false);
}

SDL_Renderer *renderer;
SDL_Window *window;
//Uint32 * pixels2; // borrow pixel array from gtk_cairo.c
extern uint32_t * pixels;
SDL_Texture * texture;

void set_pixel(int x, int y, uint32_t color) {
  int pixel_idx = (y * SCREEN_WIDTH * 8) + x;
  pixels[pixel_idx] = color;
}

// On Haiku it is made very clear that we should not redraw
// directly from the user thread. So, don't.
void redraw(int from_x, int from_y, int width, int height) {
    SDL_Event event;
    event.type = SDL_DISPLAYEVENT;
    SDL_PushEvent(&event);
}

void actual_redraw() {
    SDL_UpdateTexture(texture, NULL, pixels, SCREEN_WIDTH * 8 * sizeof(Uint32));

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void init_sdl_video() {
    SDL_CreateWindowAndRenderer(SCREEN_WIDTH * 8 * SCALE, SCREEN_HEIGHT * 8 * SCALE, 0, &window, &renderer);
    SDL_SetWindowTitle(window, "Tricolore on SDL");
    SDL_RenderSetScale(renderer, SCALE, SCALE);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, SCREEN_WIDTH * 8, SCREEN_HEIGHT * 8);
    redraw(0, 0, SCREEN_WIDTH * 8, SCREEN_HEIGHT * 8);
}

void init_sdl() {
    SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO);
    init_sdl_audio();
    init_sdl_video();

    poll_stdin.fd = fileno(stdin);
    poll_stdin.events = POLLIN;
}

void shutdown_sdl() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    // SDL exit is actually more quiet when we skip these:
//    SDL_CloseAudioDevice(audio_device);
//    SDL_Quit();
}

FILE * open_file (const char * filename, const char * mode) {
  return fopen(filename, mode);
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

void process_keydown(int key) {
    switch (key) {
        case (SDLK_LEFT):
            direction->value = ((direction->value & PLANE_VERT) | DIR_LEFT);
            break;
        case (SDLK_UP):
            direction->value = ((direction->value & PLANE_HOR) | DIR_UP);
            break;
        case (SDLK_RIGHT):
            direction->value = ((direction->value & PLANE_VERT) | DIR_RIGHT);
            break;
        case (SDLK_DOWN):
            direction->value = ((direction->value & PLANE_HOR) | DIR_DOWN);
            break;
        case (SDLK_RETURN):
            printf("Return key\n");
            break;
        case (SDLK_BACKSPACE):
            printf("Backspace key\n");
            break;
        case (SDLK_ESCAPE):
            printf("Escape key\n");
            break;
        case (SDLK_TAB):
            printf("Escape key\n");
            break;
        default:
            if (key < 0xff00) {
              // Filters out any modifiers etc. that we do not explicitly catch,
              // leaving mainly basic ascii, and perhaps a few special chars
              printf("Key press: %c (%x)\n", key, key);
            }
            break;
    }
}

void process_keyup(int key) {
    switch (key) {
        case (SDLK_LEFT):
        case (SDLK_RIGHT):
            direction->value = direction->value & PLANE_VERT;
            break;
        case (SDLK_UP):
        case (SDLK_DOWN):
            direction->value = direction->value & PLANE_HOR;
            break;
        default:
            direction->value = 0;
            break;
    }
}

extern int shift; // see tricolore.c . To be further refined

int main (int argc, char ** argv) {
    pixels = malloc(sizeof(uint32_t) * SCREEN_HEIGHT * 8 * SCREEN_WIDTH * 8);
    pasta_init();
    tricolore_init();
//    display_init(argc, argv);
    init_sdl();

    pthread_t worker_thread;
    pthread_create(&worker_thread, NULL, mainloop, &argv[1]);

    SDL_Event event;
    bool active = true;
    while (active) {
        while (SDL_PollEvent(&event)) {
            switch(event.type) {
                case SDL_QUIT:
                    active = false;
                    break;
                case SDL_KEYDOWN:
                    process_keydown(event.key.keysym.sym);
                    break;
                case SDL_KEYUP:
                    process_keyup(event.key.keysym.sym);
                    break;
                case SDL_MOUSEMOTION:
                    pointer_x->value = (event.motion.x >> shift) / SCALE;
                    pointer_y->value = (event.motion.y >> shift) / SCALE;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == 1) click->value++;
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == 1) click->value = 0;
                    break;
		case SDL_DISPLAYEVENT:
		    actual_redraw(); // trigger actual redraw from the SDL thread
		    break;
            }
        }
    }
    shutdown_sdl();
    screen_active = false;

    pthread_join(worker_thread, NULL);
    return 0;
}
