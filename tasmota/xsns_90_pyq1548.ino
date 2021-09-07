/*
  xsns_90_pyq_1548_7660.ino - PIR PYQ 1548/7660 support for Tasmota

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_PYQ1548


/*********************************************************************************************\
 * PYQ 1548/7659 - Sensor PIR (Passive Infrarot)
 *
 * Code for PYQ 1548/7659 Sensor:
 * References:
 * - https://github.com/renebohne/esp32-bigclown-pir-module/blob/master/src/main.cpp
\*********************************************************************************************/

#define XSNS_90                   90
#define PYQ1548_CoolDown          5  //Reading (seconds to display movement)
#define PYQ1548_ReActivate        10 //Reactivate (write config) device after On/OFF using Relay
    //see https://media.digikey.com/pdf/Data%20Sheets/Excelitas%20PDFs/PYQ1648-7052.pdf
    // | 8bit sensitivity | 4bit blind time | 2bit pulse counter | 2bit window time | 2bit operatin mode | 2bit filter source | 5bit reserved |
    //00011000 0100 01 10 10 00 10000
#define PYQ1548_DEFAULT_CONFIG            0x00304D10
#define PYQ1548_MUST_SET                  0x00000010

bool bmove = 0;
uint8_t coold = PYQ1548_CoolDown;
uint16_t confwrite = PYQ1548_ReActivate;
bool pyq1548_found = 0;

typedef union {
  uint32_t regval;
  struct {                             // bits              //range  //default   //description
      uint32_t count_mode:1;           // bit [0]           0..1     0
      uint32_t _must_set_to_0:1;       // bit [1]                    0
      uint32_t hpf_cut_off:1;          // bit [2]           0..1     0
      uint32_t _must_set_to_2:2;       // bits [4..3]                2
      uint32_t signal_source:2;        // bits [6..5]       0..3     0
      uint32_t operation_mode:2;       // bits [8..7]       0..3     2
      uint32_t window_time:2;          // bits [10..9]      0..3     2
      uint32_t pulse_counter:2;        // bits [12..11]     0..3     1           Pulses needed to fire the trigger 0=1pulse,1=2pulse,2=3pulse,3=4pulse
      uint32_t blind_time:4;           // bits [16..13]     0...15   2           Seconds to wait after Triggered: ([Reg Val] / 2) + 0.5sec
      uint32_t threshold:8;            // bits [24..17]     0..255   24          Sensitvity, lower = more Sensitive
  } field;

} PYQ1548_Config_t;

PYQ1548_Config_t PYQ1548_Config = { .regval = PYQ1548_DEFAULT_CONFIG };

void writeregval(int pin1,uint32_t regval){
  int i;
  int _pin1=pin1;
  unsigned long _regval=regval;
  unsigned char nextbit;
  unsigned long regmask = 0x1000000;
  regval |= PYQ1548_MUST_SET; // Insure that reserved2 field is set to 2
  pinMode(_pin1,OUTPUT);
  digitalWrite(_pin1,LOW);
  for(i=0; i < 25; i++) {
    nextbit = (_regval&regmask)!=0;
    regmask >>=1;
    digitalWrite(_pin1,LOW);
    digitalWrite(_pin1,HIGH);

    if(nextbit) {
      digitalWrite(_pin1,HIGH);
    }
    else{
      digitalWrite(_pin1,LOW);
    }
    delayMicroseconds(100);
  }
  digitalWrite(_pin1,LOW);
  delayMicroseconds(600);
}

bool PYQ1548Init(void)
{
  AddLog(0, PSTR("PYQ1548: regval = 0x%08X"), PYQ1548_Config.regval );
  AddLog(0, PSTR("PYQ1548: count_mode [0] = %d"), PYQ1548_Config.field.count_mode );
  AddLog(0, PSTR("PYQ1548: hpf cut off [2] = %d"), PYQ1548_Config.field.hpf_cut_off );
  AddLog(0, PSTR("PYQ1548: signal source [6..5] = %d"), PYQ1548_Config.field.signal_source );
  AddLog(0, PSTR("PYQ1548: op mode [8..7] = %d"), PYQ1548_Config.field.operation_mode );
  AddLog(0, PSTR("PYQ1548: window time [10..9] = %d"), PYQ1548_Config.field.window_time );
  AddLog(0, PSTR("PYQ1548: pulse counter [12..11] = %d"), PYQ1548_Config.field.pulse_counter );
  AddLog(0, PSTR("PYQ1548: blind time [16..13] = %d"), PYQ1548_Config.field.blind_time );
  AddLog(0, PSTR("PYQ1548: threshold [24..17] = %d"), PYQ1548_Config.field.threshold );

  if (PinUsed(GPIO_PYQ_PIR_DL) && PinUsed(GPIO_PYQ_PIR_SER))  // Only start, if the pins are configured
  {
    pinMode(Pin(GPIO_PYQ_PIR_SER), OUTPUT);   //Serial IN Interface
    pinMode(Pin(GPIO_PYQ_PIR_DL), INPUT);     //Direct Link Interface

    writeregval(Pin(GPIO_PYQ_PIR_SER), PYQ1548_Config.regval );
    pyq1548_found = 1;
    return true;
  } else {
    AddLog(LOG_LEVEL_DEBUG, PSTR("PYQ: No PIN Configured!")); 
    return false;
  }
}

void PYQ1548Reading(void) {
  if (PinUsed(GPIO_PYQ_PIR_DL) && PinUsed(GPIO_PYQ_PIR_SER) && pyq1548_found)  // Only start, if the pins are configured
  {
    if(digitalRead(Pin(GPIO_PYQ_PIR_DL))) {
      if (!bmove)  {
        bmove = 1;
        MqttPublishSensor();
      }
      coold = PYQ1548_CoolDown;
      AddLog(LOG_LEVEL_DEBUG, PSTR("PYQ: MOVEMENT!"));
      pinMode(Pin(GPIO_PYQ_PIR_DL),OUTPUT);
      digitalWrite(Pin(GPIO_PYQ_PIR_DL),LOW);
      delayMicroseconds(100);
      pinMode(Pin(GPIO_PYQ_PIR_DL),INPUT);
    }else{
      if (bmove == 1 && coold >= 0 ) {
        coold--;
      }
      if (coold <= 0 && bmove == 1 ) {
        bmove = 0;
        MqttPublishSensor();
      }
    }

    if (confwrite <= 0){
      writeregval(Pin(GPIO_PYQ_PIR_SER), PYQ1548_Config.regval );
      confwrite = PYQ1548_ReActivate;
    }else{
      confwrite--;
    }
    return;
  }
  return;
}

void PYQ1548Show(bool json)
{
  if (PinUsed(GPIO_PYQ_PIR_DL) && PinUsed(GPIO_PYQ_PIR_SER) && pyq1548_found) {
    if (json) {
      ResponseAppend_P(PSTR(",\"PYQ\":{\"Movement\": %s}"), bmove?"true":"false");
  #ifdef USE_WEBSERVER
    } else {
      WSContentSend_PD("{s}Movement {m}%s {e}", bmove?"Moved!":"Nothing");
  #endif  // USE_WEBSERVER
    }
  }
  return;
}

bool PYQ1548CommandSensor(void)
{
  bool serviced = true;
  uint8_t paramcount = 0;
  if (XdrvMailbox.data_len > 0) {
    paramcount=1;
  } else {
    serviced = false;
    return serviced;
  }
  char argument[XdrvMailbox.data_len];
  UpperCase(XdrvMailbox.data,XdrvMailbox.data);
  
  //Let the User set the Sensitivity(Threshold) between 1..255
  if (!strcmp(ArgV(argument, 1),"SENS")) {
    uint16_t setval = atoi(ArgV(argument, 2));
    if ((setval >= 1) && (setval <= 255)) {
      PYQ1548_Config.field.threshold = setval;
      serviced = true;
    }else{
      AddLog(LOG_LEVEL_ERROR, PSTR("PYQ: sens out of range!"));
      serviced = false;
    }
  }
  //Let the User set the Blind-Time between 0..15
  if (!strcmp(ArgV(argument, 1),"BLIND")) {
    uint16_t setval = atoi(ArgV(argument, 2));
    if ((setval >= 0) && (setval <= 15)) {
      PYQ1548_Config.field.blind_time = setval;
      serviced = true;
    }else{
      AddLog(LOG_LEVEL_ERROR, PSTR("PYQ: blind out of range!"));
      serviced = false;
    }
  }
  //Let the User set the Pulse-Counter between 0..3
  if (!strcmp(ArgV(argument, 1),"PULSE")) {
    uint16_t setval = atoi(ArgV(argument, 2));
    if ((setval >= 0) && (setval <= 3)) {
      PYQ1548_Config.field.pulse_counter = setval;
      serviced = true;
    }else{
      AddLog(LOG_LEVEL_ERROR, PSTR("PYQ: pulse out of range!"));
      serviced = false;
    }
  }
  //Write Config if parameter have been set, and reset conf write timer
  if (serviced) { 
    writeregval(Pin(GPIO_PYQ_PIR_SER), PYQ1548_Config.regval );
    confwrite = PYQ1548_ReActivate;
  }
  return serviced;
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns90(uint8_t function)
{
  bool result = false;
  switch (function) {
    case FUNC_INIT:
      result = PYQ1548Init();
      break;
    case FUNC_EVERY_SECOND:
      PYQ1548Reading();
      result = true;
      break;
    case FUNC_JSON_APPEND:
      PYQ1548Show(1);
      break;
    case FUNC_COMMAND_SENSOR:
      if (XSNS_90 == XdrvMailbox.index) {
        result = PYQ1548CommandSensor();
      }
      break;   
#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      PYQ1548Show(0);
      break;
#endif  // USE_WEBSERVER
  }
  return result;
}

#endif  // USE_PYQ1548
