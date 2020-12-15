/*
  ESP8266AutoIOT.cpp - Library for initiating a WiFi connection and managing a server.
  Created by Robert Reed, September 25, 2020.
  Released into the public domain.
*/

#include "ESP8266AutoIOT.h"
#include "Arduino.h"
#include <ESP8266WiFi.h>        //https://github.com/esp8266/Arduino
#include <DNSServer.h>          // Included with core
#include <ESP8266WebServer.h>   // Included with core
#include <WiFiManager.h>        //https://github.com/RobertMcReed/WiFiManager.git#ota
#include <ESP8266mDNS.h>        // Included with core
#include <ArduinoOTA.h>         // Included with core

const unsigned long CONNECT_TIMEOUT = 30; // How long to attempt to connect to saved WiFi before going into AP mode
const unsigned long AP_TIMEOUT = 60; // Wait 60 Seconds in the config portal before trying again the original WiFi creds

void ESP8266AutoIOT::_setup(bool enableOTA)
{
  _otaEnabled = enableOTA;
  _hasBegun = false;

  if (!_accessPoint)
  {
    _accessPoint = (char*)"esp8266";
  }

  if (!_password)
  {
    _password = (char*)"newcouch";
  }

  _LED_ON = LOW;
  _LED_OFF = HIGH;
  _LED = LED_BUILTIN;
  _ledEnabled = true;

  _corsOrigin = "*";
  _corsEnabled = false;

  WiFiManager wifiManager;
  server = new ESP8266WebServer(80);
}

ESP8266AutoIOT::ESP8266AutoIOT()
{
  _setup(false);
}

ESP8266AutoIOT::ESP8266AutoIOT(bool enableOTA)
{
  _setup(enableOTA);
}

ESP8266AutoIOT::ESP8266AutoIOT(char* accessPoint, char* password)
{
  _accessPoint = accessPoint;
  _password = password;
  _setup(true);
}

ESP8266AutoIOT::ESP8266AutoIOT(char* accessPoint, char* password, bool enableOTA)
{
  _accessPoint = accessPoint;
  _password = password;
  _setup(enableOTA);
}

void ESP8266AutoIOT::_digitalWrite(int value)
{
   if (_ledEnabled)
   {
     digitalWrite(_LED, value);
   }
}

void ESP8266AutoIOT::_sendCorsHeaderIfEnabled()
{
   if (_corsEnabled)
   {
     server->sendHeader("Access-Control-Allow-Origin", _corsOrigin);
   }
}

void ESP8266AutoIOT::_handleGetRequestVoidFn(voidCallback fn)
{
  _ledOn();
  if (server->method() == HTTP_GET) {
    _sendCorsHeaderIfEnabled();
    fn();
    server->send(200, "text/plain", "Success");
  } else {
    server->send(405, "text/plain", "Method Not Allowed");
  }
  _ledOff();
}

void ESP8266AutoIOT::_handleGetRequestStr(String response, bool isHtml)
{
  _ledOn();
  if (server->method() == HTTP_GET) {
    _sendCorsHeaderIfEnabled();
    const char* contentType = isHtml ? "text/html" : "text/plain";
    server->send(200, contentType, response);
  } else {
    server->send(405, "text/plain", "Method Not Allowed");
  }
  _ledOff();
}

void ESP8266AutoIOT::_handleGetRequestStrFn(stringCallback fn, bool isHtml)
{
  _ledOn();
  if (server->method() == HTTP_GET) {
    _sendCorsHeaderIfEnabled();
    String response = fn();
    const char* contentType = isHtml ? "text/html" : "application/json";
    server->send(200, contentType, response);
  } else {
    server->send(405, "text/plain", "Method Not Allowed");
  }
  _ledOff();
}

void ESP8266AutoIOT::_handlePostRequestVoidFn(voidCallbackStr fn)
{
  _ledOn();
  if (server->method() == HTTP_POST) {
    _sendCorsHeaderIfEnabled();

    if (server->hasArg("plain") == false) {
      server->send(400);
      return;
    }
    server->send(200);
    String body = server->arg("plain");
    fn(body);
  } else {
    server->send(405, "text/plain", "Method Not Allowed");
  }
  _ledOff();
}


void ESP8266AutoIOT::_handlePostRequestStrFn(stringCallbackStr fn)
{
  _ledOn();
  if (server->method() == HTTP_POST) {
    _sendCorsHeaderIfEnabled();

    if (server->hasArg("plain") == false) {
      server->send(400);
      return;
    }
    String body = server->arg("plain");
    String response = fn(body);
    server->send(200, "application/json", response);
  } else {
    server->send(405, "text/plain", "Method Not Allowed");
  }
  _ledOff();
}

void ESP8266AutoIOT::_handleNotFound()
{
  _ledOn();
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server->uri();
  message += "\nMethod: ";
  message += (server->method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server->args();
  message += "\n";
  for (uint8_t i = 0; i < server->args(); i++) {
    message += " " + server->argName(i) + ": " + server->arg(i) + "\n";
  }
  server->send(404, "text/plain", message);
  _ledOff();
}

void ESP8266AutoIOT::get(String path, stringCallback fn, bool isHtml) {
  server->on(path, std::bind(&ESP8266AutoIOT::_handleGetRequestStrFn, this, fn, isHtml));
}

void ESP8266AutoIOT::get(String path, stringCallback fn) {
  server->on(path, std::bind(&ESP8266AutoIOT::_handleGetRequestStrFn, this, fn, false));
}

void ESP8266AutoIOT::get(String path, voidCallback fn) {
  server->on(path, std::bind(&ESP8266AutoIOT::_handleGetRequestVoidFn, this, fn));
}

void ESP8266AutoIOT::get(String path, String response) {
  server->on(path, std::bind(&ESP8266AutoIOT::_handleGetRequestStr, this, response, false));
}

void ESP8266AutoIOT::get(String path, String response, bool isHtml) {
  server->on(path, std::bind(&ESP8266AutoIOT::_handleGetRequestStr, this, response, isHtml));
}

void ESP8266AutoIOT::post(String path, voidCallbackStr fn) {
  server->on(path, std::bind(&ESP8266AutoIOT::_handlePostRequestVoidFn, this, fn));
}

void ESP8266AutoIOT::post(String path, stringCallbackStr fn) {
  server->on(path, std::bind(&ESP8266AutoIOT::_handlePostRequestStrFn, this, fn));
}

void ESP8266AutoIOT::root(stringCallback fn) {
  _rootHandled = true;
  get("/", fn, true);
}

void ESP8266AutoIOT::root(String response) {
  _rootHandled = true;
  get("/", response, true);
}

void ESP8266AutoIOT::_handleDefaultRoot()
{
  root("Success");
}

void ESP8266AutoIOT::disableLED()
{
  _ledEnabled = false;
}

void ESP8266AutoIOT::enableCors()
{
  _corsEnabled = true;
}

void ESP8266AutoIOT::enableCors(String origin)
{
  _corsEnabled = true;
  _corsOrigin = origin;
}

void ESP8266AutoIOT::begin()
{
  _hasBegun = true;
  if (_ledEnabled)
  {
    pinMode(_LED, OUTPUT);
  }

  // I've been told this line is a good idea
  WiFi.mode(WIFI_STA);
  
  // Set hostname from settings
  // It's particularly dumb that they don't use the same method
  #ifdef ESP32
    WiFi.setHostname(_accessPoint);
  #else
    WiFi.hostname(_accessPoint);
  #endif

  // wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setConnectTimeout(CONNECT_TIMEOUT);
  wifiManager.setTimeout(AP_TIMEOUT);
  wifiManager.setCountry("US");

  if (!wifiManager.autoConnect(_accessPoint, _password)) {
    // If we've hit the config portal timeout, then retstart
    
    Serial.println("%%% Failed to connect and hit timeout, restarting");
    delay(100);
    ESP.restart();

    // Not sure if this line is necessary
    delay(5000);
  }

  Serial.println("Connected! to WiFi!");

  if (MDNS.begin(_accessPoint))
  {
    Serial.println("MDNS responder started");
  }

  if (!_rootHandled)
  {
    _handleDefaultRoot();
  }

  server->onNotFound(std::bind(&ESP8266AutoIOT::_handleNotFound, this));

  server->begin();
  Serial.println("HTTP server started");

  if (_otaEnabled) {
    ArduinoOTA.setPassword(_password);
    ArduinoOTA.begin();
    Serial.println("ArduinoOTA enabled!");
  } else {
    Serial.println("ArduinoOTA disabled.");
  }
  _ledOff();
}

void ESP8266AutoIOT::loop()
{
  if (!_hasBegun)
  {
    if (_ledEnabled)
    {
      _ledOn();
      delay(500);
      _ledOff();
      delay(500);
      _ledOn();
      delay(500);
      _ledOff();
    }

    Serial.println("It looks like you forgot to call app.begin(); in setup()");
    Serial.println("WiFi connectivity is disabled!");

    delay(10000);
  }
  else
  {
    _ledOff();
    server->handleClient();
    _ledOff();
    if (_otaEnabled)
    {
      ArduinoOTA.handle();
    }
  }
}

void ESP8266AutoIOT::_ledOn()
{
  _digitalWrite(_LED_ON);
}

void ESP8266AutoIOT::_ledOff()
{
  _digitalWrite(_LED_OFF);
}

void ESP8266AutoIOT::resetCredentials()
{
  wifiManager.resetSettings();
}
