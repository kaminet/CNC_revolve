#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <Automaton.h>
#include "Atm_stepper.h"
#include "FS.h"
#include <WiFiClient.h>
#include <TimeLib.h>
#include <NtpClientLib.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266mDNS.h>
#include <Ticker.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <FSWebServerLib.h>
#include <Hash.h>

#include "NodeMCU-Hardware.h"

#define SERVO_DEBUG 1
#ifndef SERVO_DEBUG
  #define SERVO_DEBUG 0
#endif


Atm_stepper stepper;
Atm_led drillArm;
Atm_button runButton, drillButton;

/*
 *  Start a stepper motor running when a button is pressed
 *  Change motor direction on every new press of the button
 */

 // <FSWebServerLib.h>
 // #define CONNECTION_LED 16 // D0 Connection LED pin (Node LED). -1 to disable
 // #define AP_ENABLE_BUTTON 14 // D5 Button pin to enable AP during startup for configuration. -1 to disable

#define STEPS 500
#define STEP_PULSE 5000
#define STEP_PIN PIN_PWM_B
#define DIR_PIN PIN_DIR_B
#define RUN_BUTTON_PIN PIN_D7
#define RUN_BUTTON_DELAY 300
#define DRILL_BUTTON_PIN PIN_D6
#define DRILL_BUTTON_DELAY 300
#define DRILL_ARM_PIN PIN_D1


String data = "";

typedef struct {
	long position;
	float scale; // steps per unit
	float feed; // units per second
	uint32_t stepDuration; // time for one pulse
	float acceleration; // for trapezoidal movment
	bool enable = false; // arm motion allowed

	uint32_t calcStepduration( float scale, float feed); // calculate step duration from scale and feed
	void loadConfig ( void );
	void saveConfig ( void );
	void begin (float , float , float);
} strAxis;
strAxis xAxis ; // arm work data

uint32_t strAxis::calcStepduration( float scale, float feed){
  if ( scale ==0 || feed==0 ) {
  	stepDuration = STEP_PULSE;
	}
	else {
		stepDuration = 1000000/scale/feed;
	}
}
/*
void strAxis::begin( int scale, float feed, float acceleration){
  this->scale = scale;
	this->feed = feed;
	feed = calcStepduration( scale, feed) ;
	Serial.print("Calculated pulse length: ");
	Serial.println(stepDuration);
}*/

void strAxis::loadConfig ( void ) {
	ESPHTTPServer.load_user_config("xScale", scale);
	ESPHTTPServer.load_user_config("xFeed", feed);
	calcStepduration(scale, feed);
}
void strAxis::saveConfig ( void ) {
	ESPHTTPServer.save_user_config("xScale", scale);
	ESPHTTPServer.save_user_config("xFeed", feed);
}

typedef struct {
	int inMaxTime = 2000;
	int outMinTime = 200;
	int repeats = 5;
	bool activeLow = false;
	void loadConfig ( void );
	void saveConfig ( void );
} strDrill;
strDrill yDrill ; // arm work data

void strDrill::loadConfig ( void ) {
	String sTemp = "";
	int iTemp = 0;
	ESPHTTPServer.load_user_config("inMaxTime", inMaxTime);
	ESPHTTPServer.load_user_config("outMinTime", outMinTime);
	ESPHTTPServer.load_user_config("repeats", repeats);
	ESPHTTPServer.load_user_config("activeLow", sTemp );
	if ( sTemp[0] == '0' || sTemp[0] == 'f' || sTemp[0] == 'F' ) activeLow = false;
	else activeLow = true;
}
void strDrill::saveConfig ( void ) {
	ESPHTTPServer.save_user_config("inMaxTime", inMaxTime);
	ESPHTTPServer.save_user_config("outMinTime", outMinTime);
	ESPHTTPServer.save_user_config("repeats", repeats);
	ESPHTTPServer.save_user_config("activeLow", activeLow);
}

int steps = STEPS;
int runButtonDelay = RUN_BUTTON_DELAY;

// configure callbacks
void  callbackJSON(AsyncWebServerRequest *request) {
	//its possible to test the url and do different things,
	String values = "{\"message\": \"Hello world! \" , \"url\":\"" + request->url() + "\"}";
	request->send(200, "text/plain", values);
	values = "";
}

void  callbackREST(AsyncWebServerRequest *request) {
	//its possible to test the url and do different things,
	//test you rest URL
	if (request->url() == "/rest/userdemo")	{
		//contruct and send and desired repsonse
		// get sample data from json file
		String data = "";
		ESPHTTPServer.load_user_config("user1", data);
		String values = "user1|"+ data +"|input\n";

		ESPHTTPServer.load_user_config("user2", data);
		values += "user2|" + data + "|input\n";

		ESPHTTPServer.load_user_config("user3", data);
		values += "user3|" + data + "|input\n";
		request->send(200, "text/plain", values);
		values = "";
	}
	else {
		//its possible to test the url and do different things,
		String values = "message:Hello world! \nurl:" + request->url() + "\n";
		request->send(200, "text/plain", values);
		values = "";
	}
}

void  callbackPOST(AsyncWebServerRequest *request) {
String values = "";
	//its possible to test the url and do different things,
	if (request->url() == "/post/user")
	{
		String target = "/";

		       for (uint8_t i = 0; i < request->args(); i++) {
            DEBUGLOG("Arg %d: %s\r\n", i, request->arg(i).c_str());
			Serial.print(request->argName(i));
			Serial.print(" : ");
			Serial.println(ESPHTTPServer.urldecode(request->arg(i)));

			//check for post redirect
			if (request->argName(i) == "afterpost")
			{
				target = ESPHTTPServer.urldecode(request->arg(i));
			}
			else  //or savedata in Json File
			{
				ESPHTTPServer.save_user_config(request->argName(i), request->arg(i));
			}
        }

		request->redirect(target);

	}
	else {
		String values = "message:Hello world! \nurl:" + request->url() + "\n";
		request->send(200, "text/plain", values);
		values = "";
	}
}
// prepare handlers
auto handler_stepper_values = ESPHTTPServer.on("/stepper_values", HTTP_GET, [](AsyncWebServerRequest *request) {
	    for (uint8_t i = 0; i < request->args(); i++) {
	      DEBUGLOG("Arg %d: %s\r\n", i, request->arg(i).c_str());
				Serial.print(request->argName(i));
				Serial.print(" : ");
				Serial.println(ESPHTTPServer.urldecode(request->arg(i)));
				if (request->argName(i) == "enable") {
				}
				if (request->argName(i) == "stepDuration") {
					stepper.setStepDuration( request->arg(i).toInt() );
				}
				if (request->argName(i) == "step") {
					stepper.step( request->arg(i).toInt() );
				}
	    }
			String values = "";
			values = "stepDuration|" + (String)stepper.getStepDuration() + "|input\n";
			// values += "step|" + (String)steps + "|input\n";
			request->send(200, "text/plain", values);
	});

auto handler_stepper = ESPHTTPServer.on("/stepper", HTTP_GET, [](AsyncWebServerRequest *request) {
		String values = "";
    for (uint8_t i = 0; i < request->args(); i++) {
      DEBUGLOG("Arg %d: %s\r\n", i, request->arg(i).c_str());
			Serial.print(request->argName(i));
			Serial.print(" : ");
			Serial.println(ESPHTTPServer.urldecode(request->arg(i)));
    }
	  request->send(SPIFFS, "/stepper.html");
});
auto handler_axis = ESPHTTPServer.on("/axis", HTTP_GET, [](AsyncWebServerRequest *request) {
		String target = "/";
		// String values = "";
    for (uint8_t i = 0; i < request->args(); i++) {
      DEBUGLOG("Arg %d: %s\r\n", i, request->arg(i).c_str());
			Serial.print(request->argName(i));
			Serial.print(" : ");
			Serial.println(ESPHTTPServer.urldecode(request->arg(i)));

			//check for post redirect
			if (request->argName(i) == "afterpost")	{
				target = ESPHTTPServer.urldecode(request->arg(i));
			}
			else {  //or savedata in Json File
				ESPHTTPServer.save_user_config(request->argName(i), request->arg(i));
			}
    }
		xAxis.loadConfig();
	  xAxis.calcStepduration( xAxis.scale, xAxis.feed );
		stepper.setStepDuration(xAxis.stepDuration);
		ESPHTTPServer.load_user_config("xAction01", steps);
		ESPHTTPServer.load_user_config("runButtonDelay", runButtonDelay);
		request->send(SPIFFS, "/axis.html");
		runButton.longPress(2, runButtonDelay);
});
auto handler_axis_values = ESPHTTPServer.on("/axis_values", HTTP_GET, [](AsyncWebServerRequest *request) {
			String values = "";
			values = "xScale|" + (String)xAxis.scale + "|input\n";
			values += "xFeed|" + (String)xAxis.feed + "|input\n";
			values += "xAction01|" + (String)steps + "|input\n";
			values += "runButtonDelay|" + (String)runButtonDelay + "|input\n";
			request->send(200, "text/plain", values);
			values = "";
});
auto handler_arm = ESPHTTPServer.on("/arm", HTTP_GET, [](AsyncWebServerRequest *request) {
		String target = "/";
		// String values = "";
    for (uint8_t i = 0; i < request->args(); i++) {
      DEBUGLOG("Arg %d: %s\r\n", i, request->arg(i).c_str());
			#if SERVO_DEBUG
				Serial.print(request->argName(i));
				Serial.print(" : ");
				Serial.println(ESPHTTPServer.urldecode(request->arg(i)));
			#endif
			//check for post redirect
			if (request->argName(i) == "afterpost")	{
				target = ESPHTTPServer.urldecode(request->arg(i));
			}
			if (request->argName(i) == "drillNow")	{
				drillArm.trigger( drillArm.EVT_BLINK );
			}
			// if (request->argName(i) == "activeLow") {
			// 	if ( request->arg(i) == "0" ) yDrill.activeLow = false;
			// 	else yDrill.activeLow = true;
			// }
			else {  //or savedata in Json File
				ESPHTTPServer.save_user_config(request->argName(i), request->arg(i));
			}
    }
		yDrill.loadConfig();
		request->send(SPIFFS, "/arm.html");
		drillArm
			.blink( yDrill.inMaxTime, yDrill.outMinTime )
			.repeat( yDrill.repeats );
			// .activeLow ( yDrill.activeLow  );

});
auto handler_arm_values = ESPHTTPServer.on("/arm_values", HTTP_GET, [](AsyncWebServerRequest *request) {
			String values = "";
			values = "inMaxTime|" + (String)yDrill.inMaxTime + "|input\n";
			values += "outMinTime|" + (String)yDrill.outMinTime + "|input\n";
			values += "repeats|" + (String)yDrill.repeats + "|input\n";
			values += "activeLow|" + (String)(yDrill.activeLow ? "1" : "0") + "|input\n";
			request->send(200, "text/plain", values);
			values = "";
});

void setup() {
    // put your setup code here, to run once:
  // WiFi is started inside library
  SPIFFS.begin(); // Not really needed, checked inside library and started if needed
  ESPHTTPServer.begin(&SPIFFS);
  // /* add setup code here */
	//
	// //set optioanl callback
	ESPHTTPServer.setJSONCallback(callbackJSON);
	//
	// //set optioanl callback
	ESPHTTPServer.setRESTCallback(callbackREST);
	//
	// //set optioanl callback
	ESPHTTPServer.setPOSTCallback(callbackPOST);


// Prepare Automatons
  stepper.begin( STEP_PIN, DIR_PIN, STEP_PULSE );
	xAxis.loadConfig();
  xAxis.calcStepduration( xAxis.scale, xAxis.feed );
	yDrill.loadConfig();
	stepper.setStepDuration(xAxis.stepDuration);
	ESPHTTPServer.load_user_config("xAction01", steps);
	ESPHTTPServer.load_user_config("runButtonDelay", runButtonDelay);
	// data == "" ? steps = STEPS : steps = data.toInt();
	runButton.begin( RUN_BUTTON_PIN );
	runButton.onPress( [] ( int idx, int v, int up ) {
		switch (v) {
			case 1:
				Serial.println("button to short");
				return;
			case 2:
			Serial.print("scale: ");
			Serial.print(xAxis.scale);
			Serial.print("  |   ");
			Serial.print("feed: ");
			Serial.print(xAxis.feed);
			Serial.print("  |   ");
			Serial.print("action: ");
			Serial.print(steps);
			Serial.print("  |   ");
			Serial.print("duration: ");
			Serial.println(stepper.getStepDuration());
			// stepper.step( steps*xAxis.scale );
			drillArm.trigger( drillArm.EVT_BLINK );
			return;
		}
	});
	runButton.longPress(2, runButtonDelay);

	drillArm.begin( DRILL_ARM_PIN , yDrill.activeLow )
	// drillArm.begin( DRILL_ARM_PIN , true )
    // .lead( 200 )
    .blink( yDrill.inMaxTime, yDrill.outMinTime )
    .repeat( yDrill.repeats );

	drillArm.onFinish( [] ( int idx, int v, int up ){
		Serial.println("drillArm DONE");
		stepper.step( steps*xAxis.scale );
	});

	drillButton.begin( DRILL_BUTTON_PIN )
	  // .onPress( drillArm, drillArm.EVT_OFF_TIMER );
	  .onPress( drillArm, drillArm.EVT_ON_TIMER );

	stepper.onFinish( [] ( int idx, int v, int up ){
		Serial.println("stepper DONE");
	});
}

void loop() {
    // put your main code here, to run repeatedly:

  // DO NOT REMOVE. Attend OTA update from Arduino IDE
  ESPHTTPServer.handle();
	automaton.run();

}
