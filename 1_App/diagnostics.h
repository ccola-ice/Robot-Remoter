#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H
#include <stdint.h>
/* Foreground service screens; normal application tasks resume on return. */
void diagnostics_menu(void);
void eeprom_menu(void);
void diag_screen(const char *title);
void diag_line(uint16_t y, const char *text);
int diag_key(void); /* Debounced press AND release; MenuKey or -1. */
void diag_release(void);
#endif
