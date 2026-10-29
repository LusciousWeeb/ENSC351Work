// terminal_out.h
// Module which prints the light-sampler status to the terminal once a second.
//
// It runs its own thread. Each second the thread:
//   1. calls Sampler_moveCurrentDataToHistory() (this module is the one
//      place that triggers the once-per-second history rotation),
//   2. prints line 1: samples taken, LED flash frequency, average light level,
//      dip count and sampling-jitter statistics for the previous second,
//   3. prints line 2: 10 evenly spaced samples from the previous second.
//
// Startup order: Period_init(), Sampler_init(), then TerminalOut_init().
// Shutdown in the reverse order: TerminalOut_cleanup() first.
 
#ifndef TERMINAL_OUT_H
#define TERMINAL_OUT_H
 
// Start/stop the background printing thread.
// Cleanup wakes the thread immediately (it does not wait out the second).
void TerminalOut_init(void);
void TerminalOut_cleanup(void);
 
// Tell the terminal what frequency the LED is currently flashing at, so it can
// be shown on line 1. Call this from whichever module sets the LED frequency
// (rotary encoder / PWM code). Thread-safe. Until it is called, 0 Hz is shown.
void TerminalOut_setFlashHz(int hz);
 
#endif
