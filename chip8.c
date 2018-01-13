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

#include "chip8.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32
#define SDISPLAY_WIDTH 128
#define SDISPLAY_HEIGHT 64
#define DISPLAY_SIZE ((SDISPLAY_WIDTH * SDISPLAY_HEIGHT) / 8)
#define MEMORY_SIZE 4096
#define NUM_REGS 16
#define PROGRAM_OFFSET 0x200
#define STACK_OFFSET 0xea0
#define SUPER_DIGITS_OFFSET 0x50

static void chip8_set_pixel(chip8_t *ch, int x, int y, bool val);
static bool chip8_get_bit(uint8_t *bytes, int ix);
static void chip8_set_bit(uint8_t *bytes, int ix, bool val);
static void chip8_draw_sprite(chip8_t *ch, uint8_t *sprite, int n, int x, int y, uint8_t *vf);

struct chip8 {
    uint8_t memory[MEMORY_SIZE];
    uint16_t *stack;
    uint8_t display[DISPLAY_SIZE];
    uint8_t regs[NUM_REGS];
    uint16_t i_reg;
    uint16_t program_counter;
    uint8_t stack_pointer;
    uint8_t delay_timer;
    uint8_t sound_timer;
    size_t program_size;
    bool increment_ireg;
    bool schip_mode;
    int timer_counter;
};

static uint8_t digits[] = {
    0xf0, 0x90, 0x90, 0x90, 0xf0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xf0, 0x10, 0xf0, 0x80, 0xf0, // 2
    0xf0, 0x10, 0xf0, 0x10, 0xf0, // 3
    0x90, 0x90, 0xf0, 0x10, 0x10, // 4
    0xf0, 0x80, 0xf0, 0x10, 0xf0, // 5
    0xf0, 0x80, 0xf0, 0x90, 0xf0, // 6
    0xf0, 0x10, 0x20, 0x40, 0x40, // 7
    0xf0, 0x90, 0xf0, 0x90, 0xf0, // 8
    0xf0, 0x90, 0xf0, 0x10, 0xf0, // 9
    0xf0, 0x90, 0xf0, 0x90, 0x90, // A
    0xe0, 0x90, 0xe0, 0x90, 0xe0, // B
    0xf0, 0x80, 0x80, 0x80, 0xf0, // C
    0xe0, 0x90, 0x90, 0x90, 0xe0, // D
    0xf0, 0x80, 0xf0, 0x80, 0xf0, // E
    0xf0, 0x80, 0xf0, 0x80, 0x80  // F
};

static uint8_t super_digits[] = {
    0xff, 0xff, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xff, 0xff, // 0
    0x18, 0x78, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0xff, 0xff, // 1
    0xff, 0xff, 0x03, 0x03, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, // 2
    0xff, 0xff, 0x03, 0x03, 0xff, 0xff, 0x03, 0x03, 0xff, 0xff, // 3
    0xc3, 0xc3, 0xc3, 0xc3, 0xff, 0xff, 0x03, 0x03, 0x03, 0x03, // 4
    0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0x03, 0x03, 0xff, 0xff, // 5
    0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xc3, 0xc3, 0xff, 0xff, // 6
    0xff, 0xff, 0x03, 0x03, 0x06, 0x0c, 0x18, 0x18, 0x18, 0x18, // 7
    0xff, 0xff, 0xc3, 0xc3, 0xff, 0xff, 0xc3, 0xc3, 0xff, 0xff, // 8
    0xff, 0xff, 0xc3, 0xc3, 0xff, 0xff, 0x03, 0x03, 0xff, 0xff, // 9
    0x7e, 0xff, 0xc3, 0xc3, 0xc3, 0xff, 0xff, 0xc3, 0xc3, 0xc3, // A
    0xfc, 0xfc, 0xc3, 0xc3, 0xfc, 0xfc, 0xc3, 0xc3, 0xfc, 0xfc, // B
    0x3c, 0xff, 0xc3, 0xc0, 0xc0, 0xc0, 0xc0, 0xc3, 0xff, 0x3c, // C
    0xfc, 0xfe, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xfe, 0xfc, // D
    0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, // E
    0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xc0  // F
};

chip8_t* chip8_make(void) {
    chip8_t *ch = malloc(sizeof(chip8_t));
    if (ch == NULL) {
        return NULL;
    }
    memset(ch, 0, sizeof(chip8_t));
    ch->stack = (uint16_t*)(ch->memory + STACK_OFFSET);
    memcpy(ch->memory, digits, sizeof(digits));
    memcpy(ch->memory + SUPER_DIGITS_OFFSET, super_digits, sizeof(super_digits));
    return ch;
}

void chip8_destroy(chip8_t *ch) {
    free(ch);
}

bool chip8_load_program(chip8_t *ch, unsigned char *program, size_t size) {
    if (PROGRAM_OFFSET + size >= STACK_OFFSET) {
        return false;
    }
    memcpy(ch->memory + PROGRAM_OFFSET, program, size);
    ch->program_size = size;
    ch->i_reg = 0;
    ch->program_counter = PROGRAM_OFFSET;
    ch->stack_pointer = 0;
    ch->delay_timer = 0;
    ch->sound_timer = 0;
    ch->increment_ireg = false;
    ch->schip_mode = false;
    ch->timer_counter = 0;
    return true;
}

bool chip8_cpu_tick(chip8_t *ch, const chip8_keyboard_input_t *input) {
    ch->timer_counter++;
    if (ch->timer_counter >= 16 || (ch->schip_mode && ch->timer_counter >= 8)) {
        if (ch->delay_timer > 0) {
            ch->delay_timer--;
        }
        if (ch->sound_timer > 0) {
            ch->sound_timer--;
        }
        ch->timer_counter = 0;
    }
    if ((ch->program_counter - PROGRAM_OFFSET + 1) >= ch->program_size) {
        return false;
    }
    uint8_t bytes[2] = {
        ch->memory[ch->program_counter],
        ch->memory[ch->program_counter + 1]
    };
    uint16_t opcode = bytes[1] | (bytes[0] << 8);
    uint8_t nibs[4] = {
        (bytes[0] & 0xf0) >> 4, bytes[0] & 0x0f,
        (bytes[1] & 0xf0) >> 4, bytes[1] & 0x0f
    };
    uint16_t nnn = opcode & 0xfff;
    uint8_t nn = opcode & 0xff;
    uint8_t n = nibs[3];
    uint8_t x = nibs[1];
    uint8_t y = nibs[2];
    if (nibs[0] == 0x0 && nibs[1] == 0x0 && nibs[2] == 0xc) { // 00CN schip
        int rem_lines = chip8_get_height(ch) - n;
        int width = chip8_get_width(ch);
        memmove(ch->display + (width / 8) * n, ch->display, rem_lines * (width / 8));
        memset(ch->display, 0, (width / 8) * n);
    } else if (opcode == 0x00e0) { // 00E0
        memset(ch->display, 0x0, DISPLAY_SIZE);
    } else if (opcode == 0x00ee) { // 00EE
        ch->program_counter = ch->stack[ch->stack_pointer];
        ch->stack_pointer--;
    } else if (opcode == 0x00fa) { // 00FA non-standard
        ch->increment_ireg = !ch->increment_ireg;
    } else if (opcode == 0x00fb) { // 00FB schip
        int row_len = chip8_get_width(ch) / 8;
        uint8_t byte = 0;
        for (int row = 0; row < chip8_get_height(ch); row++) {
            uint8_t *row_ptr = ch->display + (row_len * row);
            for (int i = 0; i < row_len; i++) {
                uint8_t current_byte = row_ptr[i];
                row_ptr[i] = (current_byte >> 4) | (byte << 4);
                byte = current_byte;
            }
        }
    } else if (opcode == 0x00fc) { // 00FC schip
        int row_len = chip8_get_width(ch) / 8;
        for (int row = 0; row < chip8_get_height(ch); row++) {
            uint8_t byte = 0;
            uint8_t *row_ptr = ch->display + (row_len * row);
            for (int i = row_len - 1; i >= 0; i--) {
                uint8_t current_byte = row_ptr[i];
                row_ptr[i] = ((current_byte << 4) & 0xf0) | ((byte >> 4) & 0xf);
                byte = current_byte;
            }
        }
    } else if (opcode == 0x00fd) { // 00FD
        // exit
    } else if (opcode == 0x00fe) { // 00FE schip
        ch->schip_mode = false;
    } else if (opcode == 0x00ff) { // 00FF schip
        ch->schip_mode = true;
    } else if (nibs[0] == 0x0) { // 0NNN
        // jump to sys addr
    } else if (nibs[0] == 0x1) { // 1NNN
        ch->program_counter = nnn - 2;
    } else if (nibs[0] == 0x2) { // 2NNN
        ch->stack_pointer++;
        ch->stack[ch->stack_pointer] = ch->program_counter;
        ch->program_counter = nnn - 2;
    } else if (nibs[0] == 0x3) { // 3XNN
        if (ch->regs[x] == nn) {
            ch->program_counter += 2;
        }
    } else if (nibs[0] == 0x4) { // 4XNN
        if (ch->regs[x] != nn) {
            ch->program_counter += 2;
        }
    } else if (nibs[0] == 0x5 && nibs[3] == 0x0) { // 5XY0
        if (ch->regs[x] == ch->regs[y]) {
            ch->program_counter += 2;
        }
    } else if (nibs[0] == 0x6) { // 6XNN
        ch->regs[x] = nn;
    } else if (nibs[0] == 0x7) { // 7XNN
        ch->regs[x] = ch->regs[x] + nn;
    } else if (nibs[0] == 0x8 && nibs[3] == 0x0) { // 8XY0
        ch->regs[x] = ch->regs[y];
    } else if (nibs[0] == 0x8 && nibs[3] == 0x1) { // 8XY1
        ch->regs[x] = ch->regs[x] | ch->regs[y];
    } else if (nibs[0] == 0x8 && nibs[3] == 0x2) { // 8XY2
        ch->regs[x] = ch->regs[x] & ch->regs[y];
    } else if (nibs[0] == 0x8 && nibs[3] == 0x3) { // 8XY3
        ch->regs[x] = ch->regs[x] ^ ch->regs[y];
    } else if (nibs[0] == 0x8 && nibs[3] == 0x4) { // 8XY4
        uint16_t r = ch->regs[x] + ch->regs[y];
        ch->regs[x] = r;
        ch->regs[0xf] = r > 0xff ? 0x1 : 0x0;
    } else if (nibs[0] == 0x8 && nibs[3] == 0x5) {  // 8XY5
        ch->regs[0xf] = ch->regs[x] > ch->regs[y] ? 0x1 : 0x0;
        ch->regs[x] = ch->regs[x] - ch->regs[y];
    } else if (nibs[0] == 0x8 && nibs[3] == 0x6) {  // 8XY6
        ch->regs[0xf] = ch->regs[x] & 0x1;
        ch->regs[x] = ch->regs[x] >> 1;
    } else if (nibs[0] == 0x8 && nibs[3] == 0x7) {  // 8XY7
        ch->regs[0xf] = ch->regs[y] > ch->regs[x] ? 0x1 : 0x0;
        ch->regs[x] = ch->regs[y] - ch->regs[x];
    } else if (nibs[0] == 0x8 && nibs[3] == 0xe) {  // 8XYE
        ch->regs[0xf] = (ch->regs[x] >> 7) & 0x1;
        ch->regs[x] = ch->regs[x] << 1;
    } else if (nibs[0] == 0x9 && nibs[3] == 0x0) {  // 9XY0
        if (ch->regs[x] != ch->regs[y]) {
            ch->program_counter += 2;
        }
    } else if (nibs[0] == 0xa) { // ANNN
        ch->i_reg = nnn;
    } else if (nibs[0] == 0xb) { // BNNN
        ch->program_counter = nnn + ch->regs[0] - 2;
    } else if (nibs[0] == 0xc) {  // CXNN
        ch->regs[x] = rand() & nn;
    } else if (nibs[0] == 0xd) { // DXYN
        if ((ch->i_reg + (16 * 16)) >= MEMORY_SIZE) {
            return false;
        }
        uint8_t *sprite = ch->memory + ch->i_reg;
        chip8_draw_sprite(ch, sprite, n, ch->regs[x], ch->regs[y], &ch->regs[0xf]);
    } else if (nibs[0] == 0xe && nibs[2] == 0x9 && nibs[3] == 0xe) { // EX9E
        if (input->keys[ch->regs[x]]) {
            ch->program_counter += 2;
        }
    } else if (nibs[0] == 0xe && nibs[2] == 0xa && nibs[3] == 0x1) { // EXA1
        if (!input->keys[ch->regs[x]]) {
            ch->program_counter += 2;
        }
    } else if (nibs[0] == 0xf && nibs[2] == 0x0 && nibs[3] == 0x7) { // FX07
        ch->regs[x] = ch->delay_timer;
    } else if (nibs[0] == 0xf && nibs[2] == 0x0 && nibs[3] == 0xa) { // FX0A
        bool key_pressed = false;
        for (int i = 0; i < sizeof(input->keys); i++) {
            if (input->keys[i]) {
                key_pressed = true;
                break;
            }
        }
        if (!key_pressed) {
            return true;
        }
    } else if (nibs[0] == 0xf && nibs[2] == 0x1 && nibs[3] == 0x5) { // FX15
        ch->delay_timer = ch->regs[x];
    } else if (nibs[0] == 0xf && nibs[2] == 0x1 && nibs[3] == 0x8) { // FX18
        ch->sound_timer = ch->regs[x];
    } else if (nibs[0] == 0xf && nibs[2] == 0x1 && nibs[3] == 0xe) { // FX1E
        ch->i_reg += ch->regs[x];
    } else if (nibs[0] == 0xf && nibs[2] == 0x2 && nibs[3] == 0x9) { // FX29
        ch->i_reg = ch->regs[x] * 5;
    } else if (nibs[0] == 0xf && nibs[2] == 0x3 && nibs[3] == 0x0) { // FX30
        ch->i_reg = SUPER_DIGITS_OFFSET + ch->regs[x] * 10;
    } else if (nibs[0] == 0xf && nibs[2] == 0x3 && nibs[3] == 0x3) { // FX33
        uint8_t val = ch->regs[x];
        uint8_t bcd100 = val / 100;
        uint8_t bcd10 = (val - (bcd100 * 100)) / 10;
        uint8_t bcd1 = val - (bcd100 * 100) - (bcd10 * 10);
        if ((ch->i_reg + 2) >= MEMORY_SIZE) {
            return false;
        }
        ch->memory[ch->i_reg] = bcd100;
        ch->memory[ch->i_reg + 1] = bcd10;
        ch->memory[ch->i_reg + 2] = bcd1;
    } else if (nibs[0] == 0xf && nibs[2] == 0x5 && nibs[3] == 0x5) { // FX55
        for (int i = 0; i <= x; i++) {
            ch->memory[ch->i_reg + i] = ch->regs[i];
        }
        if (ch->increment_ireg) {
            ch->i_reg += x + 1;
        }
    } else if (nibs[0] == 0xf && nibs[2] == 0x6 && nibs[3] == 0x5) { // FX65
        for (int i = 0; i <= x; i++) {
            ch->regs[i] = ch->memory[ch->i_reg + i];
        }
        if (ch->increment_ireg) {
            ch->i_reg += x + 1;
        }
    } else { // opcode not found
        return false;
    }
    ch->program_counter += 2;
    return true;
}

bool chip8_should_beep(chip8_t *ch) {
    return ch->sound_timer > 0;
}

int chip8_get_width(chip8_t *ch) {
    return ch->schip_mode ? SDISPLAY_WIDTH : DISPLAY_WIDTH;
}

int chip8_get_height(chip8_t *ch) {
    return ch->schip_mode ? SDISPLAY_HEIGHT : DISPLAY_HEIGHT;
}

bool chip8_get_pixel(chip8_t *ch, int x, int y) {
    int width = chip8_get_width(ch);
    int height = chip8_get_height(ch);
    int ix = ((y % height) * width) + (x % width);
    return chip8_get_bit(ch->display, ix);
}

int chip8_is_super(chip8_t *ch) {
    return ch->schip_mode;
}

// Internal
static void chip8_draw_sprite(chip8_t *ch, uint8_t *sprite, int n, int x, int y, uint8_t *vf) {
    int cols = 8;
    int rows = n;
    if (n == 0) {
        cols = 16;
        rows = 16;
    }
    *vf = 0;
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            int pixel_ix = (row * cols) + col;
            bool display_pixel = chip8_get_pixel(ch, x + col, y + row);
            bool sprite_pixel = chip8_get_bit(sprite, pixel_ix);
            if (display_pixel && sprite_pixel) {
                *vf = 1;
            }
            chip8_set_pixel(ch, x + col, y + row, display_pixel ^ sprite_pixel);
        }
    }
}

static bool chip8_get_bit(uint8_t *bytes, int ix) {
    return (bytes[ix / 8] >> (7 - (ix % 8))) & 1;
}

static void chip8_set_bit(uint8_t *bytes, int ix, bool val) {
    unsigned int byte_ix = ix / 8;
    unsigned int bit_ix = 7 - (ix % 8);
    uint8_t byte = bytes[byte_ix];
    byte = (byte & ~(1 << bit_ix)) | (-(unsigned int)val & (1 << bit_ix));
    bytes[byte_ix]= byte;
}

static void chip8_set_pixel(chip8_t *ch, int x, int y, bool val) {
    int width = chip8_get_width(ch);
    int height = chip8_get_height(ch);
    int ix = ((y % height) * width) + (x % width);
    chip8_set_bit(ch->display, ix, val);
}
