 /**
 *     PROJECT: MySensors / Small battery sensor low power 8 mhz
 *     PROGRAMMER: Jumping
 *     DATE: october 10, 2016/ last update: october 10, 2016
 *     FILE: sensor12_energymeter.ino
 *     LICENSE: Public domain
 *    
 *     Hardware: ATMega328p board w/ NRF24l01
 *        and MySensors 2.2
 *        hw signing atsha204a
 *        mysbootloader 1.3 beta
 *        version 1.1 modified for Home Assistant
 *        https://forum.mysensors.org/topic/4819/power-meter-pulse-sensor/130    
 *    Special:
 *        program with Arduino Pro 3.3V 8Mhz!!!
 *        
 *    Summary:
 *        low power (battery)
 *        pulse sensor read
 *        voltage meter for battery
 *    
 *    Remarks:
 ********************************************************************
*/
// Enable OTA
//#define MY_OTA_FIRMWARE_FEATURE

// Enable debug prints to serial monitor
#define MY_DEBUG 
// Enable and select radio type attached
#define MY_RADIO_NRF24
//#define MY_RADIO_RFM69

// define NRF24 channel
#define MY_RF24_CHANNEL 76
//
// NODE ID
#define MY_NODE_ID 12
#define NODE_TXT " ENERGY_12"  // Text to add to sensor name
//
//#define MY_SIGNING_ATSHA204
//#define MY_SIGNING_ATSHA204_PIN 17
//#define MY_SIGNING_REQUEST_SIGNATURES
#include <MySensors.h>


#define DIGITAL_INPUT_SENSOR 3  // The digital input you attached your light sensor.  (Only 2 and 3 generates interrupt!)
#define PULSE_FACTOR 1000       // Nummber of blinks per KWH of your meeter
#define SLEEP_MODE false        // Watt-value can only be reported when sleep mode is false.
#define MAX_WATT 10000          // Max watt value to report. This filetrs outliers.
#define WATT_CHILD_ID       1 // Id of the sensor child
#define KWH_CHILD_ID        2
#define PC_CHILD_ID         3
//#define BATTERY_CHILD_ID    7   // BATTERY DEFINITION
//#define batteryVoltage_PIN  A0  //analog input A0 on ATmega328 is battery voltage
// Reference values for ADC and Battery measurements
//const float VccMin          = 1.0*2.8 ;             // Minimum expected Vcc level, in Volts.
//const float VccMax          = 1.0*4.2 ;             // Maximum expected Vcc level, in Volts. 
//const float VccCorrection   = 3.4/3.44;             // Measured Vcc by multimeter divided by reported 
//const float Threshold       = 0.1 ;                // send only if change > treshold (Volt)
//const float VccReference    = 4.433 ;               // voltage reference for measurement, definitive init in setup

uint32_t SEND_FREQUENCY = 20000; // Minimum time between send (in milliseconds). We don't want to spam the gateway.
double ppwh = ((double)PULSE_FACTOR)/1000; // Pulses per watt hour
boolean pcReceived = false;
volatile uint32_t pulseCount = 0;   
volatile uint32_t lastBlink = 0;
volatile uint32_t watt = 0;
uint32_t oldPulseCount = 0;   
uint32_t oldWatt = 0;
double oldKwh;
uint32_t lastSend;

//float lastBattVoltage = 0 ;
//boolean txBattVoltage = true ;

MyMessage wattMsg(WATT_CHILD_ID,V_WATT);
MyMessage kwhMsg(KWH_CHILD_ID,V_KWH);
MyMessage pcMsg(PC_CHILD_ID,V_VAR1);
//--------- BATTERY---------------
//MyMessage voltageMsg(BATTERY_CHILD_ID, V_VOLTAGE);  // Node voltage

void presentation() {
  // Send the sketch version information to the gateway and Controller
  wait(200);
  sendSketchInfo("JMP" NODE_TXT, "1.1");
  // Register this device as power sensor
  //present(ENERGY_CHILD_ID, S_POWER, "Energy" NODE_TXT);
  present(WATT_CHILD_ID, S_POWER, "Watt" NODE_TXT);
  wait(200);
  present(KWH_CHILD_ID, S_POWER, "KWH" NODE_TXT);
  wait(200);
  present(PC_CHILD_ID, S_CUSTOM, "Pulse Count" NODE_TXT);
  wait(200);
  // -------- BATTERIA --------------
  //present(BATTERY_CHILD_ID, S_MULTIMETER, "Battery" NODE_TXT);
  //wait(200);
}

void setup()  {  
  // Fetch last known pulse count value from gw
  request(PC_CHILD_ID, V_VAR1);
  // Use the internal pullup to be able to hook up this sketch directly to an energy meter with S0 output
  // If no pullup is used, the reported usage will be too high because of the floating pin
  pinMode(DIGITAL_INPUT_SENSOR,INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(DIGITAL_INPUT_SENSOR), onPulse, RISING);
  lastSend=millis();

// use the 1.1 V internal reference
//  #if defined(__AVR_ATmega2560__)
//     analogReference(INTERNAL1V1);
//  #else
//     analogReference(INTERNAL);
//  #endif
}

void loop()     
{ 
  uint32_t now = millis();
  //float batteryVoltage = ((float)analogRead(batteryVoltage_PIN) * VccReference / 1023); // VOLTAGE battery
  //if (abs(batteryVoltage  - lastBattVoltage) >= Threshold) {
  //    lastBattVoltage = batteryVoltage;
  //    txBattVoltage= true;
  //} 

  // Only send values at a maximum frequency or woken up from sleep
  bool sendTime = now - lastSend > SEND_FREQUENCY;
  if (pcReceived && (SLEEP_MODE || sendTime)) {
    // New watt value has been calculated  
    if (!SLEEP_MODE && watt != oldWatt) {
      // Check that we dont get unresonable large watt value. 
      // could hapen when long wraps or false interrupt triggered
      if (watt<((uint32_t)MAX_WATT)) {
        send(wattMsg.set(watt));  // Send watt value to gw 
      } 
      #ifdef MY_DEBUG
         Serial.print("Watt:");
         Serial.println(watt);
      #endif
      oldWatt = watt;
    }
    // Pulse cout has changed
    if (pulseCount != oldPulseCount) {
      send(pcMsg.set(pulseCount));  // Send pulse count value to gw 
      double kwh = ((double)pulseCount/((double)PULSE_FACTOR));     
      oldPulseCount = pulseCount;
      if (kwh != oldKwh) {
        send(kwhMsg.set(kwh, 4));  // Send kwh value to gw
        #ifdef MY_DEBUG
          Serial.print("KiloWattoH:");
          Serial.println(kwh);
        #endif
        oldKwh = kwh;
      }
    }    
    lastSend = now;
  } else if (sendTime && !pcReceived) {
    // No count received. Try requesting it again
    request(PC_CHILD_ID, V_VAR1);
    lastSend=now;
  }
  //----------------SEND BATTERY----------------//
  //sendSensors();
  //----------------- SLEEP -------------------//
  if (SLEEP_MODE) {
    sleep(SEND_FREQUENCY);
  }
}

void receive(const MyMessage &message) {
  if (message.type==V_VAR1) {  
    pulseCount = oldPulseCount = message.getLong();
    #ifdef MY_DEBUG
        Serial.print("Received last pulse count from gw:");
        Serial.println(pulseCount);
    #endif
    pcReceived = true;
  }
}

void onPulse()     
{ 
  if (!SLEEP_MODE) {
    uint32_t newBlink = micros();  
    uint32_t interval = newBlink-lastBlink;
    if (interval<10000L) { // Sometimes we get interrupt on RISING
      return;
    }
    watt = (3600000000.0 /interval) / ppwh;
    lastBlink = newBlink;
  } 
  pulseCount++;
}

// send to gateway depending on tx.. settings
