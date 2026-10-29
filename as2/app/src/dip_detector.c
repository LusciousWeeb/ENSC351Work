// dip_detector.c
// Counts dips in light samples using a threshold with hysteresis.

#include "dip_detector.h"

#include <stdbool.h>

// A dip triggers when a sample is DIP_THRESHOLD_V or more BELOW the average
// (less light gives a lower voltage with this circuit; if your divider is
// wired the other way round, flip the two comparisons below).
#define DIP_THRESHOLD_V   0.10

// After a dip, the detector re-arms once a sample is back within
// (DIP_THRESHOLD_V - DIP_HYSTERESIS_V) = 0.07 V of the average.
#define DIP_HYSTERESIS_V  0.03

int DipDetector_countDips(const double *samples, int size, double average)
{
    int dips = 0;
    bool isArmed = true;

    for (int i = 0; i < size; i++) {
        double belowAverage = average - samples[i];     // > 0 when darker

        if (isArmed && belowAverage >= DIP_THRESHOLD_V) {
            dips++;
            isArmed = false;
        } else if (!isArmed
                   && belowAverage <= (DIP_THRESHOLD_V - DIP_HYSTERESIS_V)) {
            isArmed = true;
        }
    }
    return dips;
}