#include <stdbool.h>
#include <unistd.h>

#include <SDL2/SDL.h>

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
    SDL_Init(SDL_INIT_AUDIO);

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

