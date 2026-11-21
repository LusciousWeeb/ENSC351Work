// hal/src/rotary_encoder.c
// Reads the rotary encoder using libgpiod (v2 API) and reports each detent
// step through a callback.
//
// Pins (SFU Zen Hat pinout): encoder A = GPIO16 (header pin 36),
//                            encoder B = GPIO17 (header pin 11).
// The BeagleY-AI names its header lines "hat-NN-gpio" (NN = header pin), so
// the lines are found by name rather than by hard-coded chip/line numbers.
// To check on the board:   gpiofind hat-36-gpio    gpiofind hat-11-gpio
//
// How it works:
//   - Both lines are requested as inputs with edge detection on both edges.
//   - A thread waits (poll) for edge events, plus a pipe so cleanup can wake
//     it immediately.
//   - Every edge updates the A/B state and is fed through a quadrature state
//     machine that only reports a step after a complete, valid cycle, so
//     contact bounce and half-turns don't produce false steps.

#include "hal/rotary_encoder.h"

#include <errno.h>
#include <gpiod.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

// Header-pin line names (see above). The first name found is used.
static const char *const ENCODER_A_NAMES[] = { "GPIO5", NULL };
static const char *const ENCODER_B_NAMES[] = { "GPIO6", NULL };

// If turning the knob clockwise lowers the frequency, set this to 1.
#define ROTARY_ENCODER_REVERSE_DIRECTION 0

#define MAX_GPIO_CHIPS     16
#define EVENT_BUFFER_SIZE  16

// --- Quadrature decoding (full-step state machine) -------------------------
// Pin state is (B << 1) | A with both lines idling high (pull-ups), i.e. 3 at
// rest. A step is reported only when a whole valid cycle completes.
enum {
    R_START = 0x0,
    R_CW_FINAL, R_CW_BEGIN, R_CW_NEXT,
    R_CCW_BEGIN, R_CCW_FINAL, R_CCW_NEXT
};
#define DIR_NONE 0x00
#define DIR_CW   0x10
#define DIR_CCW  0x20

static const unsigned char s_stateTable[7][4] = {
    /* R_START     */ { R_START,    R_CW_BEGIN,  R_CCW_BEGIN, R_START },
    /* R_CW_FINAL  */ { R_CW_NEXT,  R_START,     R_CW_FINAL,  R_START | DIR_CW },
    /* R_CW_BEGIN  */ { R_CW_NEXT,  R_CW_BEGIN,  R_START,     R_START },
    /* R_CW_NEXT   */ { R_CW_NEXT,  R_CW_BEGIN,  R_CW_FINAL,  R_START },
    /* R_CCW_BEGIN */ { R_CCW_NEXT, R_START,     R_CCW_BEGIN, R_START },
    /* R_CCW_FINAL */ { R_CCW_NEXT, R_CCW_FINAL, R_START,     R_START | DIR_CCW },
    /* R_CCW_NEXT  */ { R_CCW_NEXT, R_CCW_FINAL, R_CCW_BEGIN, R_START },
};

// Feed in the new pin state. Returns +1 / -1 when a full step just completed
// (in ROTARY_ENCODER_* terms), otherwise 0. `state` is the decoder's memory.
static int decodeStep(unsigned char *state, unsigned char pinState)
{
    *state = s_stateTable[*state & 0x0F][pinState & 0x03];
    unsigned char dir = *state & 0x30;

    int step = 0;
    if (dir == DIR_CW) {
        step = ROTARY_ENCODER_CLOCKWISE;
    } else if (dir == DIR_CCW) {
        step = ROTARY_ENCODER_COUNTER_CLOCKWISE;
    }
#if ROTARY_ENCODER_REVERSE_DIRECTION
    step = -step;
#endif
    return step;
}


// --- Module state ----------------------------------------------------------
static pthread_t s_thread;
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static RotaryEncoder_stepCallback_t s_callback = NULL;      // protected by s_lock
static bool s_isInitialized = false;

static struct gpiod_line_request *s_request = NULL;
static struct gpiod_edge_event_buffer *s_events = NULL;
static unsigned int s_offsetA = 0;
static unsigned int s_offsetB = 0;
static int s_stopPipe[2] = { -1, -1 };      // write a byte to [1] to stop the thread


// --- Finding the GPIO lines ------------------------------------------------
// Look for a line called one of `names` (case-insensitive) on this chip.
static bool findLineOnChip(struct gpiod_chip *chip, const char *const *names,
                           unsigned int *offset)
{
    struct gpiod_chip_info *info = gpiod_chip_get_info(chip);
    if (info == NULL) {
        return false;
    }
    size_t numLines = gpiod_chip_info_get_num_lines(info);
    gpiod_chip_info_free(info);

    for (size_t i = 0; i < numLines; i++) {
        struct gpiod_line_info *lineInfo = gpiod_chip_get_line_info(chip, (unsigned int)i);
        if (lineInfo == NULL) {
            continue;
        }
        const char *lineName = gpiod_line_info_get_name(lineInfo);
        for (int n = 0; lineName != NULL && names[n] != NULL; n++) {
            if (strcasecmp(lineName, names[n]) == 0) {
                *offset = (unsigned int)i;
                gpiod_line_info_free(lineInfo);
                return true;
            }
        }
        gpiod_line_info_free(lineInfo);
    }
    return false;
}

// Search all GPIO chips for the chip that has both encoder lines.
static struct gpiod_chip *openChipWithEncoderLines(void)
{
    for (int c = 0; c < MAX_GPIO_CHIPS; c++) {
        char path[32];
        snprintf(path, sizeof(path), "/dev/gpiochip%d", c);

        struct gpiod_chip *chip = gpiod_chip_open(path);
        if (chip == NULL) {
            continue;
        }
        if (findLineOnChip(chip, ENCODER_A_NAMES, &s_offsetA)
            && findLineOnChip(chip, ENCODER_B_NAMES, &s_offsetB)) {
            return chip;
        }
        gpiod_chip_close(chip);
    }
    return NULL;
}

static bool requestLines(struct gpiod_chip *chip, bool withPullUp)
{
    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    struct gpiod_line_config *lineConfig = gpiod_line_config_new();
    struct gpiod_request_config *requestConfig = gpiod_request_config_new();
    if (settings == NULL || lineConfig == NULL || requestConfig == NULL) {
        gpiod_line_settings_free(settings);
        gpiod_line_config_free(lineConfig);
        gpiod_request_config_free(requestConfig);
        return false;
    }

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_edge_detection(settings, GPIOD_LINE_EDGE_BOTH);
    if (withPullUp) {
        gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_PULL_UP);
    }
    gpiod_request_config_set_consumer(requestConfig, "light_sampler");

    unsigned int offsets[2] = { s_offsetA, s_offsetB };
    if (gpiod_line_config_add_line_settings(lineConfig, offsets, 2, settings) == 0) {
        s_request = gpiod_chip_request_lines(chip, requestConfig, lineConfig);
    }

    gpiod_line_settings_free(settings);
    gpiod_line_config_free(lineConfig);
    gpiod_request_config_free(requestConfig);
    return s_request != NULL;
}


// --- Thread ----------------------------------------------------------------
static void reportStep(int direction)
{
    pthread_mutex_lock(&s_lock);
    RotaryEncoder_stepCallback_t callback = s_callback;
    pthread_mutex_unlock(&s_lock);

    if (callback != NULL) {
        callback(direction);            // called without our lock held
    }
}

static void *encoderThread(void *arg)
{
    (void)arg;

    // Starting pin state (idle should be both lines high = 3).
    unsigned char pinA = (gpiod_line_request_get_value(s_request, s_offsetA)
                          == GPIOD_LINE_VALUE_ACTIVE) ? 1 : 0;
    unsigned char pinB = (gpiod_line_request_get_value(s_request, s_offsetB)
                          == GPIOD_LINE_VALUE_ACTIVE) ? 1 : 0;
    unsigned char decoderState = R_START;

    struct pollfd fds[2];
    fds[0].fd = gpiod_line_request_get_fd(s_request);
    fds[0].events = POLLIN;
    fds[1].fd = s_stopPipe[0];
    fds[1].events = POLLIN;

    while (true) {
        fds[0].revents = 0;
        fds[1].revents = 0;
        if (poll(fds, 2, -1) < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("RotaryEncoder: poll");
            break;
        }
        if (fds[1].revents != 0) {
            break;                      // cleanup asked us to stop
        }
        if ((fds[0].revents & POLLIN) == 0) {
            continue;
        }

        int numEvents = gpiod_line_request_read_edge_events(s_request, s_events,
                                                            EVENT_BUFFER_SIZE);
        for (int i = 0; i < numEvents; i++) {
            struct gpiod_edge_event *event = gpiod_edge_event_buffer_get_event(s_events, (unsigned long)i);
            unsigned int offset = gpiod_edge_event_get_line_offset(event);
            unsigned char level =
                (gpiod_edge_event_get_event_type(event) == GPIOD_EDGE_EVENT_RISING_EDGE) ? 1 : 0;

            if (offset == s_offsetA) {
                pinA = level;
            } else if (offset == s_offsetB) {
                pinB = level;
            }

            int step = decodeStep(&decoderState, (unsigned char)((pinB << 1) | pinA));
            if (step != 0) {
                reportStep(step);
            }
        }
    }
    return NULL;
}


// --- Public functions ------------------------------------------------------
void RotaryEncoder_init(void)
{
    struct gpiod_chip *chip = openChipWithEncoderLines();
    if (chip == NULL) {
        fprintf(stderr,
                "ERROR: RotaryEncoder: couldn't find GPIO lines %s and %s.\n"
                "  Run 'gpioinfo | grep -i hat' on the board to see the real names,\n"
                "  then update ENCODER_A_NAMES / ENCODER_B_NAMES in rotary_encoder.c.\n",
                ENCODER_A_NAMES[0], ENCODER_B_NAMES[0]);
        exit(EXIT_FAILURE);
    }

    // Prefer an internal pull-up; if this GPIO driver can't set bias, retry
    // without it (the hat may have its own pull-ups).
    if (!requestLines(chip, true) && !requestLines(chip, false)) {
        perror("ERROR: RotaryEncoder: unable to request GPIO lines");
        gpiod_chip_close(chip);
        exit(EXIT_FAILURE);
    }
    gpiod_chip_close(chip);             // the request stays valid after this

    s_events = gpiod_edge_event_buffer_new(EVENT_BUFFER_SIZE);
    if (s_events == NULL || pipe(s_stopPipe) != 0) {
        perror("ERROR: RotaryEncoder: unable to allocate resources");
        exit(EXIT_FAILURE);
    }

    s_isInitialized = true;
    if (pthread_create(&s_thread, NULL, encoderThread, NULL) != 0) {
        perror("ERROR: RotaryEncoder: unable to create thread");
        exit(EXIT_FAILURE);
    }
}

void RotaryEncoder_cleanup(void)
{
    if (!s_isInitialized) {
        return;
    }

    // Wake the thread (it may be waiting for a knob turn that never comes).
    char wake = 'x';
    if (write(s_stopPipe[1], &wake, 1) < 0) {
        perror("RotaryEncoder: unable to signal thread");
    }
    pthread_join(s_thread, NULL);

    gpiod_edge_event_buffer_free(s_events);
    gpiod_line_request_release(s_request);
    close(s_stopPipe[0]);
    close(s_stopPipe[1]);
    s_events = NULL;
    s_request = NULL;
    s_stopPipe[0] = s_stopPipe[1] = -1;
    s_isInitialized = false;
}

void RotaryEncoder_setStepCallback(RotaryEncoder_stepCallback_t callback)
{
    pthread_mutex_lock(&s_lock);
    s_callback = callback;
    pthread_mutex_unlock(&s_lock);
}