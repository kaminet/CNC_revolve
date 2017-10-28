#pragma once

#include <Automaton.h>

class Atm_stepper: public Machine {

 public:
  enum { IDLE, START, STEP_HIGH, STEP_LOW, LOOP, DONE }; // STATES
  enum { EVT_TIMER, EVT_COUNTER, EVT_START, EVT_STOP, ELSE }; // EVENTS
  Atm_stepper( void ) : Machine() {};
  Atm_stepper& begin( int stepPin, int dirPin, uint32_t stepDuration );
  Atm_stepper& trace( Stream & stream );
  Atm_stepper& trigger( int event );
  int state( void );
  Atm_stepper& onFinish( Machine& machine, int event = 0 );
  Atm_stepper& onFinish( atm_cb_push_t callback, int idx = 0 );
  Atm_stepper& start( void );
  Atm_stepper& stop( void );
  void setStep( int steps );
  void step( int steps );
  void setStepDuration( uint32_t stepDuration );
  uint32_t getStepDuration( void ) const;

 private:
  enum { ENT_IDLE, ENT_STEP_HIGH, ENT_STEP_LOW, ENT_DONE, ENT_START }; // ACTIONS
  enum { ON_FINISH, CONN_MAX }; // CONNECTORS
  atm_connector connectors[CONN_MAX];
  int event( int id );
  void action( int id );
  atm_timer_millis timer;
  uint32_t state_micros;
  uint32_t micros_timer;
  atm_counter counter;
  int32_t steps;
  int stepPin, dirPin;
};

/*
Automaton::ATML::begin - Automaton Markup Language

<?xml version="1.0" encoding="UTF-8"?>
<machines>
  <machine name="Atm_stepper">
    <states>
      <IDLE index="0" sleep="1" on_enter="ENT_IDLE">
        <EVT_START>STEP_HIGH</EVT_START>
      </IDLE>
      <STEP_HIGH index="1" on_enter="ENT_STEP_HIGH">
        <EVT_TIMER>STEP_LOW</EVT_TIMER>
        <EVT_STOP>IDLE</EVT_STOP>
      </STEP_HIGH>
      <STEP_LOW index="2" on_enter="ENT_STEP_LOW">
        <EVT_TIMER>LOOP</EVT_TIMER>
        <EVT_STOP>IDLE</EVT_STOP>
      </STEP_LOW>
      <LOOP index="3">
        <EVT_COUNTER>STEP_HIGH</EVT_COUNTER>
        <EVT_STOP>IDLE</EVT_STOP>
        <ELSE>DONE</ELSE>
      </LOOP>
      <DONE index="4" on_enter="ENT_DONE">
        <ELSE>IDLE</ELSE>
      </DONE>
    </states>
    <events>
      <EVT_TIMER index="0" access="PRIVATE"/>
      <EVT_COUNTER index="1" access="PRIVATE"/>
      <EVT_START index="2" access="PUBLIC"/>
      <EVT_STOP index="3" access="PUBLIC"/>
    </events>
    <connectors>
      <FINISH autostore="0" broadcast="0" dir="PUSH" slots="1"/>
    </connectors>
    <methods>
    </methods>
  </machine>
</machines>

Automaton::ATML::end
*/
