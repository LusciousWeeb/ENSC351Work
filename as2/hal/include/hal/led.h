#ifndef LED_H
#define LED_H

#include <stdbool.h>

void Led_initialize(void);
void Led_setGreen(bool on);
void Led_setRed(bool on);
void Led_cleanup(void);

#endif
