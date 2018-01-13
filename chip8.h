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

#ifndef chip8_h
#define chip8_h

#include <stdbool.h>
#include <stddef.h>

typedef struct chip8 chip8_t;

typedef struct chip8_keyboard_input {
    bool keys[16];
} chip8_keyboard_input_t;

chip8_t* chip8_make(void);
void chip8_destroy(chip8_t *ch);
bool chip8_load_program(chip8_t *ch, unsigned char *program, size_t size);
bool chip8_cpu_tick(chip8_t *ch, const chip8_keyboard_input_t *input);
bool chip8_should_beep(chip8_t *ch);
int chip8_get_width(chip8_t *ch);
int chip8_get_height(chip8_t *ch);
bool chip8_get_pixel(chip8_t *ch, int x, int y);
int chip8_is_super(chip8_t *ch);

#endif /* chip8_h */
