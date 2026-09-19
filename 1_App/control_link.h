#ifndef CONTROL_LINK_H
#define CONTROL_LINK_H
#include <stdint.h>
void control_link_init(uint8_t boot_permitted);
void control_link_service(uint8_t control_page);
void control_link_inhibit(void);
const char *control_link_status(void);
#endif
