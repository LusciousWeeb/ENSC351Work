// dip_detector.h
// Module which counts "dips" in a set of light samples (assignment section 1.3).
//
// A dip is when the light level drops 0.1 V or more below the average light
// level. Once a dip has been counted, another one can't be counted until the
// light level has recovered to within 0.07 V of the average (hysteresis of
// 0.03 V), which stops noise from being counted as many dips.
//
// This module is a plain function: no thread, no stored state, so it is
// thread-safe as long as the caller's array isn't being modified.

#ifndef DIP_DETECTOR_H
#define DIP_DETECTOR_H

// Count the dips in `samples` (voltages, in time order).
//   samples: array of `size` voltages (may be NULL if size is 0)
//   size:    number of samples
//   average: the current exponentially smoothed average light level (volts)
// Returns the number of dips found.
int DipDetector_countDips(const double *samples, int size, double average);

#endif