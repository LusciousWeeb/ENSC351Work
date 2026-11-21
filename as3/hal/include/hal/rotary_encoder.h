// hal/rotary_encoder.h
// Hardware abstraction for the rotary encoder, read with the gpiod library
// (no external programs). One "step" is one detent of the knob.
//
// The module runs its own thread which watches the encoder's GPIO lines and
// reports each step through a callback, so the rest of the program never has
// to poll or worry about the GPIO details.
//
// Typical use:
//   static void onStep(int direction) {   // +1 = clockwise, -1 = counter-clockwise
//       ...change the LED frequency by `direction` Hz...
//   }
//   RotaryEncoder_init();
//   RotaryEncoder_setStepCallback(onStep);
//   ...
//   RotaryEncoder_cleanup();              // joins the thread, closes the GPIO lines

#ifndef HAL_ROTARY_ENCODER_H
#define HAL_ROTARY_ENCODER_H

// Direction values passed to the step callback.
#define ROTARY_ENCODER_CLOCKWISE          1
#define ROTARY_ENCODER_COUNTER_CLOCKWISE (-1)

// Called once per step of the knob. `direction` is ROTARY_ENCODER_CLOCKWISE
// or ROTARY_ENCODER_COUNTER_CLOCKWISE.
//
// The callback runs on the encoder's own thread, so it must be:
//   - quick (the encoder can't see further steps while it runs), and
//   - thread-safe (it touches data used by other threads).
// It must not call RotaryEncoder_cleanup().
typedef void (*RotaryEncoder_stepCallback_t)(int direction);

// Open the GPIO lines and start the thread that watches them.
// Exits the program with an error message if the GPIO lines can't be opened.
void RotaryEncoder_init(void);

// Stop the thread and close the GPIO lines. Returns promptly (it does not wait
// for the knob to move). No steps are reported once this has returned.
void RotaryEncoder_cleanup(void);

// Choose the function called on each step. Pass NULL to ignore steps.
// May be called before or after RotaryEncoder_init(), and safely while the
// encoder is running.
void RotaryEncoder_setStepCallback(RotaryEncoder_stepCallback_t callback);

#endif // HAL_ROTARY_ENCODER_H