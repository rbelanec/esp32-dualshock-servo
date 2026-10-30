# ESP32 DualShock 4 servo

An ESP32 pairs with a PlayStation 4 DualShock 4 controller over Bluetooth, and the left stick turns a hobby servo:
full left is 0°, center is 90°, full right is 180°.

All project code is in `main/dualshock_servo.cpp`. The build setup comes from Ricardo Quesada's
[esp-idf-arduino-bluepad32-template](https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template).
`arduino/dualshock_servo/` holds the same program as a sketch for the Arduino IDE.

## Hardware

| Part | What we used |
| --- | --- |
| Board | ESP32 DevKit with a CP2102 USB-serial chip. Servo and controller tested on an ESP32-D0WDQ6 (rev 1.0); the firmware also ran on an ESP32-D0WD-V3 (rev 3.1). |
| Controller | DualShock 4, second generation (USB ID 054c:09cc) |
| Servo | ElecFreaks EF92A, 180°, runs on 3 to 5.5 V |

The board has to be a classic ESP32. The DualShock 4 uses Bluetooth Classic (BR/EDR), which the ESP32-S3, C3 and C6
don't have.

## Wiring

| Servo wire | Connect to |
| --- | --- |
| Orange (signal) | GPIO 4 |
| Red (power) | 5 to 5.5 V supply |
| Brown (ground) | GND on the ESP32 |

Powered from the DevKit's 5V pin with the board on USB, our servo only vibrated and never turned. It moved once we
switched its power to a 5.5 V source. If the servo's supply is separate from the ESP32, connect the two grounds, or the
servo can't read the signal and jitters.

## Build and flash

The project uses PlatformIO in VS Code.

1. Clone with submodules (Arduino core and BTstack):

   ```sh
   git clone --recursive <this repo's URL>
   ```

   If you already cloned without `--recursive`, run `git submodule update --init --recursive`.
2. PlatformIO needs Python 3.10 to 3.13 for this platform. With Python 3.14 the build stops with
   `Python version must be between 3.10 and 3.13`.
3. Open the folder in VS Code with the PlatformIO extension and click **Upload** (the → arrow in the status bar). Upload
   builds first, so you don't need to click Build separately. The first build downloads the ESP-IDF toolchain and takes
   a long time; later builds are incremental.
4. Open the serial monitor (the plug icon) at 115200 baud.

From a terminal: `pio run -t upload`, then `pio device monitor`.

On Linux, if the upload fails with "permission denied" on `/dev/ttyUSB0`, give your user access to the CP2102 with a
udev rule, then unplug and replug the board:

```sh
echo 'SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", TAG+="uaccess"' \
  | sudo tee /etc/udev/rules.d/70-esp32-serial.rules
sudo udevadm control --reload-rules
```

## Arduino IDE version

`arduino/dualshock_servo/dualshock_servo.ino` does the same as the PlatformIO build: same pins, same servo timing, same
serial messages. It builds against Bluepad32's own board package instead of this repo's ESP-IDF setup.

1. In **File → Preferences → Additional boards manager URLs**, add both:

   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   https://raw.githubusercontent.com/ricardoquesada/esp32-arduino-lib-builder/master/bluepad32_files/package_esp32_bluepad32_index.json
   ```

2. In **Tools → Board → Boards Manager**, install `esp32` by Espressif, then `esp32_bluepad32` by Ricardo Quesada.
3. Select **Tools → Board → ESP32 + Bluepad32 Arduino → ESP32 Dev Module** and your serial port.
4. Open `arduino/dualshock_servo/dualshock_servo.ino` and click **Upload**.
5. Open the serial monitor at 115200 baud.

The sketch differs from `main/dualshock_servo.cpp` in two places:

- It prints with `Serial`, like the board package's own Controller example.
- `esp32_bluepad32` 4.1.0 is built on Arduino ESP32 core 2.0.17. Core 2.x drives PWM through numbered LEDC channels
  (`ledcSetup()`, `ledcAttachPin()`, `ledcWrite(channel, ...)`), while core 3.x, which the PlatformIO build uses,
  attaches by pin (`ledcAttach()`, `ledcWrite(pin, ...)`). The sketch picks the right calls with
  `ESP_ARDUINO_VERSION_MAJOR`, so it keeps compiling if the board package moves to core 3.x.

GitHub Actions compiles the sketch on every push, but only the PlatformIO build has run on hardware.

## Using it

1. Power the board. The servo sweeps to 0°, 180° and back to 90°. This self-test runs on every boot, so you can check
   the servo wiring without a controller.
2. Pair the controller once: hold **Share + PS** until the light bar double-blinks white.
3. After that, press **PS** to reconnect. Pairings survive resets and power cycles.
4. Move the left stick left and right.

The serial monitor shows what is happening:

```
Servo: 0 degrees, pulse 544 us
Servo: 180 degrees, pulse 2400 us
Servo: 90 degrees, pulse 1472 us
[2s] Scanning... press PS on a paired DualShock 4, or hold Share + PS to pair a new one
CALLBACK: Controller is connected, index=0
Controller model: DualShock 4, VID=0x054c, PID=0x09cc
Servo: 149 degrees, pulse 2080 us
[127s] Controller 0 connected: DualShock 4, battery 49%
```

On boards with an LED on GPIO 2, the LED blinks fast while scanning and stays on while a controller is connected.

## Why it works like this

### ESP-IDF with Arduino as a component

Bluepad32, the library that talks to the controller, runs on the BTstack Bluetooth stack. The Arduino core for ESP32
ships with a different stack (Bluedroid), and the two can't run together. The project therefore builds with ESP-IDF,
includes the Arduino core as an ESP-IDF component, and adds BTstack and Bluepad32 as further components. You still
write `setup()` and `loop()` in `dualshock_servo.cpp`, but the Arduino IDE can't build this ESP-IDF project. For
the Arduino IDE, use the sketch in `arduino/`, which builds against Bluepad32's board package instead.

### `Console` instead of `Serial`

Bluepad32 runs an interactive console (`bp32>`) on the USB serial port, and it conflicts with Arduino's `Serial`. Print
with `Console.printf()` instead. `Serial` works only after you disable the console with
`CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE=n` in `sdkconfig.defaults`.

### Which controllers work

Bluepad32 only talks to Bluetooth controllers. We tested the DualShock 4. A PS5 DualSense should work with the same
code: pair it with **Create + PS**. A wireless Xbox 360 controller can't work, because it doesn't use Bluetooth. It
talks to Microsoft's USB receiver over a proprietary 2.4 GHz protocol that the ESP32 radio can't speak.

### Pairings are kept across resets

The template calls `BP32.forgetBluetoothKeys()` in `setup()`, which deletes all pairings on every boot and forces you to
pair again after each reset. This project doesn't call it. The pairing key lives in the NVS flash partition, which
uploads don't erase. If a controller misbehaves, calling `forgetBluetoothKeys()` once and pairing again can clear it.

### Servo signal

A hobby servo reads a pulse every 20 ms (50 Hz), and the pulse width sets the angle. `servoWrite()` makes that pulse
with the ESP32's LEDC PWM hardware: 16-bit duty resolution, 544 µs for 0° and 2400 µs for 180°, the same range as the
Arduino Servo library. The EF92A has no published pulse range. If yours hits its end stops and buzzes at 0° or 180°,
narrow `SERVO_MIN_US` and `SERVO_MAX_US`.

The stick reads -512 to 512. Readings less than 32 from center count as center, so the servo doesn't twitch when you
let go. `servoWrite()` only updates the PWM and prints when the angle changes.

`loop()` waits 20 ms per pass, one servo period, so the servo follows the stick without lag. The template waited
150 ms, which would update the servo only about 7 times a second.

### Pin choices

GPIO 6 to 11 connect to the board's SPI flash chip. Never wire anything to the pins labeled CLK, SD0, SD1, SD2, SD3 or
CMD. When we had a display shield wired to the board, the ESP32 could no longer read its flash (`Manufacturer: ff` in
esptool) and uploads failed with "MD5 of file does not match data in flash". With the shield removed, the flash read
normally.

GPIO 0, 2, 5, 12 and 15 are strapping pins that the chip reads at reset. GPIO 12 is the worst: pulled high at boot, it
switches the flash supply to 1.8 V and the board can't boot. GPIO 4 has no such role, and a servo signal input doesn't
drive the pin anyway.

## Known issues

- **Reconnects but sends no input.** After the ESP32 restarted while the controller was connected, the controller came
  back connected but never sent input: the status line said `battery unknown` and the servo ignored the stick. We
  logged this after a reset from the laptop and saw the same symptom after a power cycle. The log was missing the
  `DS4: Calibration report received` line that a working connection prints. Fix: hold PS for 10 s until the light bar
  turns off, then pair again with Share + PS. We haven't found the cause.
- **Board stopped printing.** Twice the board stopped printing anything, including the 2-second status line, after a
  VS Code serial monitor session. Pressing EN recovered it. We don't know whether the monitor caused it.
- **Upload fails with "The chip stopped responding" or an MD5 mismatch.** Unplug anything wired to the board and
  upload again. In our tests this came from a sagging supply or a wire on a flash pin.
- **The serial port name changes.** After a replug the board can come back as `/dev/ttyUSB1`. Close any open monitor
  first, because a monitor on the old name holds the port.

## Licenses

- Template code and Bluepad32: Apache-2.0. `bluepad32_arduino` is Apache-2.0 and LGPL-2.1-or-later. See `LICENSE`.
- BTstack: free for personal, non-commercial use. Commercial use needs a license from BlueKitchen. See
  `external/btstack/LICENSE`.
