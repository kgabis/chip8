/*
    Copyright (c) 2018 Krzysztof Gabis
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#ifdef WIN32
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#include "chip8.h"

#define RMASK 0x000000ff
#define GMASK 0x0000ff00
#define BMASK 0x00ff0000
#define AMASK 0xff000000
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 320

static void set_pixel(uint32_t *surface, int x, int y);
static void set_scaled_pixel(uint32_t *surface, int scale, int x, int y);
static void get_input(const Uint8 *state, chip8_keyboard_input_t *input);
static unsigned char* read_file(const char *filename, size_t *out_size);

int main(int argc, const char * argv[]) {
    if (argc != 2) {
        printf("Usage: %s program\n", argv[0]);
        return 1;
    }
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        perror("Could not initialize SDL");
        puts("Error!");
        return -1;
    }

    char window_name_buf[256];
    snprintf(window_name_buf, sizeof(window_name_buf), "chip8 - %s", argv[1]);

    SDL_Window *window;
    SDL_Renderer *sdl_renderer;
    SDL_CreateWindowAndRenderer(640, 320, 0, &window, &sdl_renderer);
    SDL_SetWindowTitle(window, window_name_buf);
    SDL_Surface *surface = SDL_CreateRGBSurface(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32, RMASK, GMASK, BMASK, AMASK);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(sdl_renderer, surface);

    size_t program_size;
    unsigned char *program = read_file(argv[1], &program_size);
    if (!program) {
        printf("Loading %s failed", argv[1]);
        return 1;
    }

    chip8_t *ch8 = chip8_make();
    assert(ch8);

    bool ok = chip8_load_program(ch8, program, program_size);
    assert(ok);

    while (true) {
        chip8_keyboard_input_t input = {};

        SDL_Event e;
        SDL_PollEvent(&e);
        if (e.type == SDL_QUIT) {
            goto end;
        }

        const Uint8 *keyboard_state = SDL_GetKeyboardState(NULL);

        get_input(keyboard_state, &input);

        int num_ticks = chip8_is_super(ch8) ? 16 : 8;
        for (int i = 0; i < num_ticks; i++) {
            ok = chip8_cpu_tick(ch8, &input);
            assert(ok);
        }
        if (chip8_should_beep(ch8)) {
            puts("beep");
        }

        int scale = SCREEN_WIDTH / chip8_get_width(ch8);
        memset(surface->pixels, 0, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
        for (int y = 0; y < chip8_get_height(ch8); y++) {
            for (int x = 0; x < chip8_get_width(ch8); x++) {
                if (chip8_get_pixel(ch8, x, y)) {
                    set_scaled_pixel((uint32_t*)surface->pixels, scale, x, y);
                }
            }
        }

        SDL_RenderClear(sdl_renderer);
        SDL_UpdateTexture(texture, NULL, surface->pixels, surface->pitch);
        SDL_RenderCopy(sdl_renderer, texture, NULL, NULL);
        SDL_RenderPresent(sdl_renderer);

        SDL_Delay(16);
    }
end:
    chip8_destroy(ch8);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

static void set_pixel(uint32_t *surface, int x, int y) {
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) {
        return;
    }
    int ix = (y * SCREEN_WIDTH) + x;
    surface[ix] = 0xffffffff;
}

static void set_scaled_pixel(uint32_t *surface, int scale, int x, int y) {
    int x_start = x * scale;
    int x_end = (x + 1) * scale;
    int y_start = y * scale;
    int y_end = (y + 1) * scale;
    for (int yi = y_start; yi < y_end; yi++) {
        for (int xi = x_start; xi < x_end; xi++) {
            set_pixel(surface, xi, yi);
        }
    }
}

static void get_input(const Uint8 *state, chip8_keyboard_input_t *input) {
    if (state[SDL_SCANCODE_1]) input->keys[0x1] = true;
    if (state[SDL_SCANCODE_2]) input->keys[0x2] = true;
    if (state[SDL_SCANCODE_3]) input->keys[0x3] = true;
    if (state[SDL_SCANCODE_4]) input->keys[0xc] = true;
    if (state[SDL_SCANCODE_Q]) input->keys[0x4] = true;
    if (state[SDL_SCANCODE_W]) input->keys[0x5] = true;
    if (state[SDL_SCANCODE_E]) input->keys[0x6] = true;
    if (state[SDL_SCANCODE_R]) input->keys[0xd] = true;
    if (state[SDL_SCANCODE_A]) input->keys[0x7] = true;
    if (state[SDL_SCANCODE_S]) input->keys[0x8] = true;
    if (state[SDL_SCANCODE_D]) input->keys[0x9] = true;
    if (state[SDL_SCANCODE_F]) input->keys[0xe] = true;
    if (state[SDL_SCANCODE_Z]) input->keys[0xA] = true;
    if (state[SDL_SCANCODE_X]) input->keys[0x0] = true;
    if (state[SDL_SCANCODE_C]) input->keys[0xb] = true;
    if (state[SDL_SCANCODE_V]) input->keys[0xf] = true;
}

static unsigned char* read_file(const char *filename, size_t *out_len) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return NULL;
    }
    fseek(fp, 0L, SEEK_END);
    long pos = ftell(fp);
    if (pos < 0) {
        fclose(fp);
        return NULL;
    }
    size_t file_size = pos;
    rewind(fp);
    unsigned char *file_contents = malloc(file_size);
    if (!file_contents) {
        fclose(fp);
        return NULL;
    }
    if (fread(file_contents, file_size, 1, fp) < 1) {
        if (ferror(fp)) {
            fclose(fp);
            free(file_contents);
            return NULL;
        }
    }
    fclose(fp);
    *out_len = file_size;
    return file_contents;
}
