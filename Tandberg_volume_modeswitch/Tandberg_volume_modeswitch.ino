/**
 * BasicHTTPClient.ino
 *
 *  Created on: 24.05.2015
 *
 */

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <ESP8266HTTPClient.h>

#include <Encoder.h>


ESP8266WiFiMulti WiFiMulti;
HTTPClient http;

String ipAdress = "http://192.168.50.49:11000/";
String setAnalogInput = "Play?url=Capture%3Aplughw%3A0%2C0%2F48000%2F24%2F2%2Finput0";

Encoder volumeEncoder(D1, D2);
const int buttonPin = D3;

long oldPositionVolume  = 0;
int oldButtonState = 0;
int buttonState = 0;         // variable for reading the pushbutton status



void setup() {
    
    Serial.begin(115200);


    WiFiMulti.addAP("wifi_name", "wifi_pass_word");

    pinMode(buttonPin, INPUT_PULLUP);

    Serial.println("Setup done");
}

void loop() {

  buttonState = digitalRead(buttonPin);
  if(buttonState != oldButtonState) {
    
    // releasing the button
    if(buttonState == 1 && oldButtonState == 0 ) {
      controlVolume(999);
      Serial.print("button state: ");
      Serial.println(buttonState);  
    }
    oldButtonState = buttonState;
  }
  
  

  long newPositionVolume = volumeEncoder.read() / 4;
  if (newPositionVolume != oldPositionVolume) {
    long deltaVolume = oldPositionVolume - newPositionVolume;
    oldPositionVolume = newPositionVolume;

    
    int volume = controlVolume(0);

    Serial.print("gettin volume: ");
    Serial.println(volume);

    Serial.print("deltaVolume: ");
    Serial.println(deltaVolume);

    // fix for Bluesound bug where volume 1 is reported as 0
    if(volume == 0 && deltaVolume == 1) {
      volume = 2;
    }
    else {
      volume += deltaVolume;
    }
    
    int updatedVolume = controlVolume(volume);

    Serial.print("updated: ");
    Serial.println(updatedVolume);  

  }
}

int controlVolume(int newVolume) {

  int currentVolume = 0;
 
  if((WiFiMulti.run() == WL_CONNECTED)) {

    if(newVolume == 999) {
      http.begin(ipAdress + setAnalogInput );
    }

    else if(newVolume == 0) {
      String getVolume = ipAdress + "Volume?level";
      http.begin(getVolume);
    }
      
    else {
      String setNewVolume = ipAdress + "Volume?level=";
      setNewVolume += String(newVolume);
      
      http.begin(setNewVolume); 
    }

    int httpCode = http.GET();
    
    // httpCode will be negative on error
    if(httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
     
        // file found at server
        if(httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
     
            currentVolume = readRespons(payload);
        }
    } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  }
  return currentVolume;
}

char *starttags[] = { "<volume" };
char *endtags[] = { "</volume>" };

String tagdata[1];

int readRespons(String content) {

  
  for (int i=0; i<1; i++ )
  {
       // Find the <Value> and </Value> tags and make a substring of the content between the two
       int startValue = content.indexOf(starttags[i]);
       int endValue = content.indexOf(endtags[i]);
      // Serial.println(starttags[i]);
       int k = strlen(starttags[i]);
       tagdata[i] = content.substring(startValue + k, endValue);
       // checking for ending > to close the first tag if it contains attributes
       int endingArrow = tagdata[i].indexOf(">");
       if( endingArrow != -1) {
          tagdata[i] = tagdata[i].substring(endingArrow + 1, tagdata[i].length()); 
       }
  }

  String volumeStringParsed = tagdata[0];
  // added this print out to create a small delay that caused wdt crash
  Serial.println("volumeStringParsed");
  Serial.println(volumeStringParsed);
  // filter out possible white space
  volumeStringParsed.replace(" ", ""); 
  int volume = volumeStringParsed.toInt();
 
  return volume;
}
