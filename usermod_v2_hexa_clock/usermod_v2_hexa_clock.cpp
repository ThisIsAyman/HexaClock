#include "usermod_v2_hexa_clock.h"

// Initialize static member
const char HexaClock::_name[] PROGMEM = "HexClockUsermod";

uint16_t HexaClock::getLuminance()
{
  // http://forum.arduino.cc/index.php?topic=37555.0
  // https://forum.arduino.cc/index.php?topic=185158.0
  float volts = analogRead(PHOTORESISTOR_PIN) * (referenceVoltage / adcPrecision);
  float amps = volts / resistorValue;
  float lux = amps * 1000000 * 2.0;
  lastLuminance = uint16_t(lux);

  lastMeasurement = millis();
  getLuminanceComplete = true;
  return uint16_t(lux);
}

void HexaClock::setup() 
{
  //Serial.println("Hello from hexa_clock usermod!");

  for(int i=0; i<LEDS_NO; i++)
  {
    reverseRoundMap[roundMap[i]] = i;
    reverseVerticalMap[verticalMap[i]] = i;
  }
  // set pinmode
  pinMode(PHOTORESISTOR_PIN, INPUT);
}

void HexaClock::connected() 
{
  //Serial.println("Connected to WiFi!");
}

void HexaClock::loop() 
{
  if (millis() - lastTime > 1000) {
    if(autoBrightnessEnabled && powerOn)
    {
      uint16_t lux = getLuminance();
      
      if(lux < nightModeThreshold && nightModeEnabled)
      {
        if(!nightModeOn)
        {
          nightModeOn = true;
          prevPreset = currentPreset;
          
          prevPlaylist = currentPlaylist;
          currentPlaylist = -1;
          //apply black background
          if(!reverseDigits) applyPreset(3);
        }
        
        bri = nightModeBri;
      }
      else {
        if(nightModeOn)
        {
          nightModeOn = false;
          //if there was a playlist playing on play it again
          if(prevPlaylist != -1)
          {
            currentPlaylist = prevPlaylist;
          }
          else 
          {
            applyPreset(prevPreset);
          }
        }
        bri = constrain(lux * autoBrightnessACoeff + autoBrightnessBCoeff,autoBrightnessMinBri,255);

      }
      colorUpdated(CALL_MODE_BUTTON);
      updateInterfaces(CALL_MODE_BUTTON);
    }

    
    hours = hour(localTime);
    minutes = minute(localTime);
    lastTime = millis();
  }
}

void HexaClock::addToJsonInfo(JsonObject& root)
{
  JsonObject user = root["u"];
  if (user.isNull()) user = root.createNestedObject("u");

  JsonArray lightArr = user.createNestedArray("Light"); //name
  lightArr.add(lastLuminance); //value
  lightArr.add(" lux"); //unit
}

void HexaClock::addToJsonState(JsonObject& root)
{
  //root["user0"] = userVar0;
}

void HexaClock::readFromJsonState(JsonObject& root)
{
  JsonObject usermod = root[FPSTR(_name)];
  if (usermod.isNull()) {
    // Also try generic "um" object
    usermod = root["um"][FPSTR(_name)];
  }
  if (usermod.isNull()) return;

  if (!usermod["displayClock"].isNull())
    displayClock=usermod["displayClock"].as<bool>();

  //if (root["bri"] == 255) Serial.println(F("Don't burn down your garage!"));
  if(root.containsKey("on"))
  {
    if(root["on"]==true){
      powerOn = true;
    }else
    {
      powerOn = false;
    } 
  }
}

void HexaClock::addToConfig(JsonObject& root)
{
  JsonObject top = root.createNestedObject("HexClockUsermod");
  top["displayClock"] = displayClock;
  top["ledmapEnabled"] = ledmapEnabled;
  top["autoBrightnessEnabled"] = autoBrightnessEnabled;
  top["autoBrightnessACoeff"] = autoBrightnessACoeff;
  top["autoBrightnessBCoeff"] = autoBrightnessBCoeff;
  top["autoBrightnessMinBri"] = autoBrightnessMinBri;
  top["nightModeEnabled"] = nightModeEnabled;
  top["nightModeThreshold"] = nightModeThreshold;
  top["nightModeBri"] = nightModeBri;
  top["digitWhite"] = digitWhite;
  top["reverseDigits"] = reverseDigits;
}

bool HexaClock::readFromConfig(JsonObject& root)
{
  // default settings values could be set here (or below using the 3-argument getJsonValue()) instead of in the class definition or constructor
  // setting them inside readFromConfig() is slightly more robust, handling the rare but plausible use case of single value being missing after boot (e.g. if the cfg.json was manually edited and a value was removed)

  JsonObject top = root["HexClockUsermod"];

  bool configComplete = !top.isNull();

  configComplete &= getJsonValue(top["displayClock"], displayClock, true);
  configComplete &= getJsonValue(top["ledmapEnabled"], ledmapEnabled, true);
  configComplete &= getJsonValue(top["autoBrightnessEnabled"], autoBrightnessEnabled, true);
  configComplete &= getJsonValue(top["autoBrightnessACoeff"], autoBrightnessACoeff, 2.0);
  configComplete &= getJsonValue(top["autoBrightnessBCoeff"], autoBrightnessBCoeff, 0.0);
  configComplete &= getJsonValue(top["autoBrightnessMinBri"], autoBrightnessMinBri, 10);
  configComplete &= getJsonValue(top["nightModeEnabled"], nightModeEnabled, true);
  configComplete &= getJsonValue(top["nightModeThreshold"], nightModeThreshold, 10);
  configComplete &= getJsonValue(top["nightModeBri"], nightModeBri, 1);
  configComplete &= getJsonValue(top["digitWhite"], digitWhite, true);
  configComplete &= getJsonValue(top["reverseDigits"], reverseDigits, false);

  return configComplete;
}

void HexaClock::handleOverlayDraw()
{
  if(!displayClock)return;
  int orientation = 0;
  int digit;
  //for flat-top (1,3,5) orientations there are 12 pixels
  int pixelsNo = 13 - (orientation%2);
  //used when reversing digits
  bool reverseMask[LEDS_NO] = {false};
  uint8_t local_hours = !useAMPM ? hours : (hours>12 ? hours-12 : (hours==0 ? 12 : hours));

  for(int p = 0; p < 4; p++)
  {
    switch(p){
      case 0:
        digit = local_hours/10;
        break;
      case 1:
        digit = local_hours%10;
        break;
      case 2:
        digit = minutes/10;
        break;
      case 3:
        digit = minutes%10;
        break;                                  
    }

    // iterate through every pixel of the digit mask
    for(int i=0; i<pixelsNo; i++){
      //check if this pixel is involved in displaying the time (active)
      bool active = digitMask[orientation%2][digit][i];
      if(active)
      {
        int ledId;
        //ledmap 1 => vertical pattern (as if strips were spreaded vertically)
        if(ledmapEnabled && currentLedmap == 1)
        {
          ledId = reverseVerticalMap[digitSegment[orientation][p][i]];
        }
        //ledmap 0 => round pattern (as if strips were spreaded in circles)
        else if(ledmapEnabled)
        {
          ledId = reverseRoundMap[digitSegment[orientation][p][i]];
        }
        //no ledmap
        else
        {
          ledId = digitSegment[orientation][p][i];
        }

        //normal => white digits
        if(!reverseDigits)
        {
          strip.setPixelColor(ledId, RGBW32(255*digitWhite,255*digitWhite,255*digitWhite,0));
        }
        // black digits instead of white
        else 
        {
          //mark this digit as 'should be dark' to later set color
          reverseMask[ledId]=true;
        }
      }
    }
  }
  
  //apply dark digits instead of white
  if(reverseDigits)
  {
    for(int i=0; i<LEDS_NO; i++)
    {
      if(!reverseMask[i])
      {
        strip.setPixelColor(i, RGBW32(255*!digitWhite,255*!digitWhite,255*!digitWhite,0));
      }
    }
  }
}

uint16_t HexaClock::getId()
{
  return USERMOD_ID_HEXA_CLOCK;
}

static HexaClock usermod_v2_hexa_clock;
REGISTER_USERMOD(usermod_v2_hexa_clock);
