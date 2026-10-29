// terminal_out.c
// Prints the light-sampler status once per second from its own thread.

#include "terminal_out.h"

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "periodTimer.h"
#include "sampler.h"

#define PRINT_PERIOD_SEC 1
#define NUM_SAMPLES_TO_SHOW 10

static pthread_t s_thread;
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_cond;           // signalled by cleanup to wake the thread
static bool s_isInitialized = false;
static bool s_isRunning = false;        // protected by s_lock
static int s_flashHz = 0;               // protected by s_lock


// Print the two lines of output for the previous second.
static void printStatus(void)
{
    int size = 0;
    double *history = Sampler_getHistory(&size);
    Period_statistics_t stats = Sampler_getLastSecondStatistics();

    pthread_mutex_lock(&s_lock);
    int flashHz = s_flashHz;
    pthread_mutex_unlock(&s_lock);

    // Line 1: fixed-width fields so the columns stay aligned.
    printf("#Smpl/s = %4d    Flash @ %3dHz    avg = %5.3fV   dips = %3d    "
           "Smpl ms[%6.3f, %6.3f] avg %5.3f/%d\n",
           size,
           flashHz,
           Sampler_getAverageReading(),
           Sampler_getDipCount(),
           stats.minPeriodInMs,
           stats.maxPeriodInMs,
           stats.avgPeriodInMs,
           stats.numSamples);

    // Line 2: up to 10 samples, evenly spaced across the history.
    // With fewer than 10 samples, show them all.
    int numToShow = (size < NUM_SAMPLES_TO_SHOW) ? size : NUM_SAMPLES_TO_SHOW;
    for (int i = 0; i < numToShow; i++) {
        int index = i;
        if (size >= NUM_SAMPLES_TO_SHOW) {
            index = (int)((double)i * size / NUM_SAMPLES_TO_SHOW + 0.5);
        }
        printf(" %3d:%5.3f", index, history[index]);
    }
    printf("\n");
    fflush(stdout);

    free(history);
}

static void *terminalThread(void *arg)
{
    (void)arg;

    // Wake up at fixed 1 s intervals (absolute times), so the period doesn't
    // drift by however long printing takes.
    struct timespec nextWake;
    clock_gettime(CLOCK_MONOTONIC, &nextWake);

    pthread_mutex_lock(&s_lock);
    while (s_isRunning) {
        nextWake.tv_sec += PRINT_PERIOD_SEC;

        // Sleep until the next second, or until cleanup signals us to stop.
        // (timedwait unlocks the mutex while waiting.)
        while (s_isRunning
               && pthread_cond_timedwait(&s_cond, &s_lock, &nextWake) == 0) {
            // Spurious/stop wake-up: loop re-checks s_isRunning.
        }
        if (!s_isRunning) {
            break;
        }

        // Don't hold our lock while talking to the sampler.
        pthread_mutex_unlock(&s_lock);
        Sampler_moveCurrentDataToHistory();
        printStatus();
        pthread_mutex_lock(&s_lock);
    }
    pthread_mutex_unlock(&s_lock);
    return NULL;
}


void TerminalOut_init(void)
{
    assert(!s_isInitialized);

    // Condition variable on the monotonic clock so wall-clock changes can't
    // stretch or shrink the 1 s period.
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&s_cond, &attr);
    pthread_condattr_destroy(&attr);

    s_isRunning = true;
    s_isInitialized = true;

    if (pthread_create(&s_thread, NULL, terminalThread, NULL) != 0) {
        perror("ERROR: Unable to create terminal output thread");
        exit(EXIT_FAILURE);
    }
}

void TerminalOut_cleanup(void)
{
    assert(s_isInitialized);

    pthread_mutex_lock(&s_lock);
    s_isRunning = false;
    pthread_cond_signal(&s_cond);
    pthread_mutex_unlock(&s_lock);

    pthread_join(s_thread, NULL);
    pthread_cond_destroy(&s_cond);
    s_isInitialized = false;
}

void TerminalOut_setFlashHz(int hz)
{
    pthread_mutex_lock(&s_lock);
    s_flashHz = hz;
    pthread_mutex_unlock(&s_lock);
}