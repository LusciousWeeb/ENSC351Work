// sampler.c
// Background thread which samples the light sensor about once per millisecond.
//
// Samples collected during the current second go into s_current. Once per
// second the app calls Sampler_moveCurrentDataToHistory(), which:
//   - copies the current samples into s_history (the "previous second"),
//   - counts the dips in that second (via the dip_detector module),
//   - grabs the sampling-period statistics for that second.
// All shared data is protected by one mutex.

#include "sampler.h"

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "dip_detector.h"
#include "hal/light_sensor.h"
#include "periodTimer.h"

// Room for well over a second of samples.
#define MAX_SAMPLES 4096

// Exponential smoothing: new average = 99.9% previous + 0.1% latest sample
#define AVG_WEIGHT_PREVIOUS 0.999
#define AVG_WEIGHT_NEW      (1.0 - AVG_WEIGHT_PREVIOUS)

#define SAMPLE_DELAY_MS 1

static pthread_t s_thread;
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static bool s_isInitialized = false;
static bool s_isRunning = false;                // protected by s_lock

// Samples being collected during the current second.
static double s_current[MAX_SAMPLES];
static int s_currentSize = 0;

// Samples (and derived data) from the previous complete second.
static double s_history[MAX_SAMPLES];
static int s_historySize = 0;
static int s_historyDips = 0;
static Period_statistics_t s_historyStats;

// Statistics over all samples since startup.
static double s_average = 0.0;
static long long s_totalSamples = 0;


static void sleepForMs(long long delayMs)
{
    struct timespec delay;
    delay.tv_sec = delayMs / 1000;
    delay.tv_nsec = (delayMs % 1000) * 1000000;
    nanosleep(&delay, NULL);
}

static void *samplerThread(void *arg)
{
    (void)arg;

    bool keepRunning = true;
    while (keepRunning) {
        double voltage = LightSensor_getVoltage();
        Period_markEvent(PERIOD_EVENT_SAMPLE_LIGHT);

        pthread_mutex_lock(&s_lock);
        {
            // First sample seeds the average; after that, smooth it.
            if (s_totalSamples == 0) {
                s_average = voltage;
            } else {
                s_average = AVG_WEIGHT_PREVIOUS * s_average
                          + AVG_WEIGHT_NEW * voltage;
            }
            s_totalSamples++;

            // If the app stops rotating the history the buffer fills up;
            // extra samples are dropped (but still counted and averaged).
            if (s_currentSize < MAX_SAMPLES) {
                s_current[s_currentSize] = voltage;
                s_currentSize++;
            }

            keepRunning = s_isRunning;
        }
        pthread_mutex_unlock(&s_lock);

        if (keepRunning) {
            sleepForMs(SAMPLE_DELAY_MS);
        }
    }
    return NULL;
}


void Sampler_init(void)
{
    assert(!s_isInitialized);

    LightSensor_init();

    s_currentSize = 0;
    s_historySize = 0;
    s_historyDips = 0;
    memset(&s_historyStats, 0, sizeof(s_historyStats));
    s_average = 0.0;
    s_totalSamples = 0;
    s_isRunning = true;
    s_isInitialized = true;

    if (pthread_create(&s_thread, NULL, samplerThread, NULL) != 0) {
        perror("ERROR: Unable to create sampler thread");
        exit(EXIT_FAILURE);
    }
}

void Sampler_cleanup(void)
{
    assert(s_isInitialized);

    pthread_mutex_lock(&s_lock);
    s_isRunning = false;
    pthread_mutex_unlock(&s_lock);

    pthread_join(s_thread, NULL);

    LightSensor_cleanup();
    s_isInitialized = false;
}

void Sampler_moveCurrentDataToHistory(void)
{
    assert(s_isInitialized);

    // Fetch (and clear) the timing stats outside our lock; the period timer
    // has its own lock, and the sampler thread takes both in the other order.
    Period_statistics_t stats;
    Period_getStatisticsAndClear(PERIOD_EVENT_SAMPLE_LIGHT, &stats);

    pthread_mutex_lock(&s_lock);
    {
        memcpy(s_history, s_current, sizeof(s_current[0]) * (size_t)s_currentSize);
        s_historySize = s_currentSize;
        s_currentSize = 0;

        s_historyDips = DipDetector_countDips(s_history, s_historySize, s_average);
        s_historyStats = stats;
    }
    pthread_mutex_unlock(&s_lock);
}

int Sampler_getHistorySize(void)
{
    assert(s_isInitialized);

    pthread_mutex_lock(&s_lock);
    int size = s_historySize;
    pthread_mutex_unlock(&s_lock);
    return size;
}

double* Sampler_getHistory(int *size)
{
    assert(s_isInitialized);
    assert(size != NULL);

    double *copy = NULL;
    pthread_mutex_lock(&s_lock);
    {
        // Always allocate at least one element so the caller always gets a
        // pointer it can free(), even when the history is empty.
        size_t count = (s_historySize > 0) ? (size_t)s_historySize : 1;
        copy = malloc(sizeof(*copy) * count);
        if (copy == NULL) {
            pthread_mutex_unlock(&s_lock);
            perror("ERROR: Unable to allocate history copy");
            exit(EXIT_FAILURE);
        }
        memcpy(copy, s_history, sizeof(*copy) * (size_t)s_historySize);
        *size = s_historySize;
    }
    pthread_mutex_unlock(&s_lock);
    return copy;
}

Period_statistics_t Sampler_getLastSecondStatistics(void)
{
    assert(s_isInitialized);

    pthread_mutex_lock(&s_lock);
    Period_statistics_t stats = s_historyStats;
    pthread_mutex_unlock(&s_lock);
    return stats;
}

double Sampler_getAverageReading(void)
{
    assert(s_isInitialized);

    pthread_mutex_lock(&s_lock);
    double average = s_average;
    pthread_mutex_unlock(&s_lock);
    return average;
}

long long Sampler_getNumSamplesTaken(void)
{
    assert(s_isInitialized);

    pthread_mutex_lock(&s_lock);
    long long total = s_totalSamples;
    pthread_mutex_unlock(&s_lock);
    return total;
}

int Sampler_getDipCount(void)
{
    assert(s_isInitialized);

    pthread_mutex_lock(&s_lock);
    int dips = s_historyDips;
    pthread_mutex_unlock(&s_lock);
    return dips;
}