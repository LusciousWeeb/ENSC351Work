// Main program to build the application
// Has main(); does initialization and cleanup and perhaps some basic logic.

#include <stdio.h>
#include <stdbool.h>
#include "badmath.h"
#include "hal/button.h"
#include "hal/led.h"
#include <unistd.h>
#include <time.h>

#include "hal/joystick.h"

static void sleepForMs(long long delayMs) {
    struct timespec delay;
    delay.tv_sec = delayMs / 1000;
    delay.tv_nsec = (delayMs % 1000) * 1000000;

    nanosleep(&delay, NULL);
}
/*
static void flashGetReady(void) {
    for (int i = 0; i < 4; i++) {
        Led_setGreen(true);
        sleepForMs(250);
        Led_setGreen(false);

        Led_setRed(true);
        sleepForMs(250);
        Led_setRed(false);
    }
}

static void flashResult(bool correct) {
    for (int i = 0; i < 5; i++) {
        if (correct) {
            Led_setGreen(true);
        } else {
            Led_setRed(true);
        }

        sleepForMs(100);

        Led_setGreen(false);
        Led_setRed(false);

        sleepForMs(100);
    }

}
    */
int main(void) {
    printf("Hello embedded world, from Lex Leung!\n");


    Led_initialize();
    Joystick_initialize();
/*
    // Green LED - 1s
    Led_setGreen(true);
    sleep(1);
    Led_setGreen(false);

    // Red LED
    Led_setRed(true);
    sleep(1);
    Led_setRed(false);

    Led_cleanup();
*/
    while (1) {
        JoystickDirection direction = Joystick_getDirection();

        switch(direction) {
            case JOYSTICK_UP:
                printf("UP\n");
                break;
            case JOYSTICK_DOWN:
                printf("DOWN\n");
                break;
            case JOYSTICK_RIGHT:
                printf("RIGHT\n");
                break;
            case JOYSTICK_LEFT:
                printf("LEFT\n");
                break;
            case JOYSTICK_NONE:
                break;
        }

        sleepForMs(100);
    }

    Joystick_cleanup();
    Led_cleanup();

    return 0;
}