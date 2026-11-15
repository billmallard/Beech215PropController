# Firmware — governor_350A

Arduino sketch for the Beechcraft Bonanza digital prop governor. Targets the **Arduino Nano (ATmega328P, 5V, 16MHz)**.

**Firmware v3.0 requires hardware Rev 3.3.** The RPM setpoint comes from the original-style panel potentiometer (10k linear, two wires on connector pins E and F, read on Nano pin A7). The rotary switch, presets and EEPROM settings of v2.0 are gone. For Rev 3.2 and earlier boards use the v2.0 sketch from git history.

How it behaves, deliberately matching the original 350A box:

- **Power-up means govern.** The aircraft prop switch only powers the governor in AUTO, so the firmware starts in AUTO with no command needed.
- **No magneto signal: no action.** Relays are held off (within 2 seconds of the last pulse) and governing resumes by itself when the signal returns. This is a standby, not a latched alarm.
- **Knob full clockwise = `POT_RPM_MAX`, full counter-clockwise = `POT_RPM_MIN`,** linear in between. An open pot circuit is a standby (`POT OPEN`, relays off, self-recovering). A shorted pot circuit reads as full clockwise, as on the original.
- **Whenever governing starts or resumes, the setpoint starts at the actual RPM** and walks to the knob position at `RAMP_RATE_RPM_PER_SEC`.
- **The ramp never makes the governor drive away from the knob setting.** The tracked setpoint is kept between the actual RPM and the knob setting. If the RPM moves toward the knob setting faster than the ramp (throttle advanced, engine starting or restarting), the ramp rides the actual RPM and no relay is driven; if the RPM is already beyond the knob setting, the governor works straight against the knob setting.
- **The OLED is optional.** It is probed once at boot; with nothing on the J2 service header there is no I2C traffic at all.
- **No upper RPM limit.** However high the reading, the governor commands a decrease, as the original does. (A 3500 RPM cut-off existed until 2026-11-15; it made the governor go quiet in exactly the overspeed it should be correcting.)
- **Nothing latches.** `RPM LOW` (a reading below `RPM_MIN_VALID`, 400) is a standby like the other two: relays off while it lasts, governing resumes by itself. It is the one standby that does not restart the ramp, so the governor commands an increase against the knob setting at once when the reading comes back up (an engine being restarted in flight). The alarm state, `ALARM` mode and the serial `c` command were removed 2026-11-15.
- **No limit on how long a relay may be commanded.** The output is held until the RPM reaches the knob setting, as on the original; the aircraft's limit switches stop the motor at the pitch stops. The 8-second `RELAY TIMEOUT` alarm was removed 2026-11-15: it latched the governor off whenever the prop rested on a stop.

---

## Before You Flash

**Three settings must be correct for your engine before uploading:**

```cpp
// governor_350A.ino — top of file
#define PULSES_PER_REV  3   // 6-cyl Continental (IO-470, IO-520, E-185/E-225)
                            // Change to 2 for 4-cylinder engines

#define POT_RPM_MAX  2650   // Knob full clockwise: take-off RPM
#define POT_RPM_MIN  1800   // Knob full counter-clockwise: lowest RPM
```

All early Bonanzas use Continental 6-cylinder engines. `3` is correct for all of them. Getting this wrong causes the RPM reading to be off by 33% and the governor will not hold correctly.

`POT_RPM_MAX` / `POT_RPM_MIN` are the two ends of the panel knob. **The shipped values are placeholders pending a decision — set them for the aircraft and engine before flight.** `POT_RPM_MAX` takes the place of the take-off RPM trim screw of the original 350A (which shipped set to 2300 unless ordered otherwise).

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
   350A Digital Governor v3.0
   PULSES_PER_REV=3
   Knob range=1800-2650 RPM
   RAMP_RATE=100 RPM/sec
   No OLED (optional) - running without display
   Boot: knob -> 2225 RPM
   AUTO. Type ? for commands.
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

**Build output for v3.0 (PlatformIO 6.1.19, Adafruit SSD1306 2.5.17, GFX 1.13.0):**
```
RAM:   [===       ]  31.4% (used 643 bytes from 2048 bytes)
Flash: [=======   ]  73.1% (used 22470 bytes from 30720 bytes)
[SUCCESS]
```

Flash usage is high because the Adafruit SSD1306 + GFX libraries are large. There is approximately 8KB of flash headroom available. The static RAM figure does not include the display's 1024-byte frame buffer, which the library takes from the heap **only when an OLED is detected at boot**. That buffer has to fit in what is left, so static RAM must stay under roughly 800 bytes or a fitted display silently fails to start. With a display running, about 340 bytes remain for the stack (estimated worst-case use is under 200) — don't add large local arrays or RAM-resident strings. All `Serial`/`display` text is wrapped in `F()` to keep it in flash for this reason; the sketch uses no `String` objects and no EEPROM. With no display fitted about 1360 bytes are free.

> `platformio.ini` restricts the AVR build to the sketch (`build_src_filter`). Without that filter `pio run` also tries to compile `test/` for AVR and fails on `unity.h`.

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
| `a` | Bench override: back to AUTO (the power-up state) |
| `m` | Bench override: MANUAL — relays held off |

None of these are needed in the aircraft. There is no command to simulate the knob — connect a 10k pot (or a fixed resistor) across J1 pins E and F.

**Example status output (`s` command):**
```
Mode=AUTO Hold=- PotADC=341 Target=2225 Ramped=2225 ACT=2203 Err=22 Relay=OFF
```

`Hold` shows why AUTO is not driving (`NO_MAG`, `POT_OPEN`, or `-` when governing). `PotADC` is the filtered pot reading: 0 at full clockwise, about 512 at full counter-clockwise, about 1023 with the pot disconnected. When in AUTO mode, the status line prints automatically every 2 seconds.

---

## Unit Tests

The firmware includes a [Unity](https://github.com/ThrowTheSwitch/Unity)-based test suite that runs on the **native desktop** — no Arduino hardware required.

### What Is Tested

Tests mock all Arduino/AVR hardware APIs (GPIO, ADC, timers, I2C, watchdog) and compile the `.ino` directly as a C++ translation unit. This allows the logic — including the real `setup()` and `loop()` — to be executed and verified offline against mock time.

| Module | Test cases | What is verified |
|---|---|---|
| `magPulseISR()` | 5 | First pulse reports no interval; interval between pulses; ringing edges inside the 2 ms blanking window are ignored and do not move the timing reference; a pulse at 3500 RPM, far beyond any redline, is never rejected; a dropout longer than 200 ms restarts timing without reporting the gap as an RPM |
| `calculateRPM()` | 6 | Zero when no pulse interval; zero when mag signal timeout; correct RPM at 1800, 2300 and 2650 with 3 PPR; rolling average dampens a spike |
| `magSignalLost()` / `checkMagSignal()` | 5 | Timeout boundary (1 ms either side of 2 s); nothing is disturbed while the signal is fresh; on loss the relays drop in every mode, the mode is unchanged, and the RPM average is reset so it must refill |
| `potAdcToRPM()` | 7 | Full CW = `POT_RPM_MAX`; full CCW = `POT_RPM_MIN`; mid-travel = midpoint; linear in pot resistance at quarter and three-quarter travel; clamps beyond both ends (including a worn 12k pot and an open reading); monotonic and in range for every ADC code; open threshold sits between a worn pot and an open wire |
| `updatePot()` / `readPotAdc()` | 9 | First reading seeds the target; target follows the knob; both end stops are reached exactly; ±1 count of ADC noise for 10 s never moves the target (three knob positions); an open reading trips only after 300 ms and never moves the target; a briefly lifting wiper is ignored; recovery is debounced and self-clearing; a worn 12k pot is minimum RPM, not a fault; the pot pin is averaged |
| `updateRamp()` | 5 | Ramps up and down at 100 RPM/sec; snaps when within one step; never overshoots; a full-range knob slam is rate-limited |
| `runControlLoop()` | 21 | INC below setpoint, DEC above, off inside the deadband; no action in MANUAL; mag loss is a non-latching standby; no drive until the RPM average has refilled; pot open holds relays off and recovers without a step; first governing pass seeds the ramp at actual RPM; there is no upper RPM limit (3501, 4000 and 6000 all command DEC); a reading below 400 is a standby with relays off and the mode still AUTO; 400 itself is governed; the low-RPM standby ends by itself, straight to INC with the ramp left at the knob setting; a ramp restart already pending (engine start) survives the low-RPM standby; the ramp never commands a decrease while the RPM rises toward the knob setting faster than the ramp, nor an increase while it falls toward it; RPM already beyond the knob setting is governed against the knob setting; a ramp lying between the RPM and the knob setting is left alone; a relay command has no time limit (60 s of continuous drive, then normal reversal); full duty outside the proportional zone; proportional duty inside it |
| `setup()` / `loop()` | 8 | Boots straight into AUTO with relays off and the target read from the pot; boot with an open pot stays in `POT OPEN` and never drives; no display means exactly one I2C probe and no further traffic; a detected display is enabled; 10 s with no magneto signal never drives and never alarms; governs by itself once the signal is valid, ramp starting at actual RPM; a signal dropout drops the relays within 2 s, stays in AUTO, and resumes by itself at the new RPM without a false alarm; ringing on every pulse does not corrupt the RPM reading |

**Total: 66 tests, 0 failures.**

### What Is Not Testable Without Hardware

| Functionality | Why it can't be unit tested |
|---|---|
| ADC accuracy of the pot input | The mock ADC returns ideal counts. Real offset, noise and the 10k/100nF input filter are checked on the bench (BENCH_TEST Test 3). |
| Relay contact waveform (actual duty cycle) | The proportional relay drive calculates `millis() % CONTROL_INTERVAL_MS` to place the on-pulse within the 100ms window. Verifying the GPIO waveform shape requires a real-time environment with accurate timer semantics. |
| OLED rendering | `updateDisplay()` has no observable output in a mock environment. OLED correctness is verified visually during bench test. |
| Watchdog reset behavior | The ATmega328P watchdog timer is a hardware peripheral. `wdt_enable()` / `wdt_reset()` are no-ops in the test mock. Watchdog function is inherent to the MCU and doesn't require software verification. |
| I2C bus fault handling | The Wire timeout (`WIRE_HAS_TIMEOUT` block) that stops a stuck bus from stalling the loop only exists in the real AVR core. |
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
test\test_firmware\test_firmware.cpp:920:test_isr_first_pulse_reports_no_interval:PASS
test\test_firmware\test_firmware.cpp:921:test_isr_reports_interval_between_pulses:PASS
...
test\test_firmware\test_firmware.cpp:999:test_loop_ringing_on_every_pulse_does_not_corrupt_rpm:PASS

-----------------------
66 Tests 0 Failures 0 Ignored
OK
```

### Test File Layout

```
firmware/governor_350A/
├── governor_350A.ino          ← Firmware source
├── platformio.ini             ← Build config (AVR + native test environments)
└── test/
    ├── mocks/                 ← Arduino/AVR API stubs for native compilation
    │   ├── Arduino.h          ← millis(), micros(), digitalRead/Write, analogRead, Serial
    │   ├── Wire.h             ← TwoWire stub with a settable "display present" flag
    │   ├── Adafruit_GFX.h     ← Empty stub (not exercised by logic tests)
    │   ├── Adafruit_SSD1306.h ← No-op method stubs
    │   └── avr/
    │       └── wdt.h          ← wdt_enable/wdt_reset no-ops
    └── test_firmware/
        └── test_firmware.cpp  ← 66 Unity test cases
```

---

## Tuning Parameters

All tuning constants are `#define`d at the top of the sketch. Defaults are conservative and appropriate for a first installation. Adjust after confirming basic operation.

| Constant | Default | Effect |
|---|---|---|
| `DEADBAND_RPM` | 25 | No correction within ±25 RPM of setpoint. Increase if relay chatters at target; decrease for tighter hold. |
| `PROP_ZONE_RPM` | 75 | Proportional slowdown starts this far from setpoint. Must be > `DEADBAND_RPM`. |
| `MIN_DUTY_PCT` | 25 | Minimum relay duty in the proportional zone (prevents relay from switching too briefly to move the motor). |
| `RAMP_RATE_RPM_PER_SEC` | 100 | How fast the setpoint walks toward the knob position (and from actual RPM whenever governing starts). At 100 RPM/sec a full-range knob sweep (850 RPM) takes ~8.5 seconds. |
| `MAG_TIMEOUT_MS` | 2000 | Relays off (standby) if no valid magneto pulse for this long. |
| `MIN_PULSE_US` | 2000 | Edges closer than this to the last accepted magneto pulse are treated as ringing and ignored. Must stay well under the shortest real period (5714 µs at 3500 RPM, 3 pulses/rev). |
| `MAX_PULSE_US` | 200000 | A gap longer than this restarts pulse timing instead of being read as a (very low) RPM. |
| `POT_OPEN_ADC` | 630 | A pot reading above this (about 16kΩ) is an open circuit. A worn 10k pot reading 12kΩ is still valid. |
| `POT_OPEN_MS` | 300 | How long an open (or recovered) reading must persist before the state changes. |
| `POT_HYST_RPM` | 5 | The target only moves when the knob has moved at least this far, so a stationary knob never jitters. |
| `RPM_MIN_VALID` | 400 | A reading below this is not governed (relays off, `RPM LOW`, self-recovering). There is no upper limit. |
| `CONTROL_INTERVAL_MS` | 100 | Control loop cadence. Also the PWM period for proportional duty output. |
