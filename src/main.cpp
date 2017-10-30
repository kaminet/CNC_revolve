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

Atm_stepper stepper;
Atm_button button;

/*
 *  Start a stepper motor running when a button is pressed
 *  Change motor direction on every new press of the button
 */

#define STEPS 500
#define STEP_PULSE 5000
#define STEP_PIN PIN_PWM_B
#define DIR_PIN PIN_DIR_B
#define BUTTON_PIN PIN_D3

String data = "";

typedef struct {
	long position;
	float scale; // steps per unit
	float feed; // units per second
	uint32_t stepDuration; // time for one pulse
	float acceleration; // for trapezoidal movment
	bool enable = false; // arm motion allowed

	uint32_t calcStepduration( float scale, float feed); // calculate step duration from scale and feed
	void loadAxis ( void );
	void saveAxis ( void );
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

void strAxis::loadAxis ( void ) {
	ESPHTTPServer.load_user_config("xScale", scale);
	ESPHTTPServer.load_user_config("xFeed", feed);
	calcStepduration(scale, feed);
}

void strAxis::saveAxis ( void ) {
	ESPHTTPServer.save_user_config("xScale", scale);
	ESPHTTPServer.save_user_config("xFeed", feed);
}


int steps = STEPS;


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
			values = "";

			xAxis.loadAxis();
		  xAxis.calcStepduration( xAxis.scale, xAxis.feed );
			stepper.setStepDuration(xAxis.stepDuration);
			ESPHTTPServer.load_user_config("xAction01", steps);
			//ESPHTTPServer.handleFileRead("/stepper.html", request); // for it to work u need edit mthod in liblary .h from private to public
			request->send(SPIFFS, "/stepper.html");
	});

auto handler_stepper = ESPHTTPServer.on("/stepper", HTTP_GET, [](AsyncWebServerRequest *request) {
		String values = "";
    for (uint8_t i = 0; i < request->args(); i++) {
      DEBUGLOG("Arg %d: %s\r\n", i, request->arg(i).c_str());
			Serial.print(request->argName(i));
			Serial.print(" : ");
			Serial.println(ESPHTTPServer.urldecode(request->arg(i)));
    }
		xAxis.loadAxis();
	  xAxis.calcStepduration( xAxis.scale, xAxis.feed );
		stepper.setStepDuration(xAxis.stepDuration);
		ESPHTTPServer.load_user_config("xAction01", steps);
		//ESPHTTPServer.handleFileRead("/stepper.html", request); // for it to work u need edit mthod in liblary .h from private to public
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
		xAxis.loadAxis();
	  xAxis.calcStepduration( xAxis.scale, xAxis.feed );
		stepper.setStepDuration(xAxis.stepDuration);
		ESPHTTPServer.load_user_config("xAction01", steps);
		request->send(SPIFFS, "/axis.html");
});
auto handler_axis_values = ESPHTTPServer.on("/axis_values", HTTP_GET, [](AsyncWebServerRequest *request) {
			String values = "";
			values = "xScale|" + (String)xAxis.scale + "|input\n";
			values += "xFeed|" + (String)xAxis.feed + "|input\n";
			values += "xAction01|" + (String)steps + "|input\n";
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
	xAxis.loadAxis();
  xAxis.calcStepduration( xAxis.scale, xAxis.feed );
	stepper.setStepDuration(xAxis.stepDuration);
	ESPHTTPServer.load_user_config("xAction01", steps);
	// data == "" ? steps = STEPS : steps = data.toInt();
	button.begin( BUTTON_PIN );

	button.onPress( [] ( int idx, int v, int up ) {
	    stepper.step( steps*xAxis.scale );
	    // steps = steps * -1; // Reverse direction on each button push

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

	});
}

void loop() {
    // put your main code here, to run repeatedly:

  // DO NOT REMOVE. Attend OTA update from Arduino IDE
  ESPHTTPServer.handle();
	automaton.run();

}
