#include "diagnostics.h"
#include "hardware_tests.h"
#include "menu.h"
#include "bsp_i2c_eeprom.h"
#include "bsp_Systick.h"
#include <stdio.h>

void eeprom_menu(void)
{
    uint8_t address = 0U, value = 0U, editing = 0U, digit = 0U, read_ok = 0U;
    uint8_t redraw = 1U, row, col, byte, error;
    int key;
    char line[96], status[96] = "Read-only browsing. Select a byte, then OK to edit.";
    diag_release();
    for(;;) {
        if(redraw) {
            diag_screen("EEPROM / AT24C08");
            diag_line(55U, "Safe window 0x00..0xFF at I2C 7-bit 0x50 (8-bit 0xA0), actual chip 1 KiB.");
            diag_line(79U, "AT24C256 0xA4 overlaps AT24C08 blocks. No access to the overlapping address.");
            for(row = 0U; row < 8U; row++) {
                uint8_t base = (address & 0xc0U) + row * 8U;
                int length = sprintf(line, "%02X: ", base);
                for(col = 0U; col < 8U; col++) {
                    error = EEPROM_Random_Read(base + col, &byte);
                    if(error) length += sprintf(line + length, " -- ");
                    else length += sprintf(line + length, " %02X ", byte);
                }
                diag_line(120U + row * 24U, line);
            }
            if(!editing) read_ok = EEPROM_Random_Read(address, &value) == 0U;
            sprintf(line, "Selected 0x%02X = %s%02X  %s", address, read_ok ? "0x" : "ERROR / ", value,
                    !editing ? "" : digit == 0U ? "EDIT high nibble" : digit == 1U ? "EDIT low nibble" : "CONFIRM WRITE");
            diag_line(332U, line);
            diag_line(370U, status);
            diag_line(410U, editing ? "LEFT/RIGHT: change nibble | OK: next / confirm write | BACK: discard" :
                      "LEFT/RIGHT: previous/next byte | OK: edit | BACK: return");
            diag_line(442U, "0xFF is reserved for Hardware Tests and cannot be edited here. No bulk erase.");
            redraw = 0U;
        }
        key = diag_key();
        if(key == MENU_KEY_BACK) {
            if(!editing) break;
            editing = 0U;
            sprintf(status, "Edit discarded; EEPROM unchanged.");
            redraw = 1U;
        }
        if(key == MENU_KEY_LEFT || key == MENU_KEY_RIGHT) {
            if(!editing) address += key == MENU_KEY_RIGHT ? 1U : 255U;
            else if(digit < 2U) {
                uint8_t shift = digit == 0U ? 4U : 0U;
                uint8_t nibble = ((value >> shift) + (key == MENU_KEY_RIGHT ? 1U : 15U)) & 0x0fU;
                value = (value & ~(0x0fU << shift)) | (nibble << shift);
            }
            redraw = 1U;
        }
        if(key == MENU_KEY_OK) {
            if(!editing) {
                if(address == HW_EEPROM_TEST_BYTE) sprintf(status, "Reserved test byte: read only.");
                else if(!read_ok) sprintf(status, "Read failed; editing disabled. Check EEPROM hardware.");
                else { editing = 1U; digit = 0U; sprintf(status, "Edit is in RAM. No write until final confirmation."); }
            } else if(digit < 2U) {
                digit++;
                if(digit == 2U) sprintf(status, "Write 0x%02X to address 0x%02X? OK writes, BACK discards.", value, address);
            } else {
                error = EEPROM_Byte_Write(address, value);
                if(!error) error = EEPROM_Random_Read(address, &byte);
                if(!error && byte != value) error = 9U;
                sprintf(status, error ? "WRITE FAILED (code %u). Re-read actual contents." : "Write and readback verified (code %u).", error);
                printf("[EEPROM] address=%02X requested=%02X result=%u\r\n", address, value, error);
                editing = 0U;
            }
            redraw = 1U;
        }
        Delay_ms(10U);
    }
}
