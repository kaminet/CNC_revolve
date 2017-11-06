#pragma once

#include <Automaton.h>

class Atm_stepper: public Machine {

 public:
  enum { IDLE, RESET, START, STEP_HIGH, STEP_LOW, LOOP, DONE }; // STATES
  enum { EVT_TIMER, EVT_COUNTER, EVT_RESET, EVT_START, EVT_STOP, ELSE }; // EVENTS
  Atm_stepper( void ) : Machine() {};
  Atm_stepper& begin( int stepPin, int dirPin, uint32_t stepDuration );
  Atm_stepper& trace( Stream & stream );
  Atm_stepper& trigger( int event );
  int state( void );
  Atm_stepper& onFinish( Machine& machine, int event = 0 );
  Atm_stepper& onFinish( atm_cb_push_t callback, int idx = 0 );
  Atm_stepper& reset( void );
  Atm_stepper& start( void );
  Atm_stepper& stop( void );
  void setStep( int steps );
  void step( int steps );
  void setStepDuration( uint32_t stepDuration );
  uint32_t getStepDuration( void ) const;

 private:
  enum { ENT_IDLE, ENT_RESET, ENT_START, ENT_STEP_HIGH, ENT_STEP_LOW, ENT_DONE }; // ACTIONS
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
        <EVT_RESET>RESET</EVT_RESET>
        <EVT_START>START</EVT_START>
      </IDLE>
      <RESET index="1" on_enter="ENT_RESET">
        <EVT_RESET>RESET</EVT_RESET>
        <ELSE>IDLE</ELSE>
      </RESET>
      <START index="2" on_enter="ENT_START">
        <EVT_COUNTER>DONE</EVT_COUNTER>
        <EVT_RESET>RESET</EVT_RESET>
        <ELSE>STEP_HIGH</ELSE>
      </START>
      <STEP_HIGH index="3" on_enter="ENT_STEP_HIGH">
        <EVT_TIMER>STEP_LOW</EVT_TIMER>
        <EVT_RESET>RESET</EVT_RESET>
        <EVT_START>START</EVT_START>
        <EVT_STOP>IDLE</EVT_STOP>
      </STEP_HIGH>
      <STEP_LOW index="4" on_enter="ENT_STEP_LOW">
        <EVT_TIMER>LOOP</EVT_TIMER>
        <EVT_RESET>RESET</EVT_RESET>
        <EVT_START>START</EVT_START>
        <EVT_STOP>IDLE</EVT_STOP>
      </STEP_LOW>
      <LOOP index="5">
        <EVT_COUNTER>DONE</EVT_COUNTER>
        <EVT_RESET>RESET</EVT_RESET>
        <EVT_STOP>IDLE</EVT_STOP>
        <ELSE>STEP_HIGH</ELSE>
      </LOOP>
      <DONE index="6" on_enter="ENT_DONE">
        <EVT_RESET>RESET</EVT_RESET>
        <EVT_START>START</EVT_START>
        <ELSE>IDLE</ELSE>
      </DONE>
    </states>
    <events>
      <EVT_TIMER index="0" access="PRIVATE"/>
      <EVT_COUNTER index="1" access="PRIVATE"/>
      <EVT_RESET index="2" access="PUBLIC"/>
      <EVT_START index="3" access="PUBLIC"/>
      <EVT_STOP index="4" access="PUBLIC"/>
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

