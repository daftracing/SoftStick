/*
 * Description: 2-in-1 to either emulate Drift Mode and Drift Stick (DaftRacing)
 * or act as a CAN interface (https://www.carloop.io/apps/app-socketcan/)
 * 
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <carloop.h>

#define __VER__           "v100"

#define OPMODE_SOFTSTICK  0
#define OPMODE_SLCAN      1
#define OPMODE_DEFAULT    OPMODE_SOFTSTICK

int opmode = OPMODE_DEFAULT;
void (*loop_function)();
LEDStatus fadeGreen(RGB_COLOR_GREEN, LED_PATTERN_FADE, LED_SPEED_NORMAL, LED_PRIORITY_IMPORTANT);
LEDStatus fadeRed(RGB_COLOR_RED, LED_PATTERN_FADE, LED_SPEED_NORMAL, LED_PRIORITY_IMPORTANT);
LEDStatus fadeGray(RGB_COLOR_GRAY, LED_PATTERN_FADE, LED_SPEED_NORMAL, LED_PRIORITY_IMPORTANT);

const char NEW_LINE = '\r';
char inputBuffer[40];
unsigned inputPos = 0;

int drift, driftmode, drift_init, dd, lapai;
CANMessage lapamsg;
CANMessage dmsg;
Carloop<CarloopRevision2> carloop;

SYSTEM_MODE(MANUAL);
SYSTEM_THREAD(ENABLED);

void serialEvent() {
  parseInput(Serial.read());
}

void set_opmode(int m) {
  opmode = m;

  if(opmode == OPMODE_SOFTSTICK) {
    fadeGray.setActive();

    carloop.can().begin(500000);
    carloop.can().addFilter(0x0c8, 0xFFF);
    carloop.can().addFilter(0x1c0, 0xFFF);

    loop_function = &loop_softstick;
  } else {
    fadeGreen.setActive();
    carloop.can().clearFilters();
    carloop.can().addFilter(0x768, 0xF00);
    loop_function = &loop_slcan;
  }
}

void button_clicked(system_event_t event, int param) {
    int times = system_button_clicks(param);

    if(times >= 3)
      set_opmode(opmode ^ 1);
}

void parse_0c8(uint8_t data[]) {
  int hb = data[3] & 0xf0;

  if(hb == 0xf0) {
      if(!drift) {
        dd = 0; 
        drift_init = 1;
      }
      drift = 1;
  } else {
      drift = 0;
  }
}

void parse_1c0(uint8_t data[]) {
  int esc = data[1];

  if(esc == 0xd0) {
    if(!driftmode) {
      driftmode = 1;
      fadeRed.setActive();
    }
  } else {
    if(driftmode) {
      driftmode = 0;
      fadeGray.setActive();
    }
  }
}

void getMessage() {
  CANMessage message;

  while(carloop.can().receive(message)) {
    switch (message.id) {
      case 0x0c8:
        parse_0c8(message.data);
      break;
      case 0x1c0:
        parse_1c0(message.data);
      break;
    }
  }
}

void lapaSim() {
  if(lapai == 16)
    lapai = 0;

  lapamsg.data[0] = 0xfe - lapai - drift;
  lapamsg.data[1] = (lapai << 4) + 2 + (drift << 2);
  carloop.can().transmit(lapamsg);

  lapai++;
}

void setupVars() {
  drift = 0;
  driftmode = 0;
  drift_init = 0;
  lapai = 0;
  dd = 0;

  lapamsg.id = 0x1c5;
  lapamsg.len = 8;
  memset(lapamsg.data, 0x0, 8);

  dmsg.id = 0x420;
  dmsg.len = 8;
  memset(dmsg.data, 0x0, 8);
  dmsg.data[6] = 0x12;
  dmsg.data[7] = 0xc4;
}

void setup() {
  float battVoltage;

  System.on(button_final_click, button_clicked);
  WiFi.off();

  setupVars();
  Serial.begin(9600);
  
  delay(3500);
  Serial.printf("Daftracing SoftStick (multimode) %s build %s\n\n", __VER__, __TIMESTAMP__);

  battVoltage = carloop.readBattery();
  if(battVoltage < 12.5 && battVoltage > 7) {
    delay(100);
    System.sleep(SLEEP_MODE_DEEP, 45);
  }

  carloop.begin();

  set_opmode(OPMODE_DEFAULT);
}

void loop() {
  return loop_function();
}

void loop_softstick() {
  static unsigned long last = 0, lastbatt = 0, strikes = 0;
  unsigned long m;
  float battVoltage;
  
  m = millis();

  if(!lastbatt || m < lastbatt || (m - lastbatt > 5000)) {
      battVoltage = carloop.readBattery();
      if(battVoltage < 12.5 && battVoltage > 7)
        strikes++;
      else
        strikes = 0;      
      
      if(strikes > 2) {
        delay(100);
        System.sleep(SLEEP_MODE_DEEP, 60);
      }
      lastbatt = m;
  }

  if(driftmode && drift && (dd % 10 == 0 || drift_init == 1)) {
    carloop.can().transmit(dmsg);
    drift_init = 0;
    delay(50);
  }
  dd++;

  getMessage();

  if(millis() - last < 40)
      return;
  last = m;
  lapaSim();
}

void loop_slcan()
{
  if (carloop.can().isEnabled()) {
    receiveMessages();
  }
}

void receiveMessages() {
  CANMessage message;
  while(carloop.can().receive(message))
  {
    printReceivedMessage(message);
  }
}

void printReceivedMessage(const CANMessage &message)
{
  Serial.printf("t%03x%d", message.id, message.len);
  for(auto i = 0; i < message.len; i++) {
    Serial.printf("%02x", message.data[i]);
  }
  Serial.write(NEW_LINE);
}

void parseInput(char c)
{
  if (inputPos < sizeof(inputBuffer))
  {
    inputBuffer[inputPos] = c;
    inputPos++;
  }

  if (c == NEW_LINE)
  {
    switch (inputBuffer[0])
    {
      case 'O':
        openCAN();
        break;
      case 'C':
        closeCAN();
        break;
      case 'S':
        changeCANSpeed(&inputBuffer[1], inputPos - 2);
        break;
      case 't':
        transmitMessage(&inputBuffer[1], inputPos - 2);
        break;
      case 'v':
        Serial.printf("Daftracing SoftStick (multimode) %s build %s\n", __VER__, __TIMESTAMP__);
        break;
    }

    inputPos = 0;
  }
}

bool opened()
{
  return carloop.can().isEnabled();
}

void openCAN()
{
  set_opmode(OPMODE_SLCAN);
}

void closeCAN()
{
  set_opmode(OPMODE_SOFTSTICK);
}

void changeCANSpeed(const char *buf, unsigned n)
{
  if (n == 0)
  {
    return;
  }

  unsigned speed = 0;
  switch (buf[0])
  {
    case '0': speed = 10000; break;
    case '1': speed = 20000; break;
    case '2': speed = 50000; break;
    case '3': speed = 100000; break;
    case '4': speed = 125000; break;
    case '5': speed = 250000; break;
    case '6': speed = 500000; break;
    case '7': speed = 800000; break;
    case '8': speed = 1000000; break;
    default: return;
  }

  carloop.can().begin(speed);
}

unsigned hex2int(char c)
{
  if (c >= '0' && c <= '9')
  {
    return c - '0';
  }
  else if (c >= 'a' && c <= 'f')
  {
    return c - 'a' + 10;
  }
  else if (c >= 'A' && c <= 'F')
  {
    return c - 'A' + 10;
  }

  return 0;
}

void transmitMessage(const char *buf, unsigned n)
{
  if (!opened())
  {
    return;
  }

  CANMessage message;

  if (n < 4) {
    return;
  }

  message.id = (hex2int(buf[0]) << 8) | (hex2int(buf[1]) << 4) | hex2int(buf[2]);
  message.len = hex2int(buf[3]);
  buf += 4;
  n -= 4;

  if (message.len > 8)
  {
    return;
  }

  for (unsigned i = 0; i < message.len && n >= 2; i++, buf += 2, n -= 2)
  {
    message.data[i] = (hex2int(buf[0]) << 4) | hex2int(buf[1]);
  }

  carloop.can().transmit(message);
}

