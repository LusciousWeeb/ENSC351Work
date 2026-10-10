// Main program to build the application
// Has main(); does initialization and cleanup and perhaps some basic logic.

#include <stdio.h>
#include <stdlib.h>
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

static long long getTimeInMs(void) {
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);

    long long seconds = spec.tv_sec;
    long long nanoseconds = spec.tv_nsec;

    return seconds * 1000 + nanoseconds / 1000000;

}
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


int main(void) {
    printf("Hello embedded world, from Lex Leung!\n");
    sleepForMs(150);
    printf("When the LEDs light up, press the joystick in that direction!\n");
    sleepForMs(100);
    printf("Press left or right to exit\n");


    Led_initialize();
    Joystick_initialize();
    srand((unsigned int)time(NULL));
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
    long long highScore;
    while (true) {
        printf("Get ready...\n");
        flashGetReady();
        
        // Ensure that the joystick is released

        JoystickDirection direction = Joystick_getDirection();
/*
        switch (direction) {
            case JOYSTICK_UP:
                printf("DEBUG: UP\n");
                break;

            case JOYSTICK_DOWN:
                printf("DEBUG: DOWN\n");
                break;

            case JOYSTICK_LEFT:
                printf("DEBUG: LEFT\n");
                break;

            case JOYSTICK_RIGHT:
                printf("DEBUG: RIGHT\n");
                break;

            case JOYSTICK_NONE:
                printf("DEBUG: NONE\n");
                break;
            
*/
        if ((direction == JOYSTICK_UP) || (direction == JOYSTICK_DOWN)) {

            printf("Please let go of joystick\n");

            while ((direction == JOYSTICK_UP) || (direction == JOYSTICK_DOWN)) {
                sleepForMs(100);
                direction = Joystick_getDirection();
            }
        }

        // Generate random delay (500ms-3000ms)
        long long randomDelayMs = 500 + (rand() % 2501);

        sleepForMs(randomDelayMs);


        // Check if move too early

        direction = Joystick_getDirection();
 
        if ((direction == JOYSTICK_UP) || (direction == JOYSTICK_DOWN)) {
            printf("TOO SOON!\n");
            continue;
        }

        JoystickDirection targetDirection;
        if (rand() % 2 == 0) {
            targetDirection = JOYSTICK_UP;

            printf("Press Up now!\n");
            Led_setGreen(true);
        } else {
            targetDirection = JOYSTICK_DOWN;

            printf("Press Down now!\n");
            Led_setGreen(true);
        }

        // Get time now
        long long startTime = getTimeInMs();

        JoystickDirection response = JOYSTICK_NONE;

        while (response == JOYSTICK_NONE) {
            response = Joystick_getDirection();

            long long timeElapsed = getTimeInMs() - startTime;

            if (timeElapsed >= 5000) {
                printf("No input detected! Exiting program\n");
                Led_setGreen(false);
                Led_setRed(false);
                Joystick_cleanup();
                Led_cleanup();
                

                return 0;
            }

            // sleepForMs(5);
        }

        // Once out of loop, user reacted:

        Led_setGreen(false);
        Led_setRed(false);

        long long reactionTime = getTimeInMs() - startTime;

        if ((response == JOYSTICK_RIGHT) || (response == JOYSTICK_LEFT)) {

            printf("User selected to quit.\n");

            return 1;

        } if (targetDirection == response) {
            printf("Correct!\n");

            if ((reactionTime < highScore) || (highScore == -1)) {
                highScore = reactionTime;
                printf("New best time!\n");
            }

            printf("Your reaction time was %lldms; best so far in game is %lldms\n", 
                reactionTime,
                highScore);

            flashResult(true);

        } else {
            printf("Incorrect.\n");

            flashResult(false);
        }

    }

    Joystick_cleanup();
    Led_cleanup();

    return 0;
}