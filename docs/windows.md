# Windows setup

These steps adapt the README's build instructions to Windows 10 and 11. We wrote them on Linux and haven't run them on
a Windows machine yet. If a step fails, copy the exact error message; most problems on Windows come from the USB driver
or from path length.

## 1. Install the USB driver

The DevKit talks to the PC through a CP2102 USB-serial chip.

1. Plug the board in and open **Device Manager**.
2. Look under **Ports (COM & LPT)** for **Silicon Labs CP210x USB to UART Bridge (COMx)**. Note the COM number.
3. If the board shows up under **Other devices** with a yellow warning sign instead, install the
   **CP210x Universal Windows Driver** from
   [Silicon Labs](https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers), then unplug and replug the
   board.

If nothing appears at all when you plug the board in, try another USB cable. Some cables only carry power.

## 2. Prepare short paths

The ESP-IDF build creates deeply nested paths and very long command lines, and Windows limits both. Builds that work on
Linux can fail on Windows with "cannot find source file" or "command too long".

1. Turn on long path support. Open PowerShell **as administrator** and run:

   ```powershell
   New-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" -Name "LongPathsEnabled" -Value 1 -PropertyType DWORD -Force
   ```

   Restart Windows afterwards.
2. Keep the project in a short folder without spaces, for example `C:\esp`. ESP-IDF refuses to build a project whose
   path contains a space.
3. If your Windows user name contains a space (`C:\Users\Jane Doe`), PlatformIO's own folder inherits it. Move that
   folder by adding a user environment variable `PLATFORMIO_CORE_DIR` with the value `C:\.platformio`, then restart VS
   Code.

## 3. Clone the repo

1. Install [Git for Windows](https://git-scm.com/download/win).
2. In PowerShell, enable long paths in Git too, then clone with submodules:

   ```powershell
   git config --global core.longpaths true
   git clone --recursive https://github.com/rbelanec/esp32-dualshock-servo.git C:\esp\esp32-dualshock-servo
   ```

   The repo is private. Git opens a browser window to sign in to GitHub the first time.

## 4. Build and flash with PlatformIO

1. Install [VS Code](https://code.visualstudio.com/) and the **PlatformIO IDE** extension. Restart VS Code when the
   extension asks.
2. **File → Open Folder** and pick `C:\esp\esp32-dualshock-servo`.
3. Click **Upload** (the → arrow in the blue status bar). The first build downloads the ESP-IDF toolchain and takes a
   long time.
4. Click the plug icon to open the serial monitor at 115200 baud. Then follow "Using it" in the README.

PlatformIO picks the COM port by itself. If you have several serial devices and it picks the wrong one, add these lines
under `[env:esp32dev]` in `platformio.ini`, with your COM number. Don't commit them, because the number differs per PC:

```ini
upload_port = COM5
monitor_port = COM5
```

If the build stops with `Python version must be between 3.10 and 3.13`, PlatformIO is running on a Python version this
ESP32 platform doesn't support. Install Python 3.13 from [python.org](https://www.python.org/downloads/windows/),
close VS Code, delete the folder `%USERPROFILE%\.platformio\penv` (or `C:\.platformio\penv` if you moved it in step 2),
and start VS Code again so PlatformIO reinstalls itself.

## 5. Or use the Arduino IDE

The Arduino IDE needs neither the long path setup nor PlatformIO. Install the USB driver (step 1), install the
[Arduino IDE](https://www.arduino.cc/en/software), and follow "Arduino IDE version" in the README. You can download the
repo as a ZIP from GitHub instead of cloning it; the Arduino sketch doesn't use the submodules.

## Troubleshooting

- **`could not open port 'COM5': PermissionError(13, 'Access is denied.')`**: another program holds the port. Close the
  serial monitor, the Arduino IDE and any other terminal program, then upload again.
- **`Failed to connect to ESP32: No serial data received`**: the board didn't enter its flashing mode by itself. Hold
  the **BOOT** button, start the upload, and release BOOT when `Connecting....` appears.
- **The servo vibrates but doesn't turn**: the servo needs its own 5 to 5.5 V supply. See "Wiring" in the README.
