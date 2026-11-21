#ifndef JOYSTICK_H
#define JOYSTICK_H

typedef enum {
    JOYSTICK_NONE,
    JOYSTICK_UP,
    JOYSTICK_DOWN,
    JOYSTICK_LEFT,
    JOYSTICK_RIGHT
} JoystickDirection;

void Joystick_initialize(void);

JoystickDirection Joystick_getDirection(void);

void Joystick_cleanup(void);

#endif