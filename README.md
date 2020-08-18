# SoftStick
Carloop firmware that emulates Drift Stick hardware or acts as a SLcan interface (https://www.carloop.io/apps/app-socketcan/).

ABS software part is also needed to make the RS actually drift.

## Install
Carloop Basic (based on Particle Photon):
```
$ bash <( curl -sL https://particle.io/install-cli )
$ particle update
$ particle dfu
$ particle flash --usb ./SoftStick.bin
```

Alternatively compile the source code and flash a Carloop device using Prticle Web IDE or Particle Workbench (https://www.particle.io/workbench).

## Usage

The firmware will automatically change the mode of operation if serial connection is opened/closed. It is also possible to force the change by quickly pressing setup button 3 times.

#### As a CAN interface (blinks green)
1. Connect it to an USB port *before* plugging it to an OBD port
2. Start SLcand
  ```
  $ sudo slcand -f -o -c -s6 /dev/ttyACM0 can0
  ```
3. Plug it to an OBD port

#### As a DriftStick (blinks white/red)
1. Plug the device to an OBD port (no USB connection)
2. Start the engine -> it should wake up and blink white
3. Disable ESC by holding the ESC OFF button for 5 sek.
4. If it blinks red it's ready to go!
5. Hand brake sensor will trigger the drift. No need to actually engage the hand brake, input from the sensor is all that is needed

It will also spoof CAN frames to make the RS drift in all Drive Modes.

In DriftStick operation mode the device will deep sleep if the engine is not running.
