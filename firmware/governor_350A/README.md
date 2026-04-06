# Firmware — governor_350A

Arduino sketch for the Beechcraft Bonanza digital prop governor. Targets the **Arduino Nano (ATmega328P, 5V, 16MHz)**.

---

## Before You Flash

**One setting must be correct for your engine before uploading:**

```cpp
// governor_350A.ino — top of file
#define PULSES_PER_REV  3   // 6-cyl Continental (IO-470, IO-520, E-185/E-225)
                            // Change to 2 for 4-cylinder engines
```

All early Bonanzas use Continental 6-cylinder engines. `3` is correct for all of them. Getting this wrong causes the RPM reading to be off by 33% and the governor will not hold correctly.

---

## Flashing the Firmware

### Option A — Arduino IDE (simplest)

1. **Install Arduino IDE 2.x** from [arduino.cc/en/software](https://www.arduino.cc/en/software)

2. **Install required libraries** via *Tools → Manage Libraries*:
   - `Adafruit SSD1306` (by Adafruit) — installs `Adafruit GFX Library` automatically
   - Accept all dependency installs when prompted

3. **Open the sketch** — `File → Open → firmware/governor_350A/governor_350A.ino`

4. **Set the board** — *Tools → Board → Arduino AVR Boards → Arduino Nano*

5. **Set the processor** — *Tools → Processor → ATmega328P*
   > If upload fails with the default setting, try *ATmega328P (Old Bootloader)*. Nanos purchased after ~2018 typically have the old bootloader despite being sold as standard.

6. **Select the port** — *Tools → Port → COM? (Arduino Nano)*
   - On Windows: check Device Manager under Ports (COM & LPT)
   - On Mac/Linux: `/dev/cu.usbserial-*` or `/dev/ttyUSB*`

7. **Verify** (optional) — Click the tick icon (✓) to compile without uploading. Confirms no errors before touching the board.

8. **Upload** — Click the arrow icon (→). The IDE compiles and flashes. The Nano's RX/TX LEDs will blink rapidly during upload, then the board resets and runs.

9. **Open Serial Monitor** — *Tools → Serial Monitor*, baud rate `9600`. You should see:
   ```
   350A Digital Governor v2.0
   PULSES_PER_REV=3
   RAMP_RATE=100 RPM/sec
   Boot: SW position 3 (CRUISE)
   Ready. Type ? for commands.
   ```

---

### Option B — PlatformIO (recommended for ongoing development)

PlatformIO integrates with VS Code and handles library management automatically. A `platformio.ini` is included in this directory.

1. **Install VS Code** from [code.visualstudio.com](https://code.visualstudio.com)

2. **Install the PlatformIO IDE extension** — search *PlatformIO IDE* in the VS Code Extensions panel

3. **Open the firmware directory** — *File → Open Folder → firmware/governor_350A*
   > Open the `governor_350A` folder itself, not the repo root. PlatformIO expects `platformio.ini` to be at the folder root.

4. **Set `PULSES_PER_REV`** in `governor_350A.ino` if needed.

5. **Build** — Click the checkmark (✓) in the PlatformIO toolbar (bottom status bar), or run:
   ```
   pio run
   ```
   PlatformIO auto-downloads the AVR toolchain and Adafruit libraries on first build.

6. **Upload** — Connect the Nano via USB, then click the arrow icon (→) in the toolbar, or:
   ```
   pio run --target upload
   ```

7. **Serial monitor** — Click the plug icon in the toolbar, or:
   ```
   pio device monitor --baud 9600
   ```

**Expected build output (approximate):**
```
RAM:   [=======   ]  66.7% (used 1367 bytes from 2048 bytes)
Flash: [========  ]  78.1% (used 23998 bytes from 30720 bytes)
[SUCCESS]
```

Flash usage is high because the Adafruit SSD1306 + GFX libraries are large. There is approximately 6KB of flash headroom available. RAM has ~680 bytes free at runtime; the call stack lives here so don't add large local arrays.

---

### Option C — Arduino CLI (headless / automation)

Useful for CI or machines without a GUI.

1. **Install arduino-cli** — download from [arduino.cc/en/software](https://www.arduino.cc/en/software) (scroll to CLI section), or:
   ```bash
   # macOS
   brew install arduino-cli

   # Linux / Windows (PowerShell)
   curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
   ```

2. **Initialize and install the AVR core:**
   ```bash
   arduino-cli config init
   arduino-cli core update-index
   arduino-cli core install arduino:avr
   ```

3. **Install required libraries:**
   ```bash
   arduino-cli lib install "Adafruit SSD1306"
   arduino-cli lib install "Adafruit GFX Library"
   ```

4. **Compile:**
   ```bash
   arduino-cli compile \
     --fqbn arduino:avr:nano:cpu=atmega328 \
     firmware/governor_350A/governor_350A.ino
   ```
   > For the old bootloader variant: `--fqbn arduino:avr:nano:cpu=atmega328old`

5. **Find the port:**
   ```bash
   arduino-cli board list
   ```

6. **Upload:**
   ```bash
   arduino-cli upload \
     --fqbn arduino:avr:nano:cpu=atmega328 \
     --port /dev/ttyUSB0 \
     firmware/governor_350A/governor_350A.ino
   ```
   Replace `/dev/ttyUSB0` with your actual port (`COMx` on Windows).

---

## Serial Debug Console

While the Nano is connected via USB, a debug console is available at **9600 baud**. Open any serial terminal (Arduino IDE Serial Monitor, PlatformIO monitor, `screen`, PuTTY, etc.).

| Command | Action |
|---|---|
| `?` | Print command help |
| `s` | Print full status line |
| `a` | Switch to AUTO mode |
| `m` | Switch to MANUAL mode, disable relays |
| `c` | Clear active alarm |
| `1`–`5` | Simulate rotary switch position (for bench testing without hardware) |

**Example status output (`s` command):**
```
Mode=AUTO SW=CRUISE Target=2300 Ramped=2300 ACT=2287 Err=13 Relay=OFF
```

When in AUTO mode, the status line prints automatically every 2 seconds.

---

## Unit Tests

The firmware includes a [Unity](https://github.com/ThrowTheSwitch/Unity)-based test suite that runs on the **native desktop** — no Arduino hardware required.

### What Is Tested

Tests mock all Arduino/AVR hardware APIs (GPIO, timers, EEPROM, I2C, watchdog) and compile the `.ino` directly as a C++ translation unit. This allows the pure logic functions to be executed and verified offline.

| Module | Test cases | What is verified |
|---|---|---|
| `calculateRPM()` | 6 | Zero when no pulse interval; zero when mag signal timeout; correct RPM at all four preset values (2200, 2300, 2450, 2650) with 3 PPR; rolling average dampens a spike without wild RPM reading |
| `magSignalLost()` | 3 | Returns false when signal is fresh; returns false 1 ms before the 2-second timeout; returns true 1 ms after the timeout |
| `updateRamp()` | 5 | Ramps up toward higher target at correct rate (100 RPM/sec); ramps down toward lower target; snaps to target when remaining gap is smaller than one step; never overshoots; skips entirely when fullCoarse preset is active |
| `runControlLoop()` | 10 | Commands INC relay when RPM is below setpoint; commands DEC relay when above; cuts off both relays inside the ±25 RPM deadband; does not act in MANUAL mode; triggers ALARM and de-energizes relays on mag signal loss; triggers ALARM on RPM below 400 or above 3500; drives DEC continuously in fullCoarse (EMERGENCY) mode; commands full relay duty when error ≥ PROP_ZONE_RPM; commands proportional duty when error is inside the proportional zone |

**Total: 24 tests, 0 failures.**

### What Is Not Testable Without Hardware

| Functionality | Why it can't be unit tested |
|---|---|
| `readSwitch()` debounce | Requires `digitalRead()` returning a changing sequence over real time. The static state variables inside the function also can't be reset from outside. |
| Relay contact waveform (actual duty cycle) | The proportional relay drive calculates `millis() % CONTROL_INTERVAL_MS` to place the on-pulse within the 100ms window. Verifying the GPIO waveform shape requires a real-time environment with accurate timer semantics. |
| OLED rendering | `updateDisplay()` has no observable output in a mock environment. OLED correctness is verified visually during bench test. |
| Watchdog reset behavior | The ATmega328P watchdog timer is a hardware peripheral. `wdt_enable()` / `wdt_reset()` are no-ops in the test mock. Watchdog function is inherent to the MCU and doesn't require software verification. |
| EEPROM persistence across power cycles | The mock EEPROM is a RAM array that resets at program start. Testing true non-volatile persistence requires either the physical chip or a more complete simulator. |
| `loadSettings()` round-trip | Depends on EEPROM persistence. Functional correctness is verified during bench test by power-cycling and confirming settings are retained. |
| Optocoupler signal conditioning | Analog/electrical behavior of the 6N137 (slew rate, noise rejection). Verified with an oscilloscope at the bench test stage per [docs/BENCH_TEST.md](../../docs/BENCH_TEST.md). |
| Full system integration | End-to-end behavior (mag signal → interrupt → RPM → relay → motor → RPM) requires the complete hardware assembly. See `docs/BENCH_TEST.md` for the full ground test procedure. |

### Running the Tests

#### With PlatformIO and GCC (Linux / macOS / Windows with MinGW)

```bash
cd firmware/governor_350A
pio test -e native
```

PlatformIO automatically downloads the Unity library and compiles the test suite. Requires `gcc` / `g++` in your PATH.

- **Linux:** `sudo apt install gcc build-essential`
- **macOS:** `xcode-select --install`
- **Windows:** Install [MSYS2](https://www.msys2.org), then `pacman -S mingw-w64-x86_64-gcc`, and add `C:\msys64\mingw64\bin` to your PATH

#### With MSVC on Windows (VS 2017/2019/2022 Build Tools)

A batch script is provided for Windows machines with Visual Studio Build Tools but no MinGW:

```bat
cd firmware\governor_350A
.pio\build_test.bat
.pio\test_firmware.exe
```

> `build_test.bat` is generated in `.pio\` (gitignored). If it's missing, re-run `pio test -e native` once — it will fail at link but will have downloaded Unity and created the `.pio` directories. Then recreate the batch file or compile manually using the MSVC command in the next section.

**Manual MSVC compile** (if the batch file is missing):
```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

cl.exe /EHsc /std:c++14 /nologo ^
  /I "test\mocks" ^
  /I ".pio\libdeps\native\Unity\src" ^
  "test\test_firmware\test_firmware.cpp" ^
  ".pio\libdeps\native\Unity\src\unity.c" ^
  /Fe:".pio\test_firmware.exe"

.pio\test_firmware.exe
```

Adjust the VS path for your installed version (2017/2019/2022).

#### Expected output

```
test\test_firmware\test_firmware.cpp:319:test_rpm_zero_when_no_pulse_interval:PASS
test\test_firmware\test_firmware.cpp:320:test_rpm_zero_when_mag_signal_lost:PASS
...
test\test_firmware\test_firmware.cpp:348:test_control_proportional_duty_in_prop_zone:PASS

-----------------------
24 Tests 0 Failures 0 Ignored
OK
```

### Test File Layout

```
firmware/governor_350A/
├── governor_350A.ino          ← Firmware source
├── platformio.ini             ← Build config (AVR + native test environments)
└── test/
    ├── mocks/                 ← Arduino/AVR API stubs for native compilation
    │   ├── Arduino.h          ← millis(), micros(), digitalRead/Write, Serial, String
    │   ├── Wire.h             ← TwoWire stub
    │   ├── EEPROM.h           ← RAM-backed EEPROM stub
    │   ├── Adafruit_GFX.h     ← Empty stub (not exercised by logic tests)
    │   ├── Adafruit_SSD1306.h ← No-op method stubs
    │   └── avr/
    │       └── wdt.h          ← wdt_enable/wdt_reset no-ops
    └── test_firmware/
        └── test_firmware.cpp  ← 24 Unity test cases
```

---

## Tuning Parameters

All tuning constants are `#define`d at the top of the sketch. Defaults are conservative and appropriate for a first installation. Adjust after confirming basic operation.

| Constant | Default | Effect |
|---|---|---|
| `DEADBAND_RPM` | 25 | No correction within ±25 RPM of setpoint. Increase if relay chatters at target; decrease for tighter hold. |
| `PROP_ZONE_RPM` | 75 | Proportional slowdown starts this far from setpoint. Must be > `DEADBAND_RPM`. |
| `MIN_DUTY_PCT` | 25 | Minimum relay duty in the proportional zone (prevents relay from switching too briefly to move the motor). |
| `RAMP_RATE_RPM_PER_SEC` | 100 | How fast the setpoint walks to a new preset. At 100 RPM/sec, ECO→MAX (450 RPM) takes ~4.5 seconds. |
| `MAG_TIMEOUT_MS` | 2000 | Alarm if no magneto pulse for this long (engine idle produces ~90 Hz; 2 seconds is 180 missed pulses). |
| `MAX_RELAY_MS` | 8000 | Safety: cut relay if it's been continuously on for 8 seconds without the RPM moving. |
| `CONTROL_INTERVAL_MS` | 100 | Control loop cadence. Also the PWM period for proportional duty output. |
