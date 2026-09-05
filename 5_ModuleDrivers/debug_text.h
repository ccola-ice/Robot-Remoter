#ifndef DEBUG_TEXT_H
#define DEBUG_TEXT_H

#include <stdint.h>
#include "ff.h"

/* printf literals and FatFs names use GBK in this project; the terminal uses
 * UTF-8. Keep this conversion at the debug UART boundary, outside the LCD. */
typedef struct {
    uint8_t lead;
    uint8_t previous_cr;
} DebugTextState;

static void DebugText_Emit(DebugTextState *state, uint8_t byte,
                           void (*emit)(uint8_t))
{
    if(byte == '\n' && !state->previous_cr) emit('\r');
    emit(byte);
    state->previous_cr = byte == '\r';
}

static void DebugText_Unicode(DebugTextState *state, uint16_t code,
                              void (*emit)(uint8_t))
{
    if(code < 0x80U) DebugText_Emit(state, (uint8_t)code, emit);
    else if(code < 0x800U)
    {
        DebugText_Emit(state, (uint8_t)(0xC0U | (code >> 6)), emit);
        DebugText_Emit(state, (uint8_t)(0x80U | (code & 0x3FU)), emit);
    }
    else
    {
        DebugText_Emit(state, (uint8_t)(0xE0U | (code >> 12)), emit);
        DebugText_Emit(state, (uint8_t)(0x80U | ((code >> 6) & 0x3FU)), emit);
        DebugText_Emit(state, (uint8_t)(0x80U | (code & 0x3FU)), emit);
    }
}

static void DebugText_Put(DebugTextState *state, uint8_t byte,
                          void (*emit)(uint8_t))
{
    if(state->lead)
    {
        uint16_t pair = ((uint16_t)state->lead << 8) | byte;
        state->lead = 0U;
        if(byte >= 0x40U && byte <= 0xFEU && byte != 0x7FU)
        {
            uint16_t code = ff_convert(pair, 1U);
            DebugText_Unicode(state, code ? code : 0xFFFDU, emit);
            return;
        }
        DebugText_Unicode(state, 0xFFFDU, emit);
        /* Preserve an ASCII control byte following a broken GBK lead. */
    }
    if(byte < 0x80U) DebugText_Emit(state, byte, emit);
    else if(byte >= 0x81U && byte <= 0xFEU) state->lead = byte;
    else DebugText_Unicode(state, byte == 0x80U ? 0x20ACU : 0xFFFDU, emit);
}

#endif
