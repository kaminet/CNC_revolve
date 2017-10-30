#include "Atm_stepper.h"

/* Add optional parameters for the state machine to begin()
 * Add extra initialization code
 */

Atm_stepper& Atm_stepper::begin( int stepPin, int dirPin, uint32_t stepDuration ) {
  // clang-format off
  const static state_t state_table[] PROGMEM = {
    /*                   ON_ENTER    ON_LOOP   ON_EXIT  EVT_TIMER  EVT_COUNTER  EVT_START  EVT_STOP       ELSE */
    /*      IDLE */      ENT_IDLE, ATM_SLEEP,       -1,        -1,          -1,     START,       -1,        -1,
    /*     START */     ENT_START,        -1,       -1,        -1,        DONE,        -1,       -1, STEP_HIGH,
    /* STEP_HIGH */ ENT_STEP_HIGH,        -1,       -1,  STEP_LOW,          -1,     START,     IDLE,        -1,
    /*  STEP_LOW */  ENT_STEP_LOW,        -1,       -1,      LOOP,          -1,     START,     IDLE,        -1,
    /*      LOOP */            -1,        -1,       -1,        -1,        DONE,        -1,     IDLE, STEP_HIGH,
    /*      DONE */      ENT_DONE,        -1,       -1,        -1,          -1,     START,       -1,      IDLE,
  };
  // clang-format on
  this->stepPin = stepPin;
  this->dirPin = dirPin;
  pinMode( stepPin, OUTPUT );
  pinMode( dirPin, OUTPUT );
  micros_timer = stepDuration / 2;
  counter.set( 0 );
  Machine::begin( state_table, ELSE );
  return *this;
}

/* Add C++ code for each internally handled event (input)
 * The code must return 1 to trigger the event
 */

int Atm_stepper::event( int id ) {
  switch ( id ) {
    case EVT_COUNTER:
      return counter.expired();
    case EVT_TIMER:
      return micros() - state_micros >= micros_timer;
  }
  return 0;
}

/* Add C++ code for each action
 * This generates the 'output' for the state machine
 *
 * Available connectors:
 *   push( connectors, ON_FINISH, 0, <v>, <up> );
 */

void Atm_stepper::action( int id ) {
  switch ( id ) {
    case ATM_ON_SWITCH:
      state_micros = micros();
      return;
    case ENT_IDLE:
      digitalWrite( stepPin, LOW );
      digitalWrite( dirPin, LOW );
      return;
    case ENT_START:
      digitalWrite( stepPin, LOW );
      if ( steps < 0 ) {
        digitalWrite( dirPin, LOW );
        counter.set( counter.value + steps * -1 );
      } else {
        digitalWrite( dirPin, HIGH );
        counter.set( counter.value + steps );
      }
      return;
    case ENT_STEP_HIGH:
      digitalWrite( stepPin, HIGH );
      return;
    case ENT_STEP_LOW:
      digitalWrite( stepPin, LOW );
      counter.decrement();
      return;
    case ENT_DONE:
      digitalWrite( stepPin, LOW );
      // this->steps = 0;
      push( connectors, ON_FINISH, 0, 0, 0 );
      return;
  }
}

/* Optionally override the default trigger() method
 * Control how your machine processes triggers
 */

Atm_stepper& Atm_stepper::trigger( int event ) {
  Machine::trigger( event );
  return *this;
}

/* Optionally override the default state() method
 * Control what the machine returns when another process requests its state
 */

int Atm_stepper::state( void ) {
  return Machine::state();
}


void Atm_stepper::setStep( int steps ) {

  this->steps = steps;
}

void Atm_stepper::step( int steps ) {

  this->steps = steps;
  trigger( EVT_START );
}

void Atm_stepper::setStepDuration( uint32_t stepDuration) {

  micros_timer = stepDuration / 2;
}

uint32_t Atm_stepper::getStepDuration( void ) const {

  return micros_timer*2;
}

/* Nothing customizable below this line
 ************************************************************************************************
*/

/* Public event methods
 *
 */

Atm_stepper& Atm_stepper::start() {
  trigger( EVT_START );
  return *this;
}

Atm_stepper& Atm_stepper::stop() {
  trigger( EVT_STOP );
  return *this;
}

Atm_stepper& Atm_stepper::reset() {
  // this->steps = 0;
  this->counter.set(0);
  this->timer.set(0);
  // this->micros_timer = 0;
  trigger( EVT_STOP );
  return *this;
}

/*
 * onFinish() push connector variants ( slots 1, autostore 0, broadcast 0 )
 */

Atm_stepper& Atm_stepper::onFinish( Machine& machine, int event ) {
  onPush( connectors, ON_FINISH, 0, 1, 1, machine, event );
  return *this;
}

Atm_stepper& Atm_stepper::onFinish( atm_cb_push_t callback, int idx ) {
  onPush( connectors, ON_FINISH, 0, 1, 1, callback, idx );
  return *this;
}

/* State trace method
 * Sets the symbol table and the default logging method for serial monitoring
 */

Atm_stepper& Atm_stepper::trace( Stream & stream ) {
  Machine::setTrace( &stream, atm_serial_debug::trace,
    "STEPPER\0EVT_TIMER\0EVT_COUNTER\0EVT_START\0EVT_STOP\0ELSE\0IDLE\0START\0STEP_HIGH\0STEP_LOW\0LOOP\0DONE" );
  return *this;
}
