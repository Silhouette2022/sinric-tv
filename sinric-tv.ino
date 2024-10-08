/*
 * Example for how to use SinricPro TV device:
 * - setup a TV device
 * - handle request using callbacks
 *
 * If you encounter any issues:
 * - check the readme.md at https://github.com/sinricpro/esp8266-esp32-sdk/blob/master/README.md
 * - ensure all dependent libraries are installed
 *   - see https://github.com/sinricpro/esp8266-esp32-sdk/blob/master/README.md#arduinoide
 *   - see https://github.com/sinricpro/esp8266-esp32-sdk/blob/master/README.md#dependencies
 * - open serial monitor and check whats happening
 * - check full user documentation at https://sinricpro.github.io/esp8266-esp32-sdk
 * - visit https://github.com/sinricpro/esp8266-esp32-sdk/issues and check for existing issues or open a new one
 */

#include <Arduino.h>
#ifdef ESP8266 
       #include <ESP8266WiFi.h>
#endif 
#ifdef ESP32   
       #include <WiFi.h>
#endif

#include "SinricPro.h"
#include "SinricProTV.h"

#include <IRremoteESP8266.h>
#include <IRsend.h>

const uint16_t kIrLed = 4;  // ESP8266 GPIO pin to use. Recommended: 4 (D2).

IRsend irsend(kIrLed);  // Set the GPIO to be used to sending the message.

#include "credentials.h"


#define USE_WOL_MEDIA_PC  1 // Set to 0 if you don't want to use WOL packets to wake up Media PC

#ifndef _CREDENTIALS_H_
#define WIFI_SSID         "YOUR-WIFI-SSID"    
#define WIFI_PASS         "YOUR-WIFI-PASSWORD"
#define APP_KEY           "YOUR-APP-KEY"      // Should look like "de0bxxxx-1x3x-4x3x-ax2x-5dabxxxxxxxx"
#define APP_SECRET        "YOUR-APP-SECRET"   // Should look like "5f36xxxx-x3x7-4x3x-xexe-e86724a9xxxx-4c4axxxx-3x3x-x5xe-x9x3-333d65xxxxxx"
#define TV_ID             "YOUR-DEVICE-ID"    // Should look like "5dc1564130xxxxxxxxxxxxxx"

#define MEDIA_PC_MAC      "00:00:00:00:00:00" // Media PC MAC
#endif
#define     BAUD_RATE      9600

#if USE_WOL_MEDIA_PC
  #include <WiFiUdp.h>
  WiFiUDP UDP;
  #include <WakeOnLan.h>
  WakeOnLan WOL(UDP); // Pass WiFiUDP class
#endif

bool tvPowerState;
unsigned int tvVolume;
unsigned int tvChannel;
bool tvMuted;

// channelNames used to convert channelNumber into channelName
// please put in your TV channel names
// channel numbers starts counting from 0!
// so "ZDF" is channel 2
const char* channelNames[] = {
  "A/V",
  "ard", 
  "ZDF", 
};

#define MAX_CHANNELS sizeof(channelNames) / sizeof(channelNames[0])  // just to determine how many channels are in channelNames array

// map channelNumbers used to convert channelName into channelNumber
// This map is initialized in "setupChannelNumbers()" function by using the "channelNames" array
std::map<String, unsigned int> channelNumbers;

void setupChannelNumbers() {
  for (unsigned int i=0; i < MAX_CHANNELS; i++) {
    channelNumbers[channelNames[i]] = i;
  }
}

// TV device callbacks

bool onAdjustVolume(const String &deviceId, int &volumeDelta) {
  int dv = volumeDelta;
  tvVolume += volumeDelta;  // calcualte new absolute volume
  Serial.printf("Volume changed about %i to %i\r\n", volumeDelta, tvVolume);
  volumeDelta = tvVolume; // return new absolute volume
  if(dv > 0)
  {
    while(dv--)
    {
      irsend.sendSony(0x490, 12, 3); // Sony Vol Up
      delay(200);
    }
  }
  else if(dv < 0)
  {
    while(dv++)
    {
      irsend.sendSony(0xC90, 12, 3); // Sony Vol Down
      delay(200);
    }
  }
  return true;
}

bool onChangeChannel(const String &deviceId, String &channel) {
  tvChannel = channelNumbers[channel]; // save new channelNumber in tvChannel variable
  Serial.printf("Change channel to \"%s\" (channel number %d)\r\n", channel.c_str(), tvChannel);
  return true;
}

bool onChangeChannelNumber(const String& deviceId, int channelNumber, String& channelName) {
  tvChannel = channelNumber; // update tvChannel to new channel number
  if (tvChannel < 0) tvChannel = 0;
  if (tvChannel > MAX_CHANNELS-1) tvChannel = MAX_CHANNELS-1;

  channelName = channelNames[tvChannel]; // return the channelName

  Serial.printf("Change to channel to %d (channel name \"%s\")\r\n", tvChannel, channelName.c_str());
  return true;
}

bool onMediaControl(const String &deviceId, String &control) {
  Serial.printf("MediaControl: %s\r\n", control.c_str());
  if (control == "Play") {}           // do whatever you want to do here
  if (control == "Pause") {}          // do whatever you want to do here
  if (control == "Stop") {}           // do whatever you want to do here
  if (control == "StartOver") {}      // do whatever you want to do here
  if (control == "Previous") {}       // do whatever you want to do here
  if (control == "Next") {}           // do whatever you want to do here
  if (control == "Rewind") {}         // do whatever you want to do here
  if (control == "FastForward") {}    // do whatever you want to do here
  return true;
}

bool onMute(const String &deviceId, bool &mute) {
  Serial.printf("TV volume is %s\r\n", mute?"muted":"unmuted");
  tvMuted = mute; // set tvMuted state
  irsend.sendSony(0x290, 12, 3); // Sony Mute
  return true;
}

bool onPowerState(const String &deviceId, bool &state) {
  Serial.printf("TV turned %s\r\n", state?"on":"off");
  tvPowerState = state; // set powerState
  if(state) {
    irsend.sendSony(0x750, 12, 3); // Turn Sony On

#if USE_WOL_MEDIA_PC
      //Send WOL UDP packet (Using the default port - 9)
      WOL.sendMagicPacket(MEDIA_PC_MAC);
#endif      
  }
  else irsend.sendSony(0xf50, 12, 3); // Turn Sony Off
  return true; 
}

bool onSelectInput(const String &deviceId, String &input) {
  Serial.printf("Input changed to %s\r\n", input.c_str());
  if(input=="HDMI 1") irsend.sendSony(0x2d58, 15, 3);
  if(input=="HDMI 2") irsend.sendSony(0x6d58, 15, 3);
  if(input=="HDMI 3") irsend.sendSony(0x1d58, 15, 3);
  if(input=="HDMI 4") irsend.sendSony(0x5d58, 15, 3);
  return true;
}

bool onSetVolume(const String &deviceId, int &volume) {
  Serial.printf("Volume set to:  %i\r\n", volume);
  tvVolume = volume; // update tvVolume
  return true;
}

bool onSkipChannels(const String &deviceId, const int channelCount, String &channelName) {
  tvChannel += channelCount; // calculate new channel number
  if (tvChannel < 0) tvChannel = 0;
  if (tvChannel > MAX_CHANNELS-1) tvChannel = MAX_CHANNELS-1;
  channelName = String(channelNames[tvChannel]); // return channel name

  Serial.printf("Skip channel: %i (number: %i / name: \"%s\")\r\n", channelCount, tvChannel, channelName.c_str());

  return true;
}

// setup function for WiFi connection
void setupWiFi() {
  Serial.printf("\r\n[Wifi]: Connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.printf(".");
    delay(250);
  }
  IPAddress localIP = WiFi.localIP();
  Serial.printf("connected!\r\n[WiFi]: IP-Address is %d.%d.%d.%d\r\n", localIP[0], localIP[1], localIP[2], localIP[3]);
}

// setup function for SinricPro
void setupSinricPro() {
  // add device to SinricPro
  SinricProTV& myTV = SinricPro[TV_ID];

  // set callback functions to device
  myTV.onAdjustVolume(onAdjustVolume);
  myTV.onChangeChannel(onChangeChannel);
  myTV.onChangeChannelNumber(onChangeChannelNumber);
  myTV.onMediaControl(onMediaControl);
  myTV.onMute(onMute);
  myTV.onPowerState(onPowerState);
  myTV.onSelectInput(onSelectInput);
  myTV.onSetVolume(onSetVolume);
  myTV.onSkipChannels(onSkipChannels);

  // setup SinricPro
  SinricPro.onConnected([](){ Serial.printf("Connected to SinricPro\r\n"); }); 
  SinricPro.onDisconnected([](){ Serial.printf("Disconnected from SinricPro\r\n"); });
  SinricPro.begin(APP_KEY, APP_SECRET);
}

// main setup function
void setup() {
  Serial.begin(BAUD_RATE); Serial.printf("\r\n\r\n");
  Serial.printf("%d channels configured\r\n", MAX_CHANNELS);

  setupWiFi();
  setupSinricPro();
  setupChannelNumbers();

#if USE_WOL_MEDIA_PC
  WOL.setRepeat(3, 100); // Repeat the packet three times with 100ms delay between
  Serial.printf("WOL Set to %s\r\n", MEDIA_PC_MAC);
#endif  

  irsend.begin();
}

void loop() {
  SinricPro.handle();
}
