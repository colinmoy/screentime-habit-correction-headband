# Zhiyuan Chen Worklog

- [Zhiyuan Chen Worklog](#zhiyuan-chen-worklog)
- [2026-01-30 - Project Concept and Team Formation](#2026-01-30---project-concept-and-team-formation)
- [2026-02-03 - Project Proposal Draft](#2026-02-03---project-proposal-draft)
- [2026-02-10 - Design Document Feedback Round](#2026-02-10---design-document-feedback-round)
- [2026-02-17 - Tolerance Analysis on Power System](#2026-02-17---tolerance-analysis-on-power-system)
- [2026-02-24 - Block Diagram and Physical Design](#2026-02-24---block-diagram-and-physical-design)
- [2026-03-10 - Breadboard Prototype with ESP32-C6](#2026-03-10---breadboard-prototype-with-esp32-c6)
- [2026-03-14 - App Scaffold and Connect Page](#2026-03-14---app-scaffold-and-connect-page)
- [2026-03-19 - Graph Page with Fake Data](#2026-03-19---graph-page-with-fake-data)
- [2026-03-25 - Stats Page with Summary Statistics](#2026-03-25---stats-page-with-summary-statistics)
- [2026-03-28 - WiFi AP and HTTP Server](#2026-03-28---wifi-ap-and-http-server)
- [2026-03-30 - React Native App Scaffold with Expo Go](#2026-03-30---react-native-app-scaffold-with-expo-go)
- [2026-04-01 - App Crash from TabIcon Infinite Loop](#2026-04-01---app-crash-from-tabicon-infinite-loop)
- [2026-04-03 - Breadboard Prototype with ESP32-C6](#2026-04-03---breadboard-prototype-with-esp32-c6)
- [2026-04-09 - IMU Algorithm and Sensor Calibration Approach](#2026-04-09---imu-algorithm-and-sensor-calibration-approach)
- [2026-04-14 - PCB Arrives, Initial Assembly](#2026-04-14---pcb-arrives-initial-assembly)
- [2026-04-20 - First Flash Attempt and Bootloader Issue](#2026-04-20---first-flash-attempt-and-bootloader-issue)
- [2026-04-21 - Power Rail Mystery: 3.03V at TP1, 14mV at ESP32 VDD](#2026-04-21---power-rail-mystery-303v-at-tp1-14mv-at-esp32-vdd)
- [2026-04-23 - ESP32 Module Reflow](#2026-04-23---esp32-module-reflow)
- [2026-04-24 - I2C Sensor Scan on PCB](#2026-04-24---i2c-sensor-scan-on-pcb)
- [2026-04-25 - Reflow Attempt Made Things Worse](#2026-04-25---reflow-attempt-made-things-worse)
- [2026-04-26 - Pivot to Protoboard with Breakout Boards](#2026-04-26---pivot-to-protoboard-with-breakout-boards)
- [2026-04-27 - Real Data Integration with Separate Distance/Posture Sessions](#2026-04-27---real-data-integration-with-separate-distanceposture-sessions)
- [2026-04-28 - Final Integration and Lessons Learned](#2026-04-28---final-integration-and-lessons-learned)

## 2026-01-30 - Project Concept and Team Formation

**Objectives:** Define the project concept, identify the user problem the device will address, and establish team roles.

**Work completed:**

Decided on the Screentime Habit Correction Headband concept. The system is a wearable that monitors two posture-related metrics: distance from the screen (warn at <12 inches) and head tilt angle (warn at >15 degrees from baseline). Identified two main physical components, a wearable headband and a stationary desktop control box to keep weight off the user's head.

Drafted team roles:
- Power lead
- Sensor integration lead
- Processing/feedback lead
- App lead

The split between a head-worn unit and a desktop unit was driven by weight: putting the battery and most electronics in the box keeps the headband under 50 g, which should be comfortable for multi-hour wear.

---

## 2026-02-03 - Project Proposal Draft

**Objectives:** Write the project proposal, define quantitative success metrics, and select primary components.

**Work completed:**

Wrote the introduction, problem statement, and high-level requirements. Cited public health data on musculoskeletal disorders from prolonged screen use.

Defined success metrics (Table 2.1):

| Metric | Target |
|--------|--------|
| ToF distance accuracy | ±0.5 inch |
| IMU angle accuracy | ±2° |
| Alarm response time | ≤3 s |
| Battery runtime | ≥2 hr |
| Historical data span | 1 week |

Component selections:
- MCU: ESP32-S3
- IMU: ICM-42670-P
- ToF sensor: VL53L0X

Drafted ethics section addressing IEEE Code of Ethics 1.1 (safety) and ACM Code 1.6 (privacy).

**References:**
- IEEE Code of Ethics, Section 1.1
- ACM Code of Ethics, Section 1.6

---

## 2026-02-10 - Design Document Feedback Round

**Objectives:** Review the design document, resolve internal inconsistencies, and finalize the power chain topology.

**Work completed:**

Caught a power subsystem conflict: the document referenced an LP2950CZ-5.0 (a 5V regulator) but the block diagram showed direct 3.3V regulation from the 3.7V battery. Resolved by adopting a two-stage chain (Eq. 3.1):

```text
Eq. 3.1:  V_batt (3.7V) → Buck Converter → 5V → LDO → 3.3V
```

Calculated peak current draw (Eq. 3.2):

```text
Eq. 3.2:  I_peak = I_ESP32_TX + I_IMU + I_ToF + I_motor + I_buzzer + 2·I_LED
                 = 240 + 0.5 + 14 + 100 + 30 + 2(20)
                 ≈ 425 mA
```

The original parts list specified a 100 mA regulator. Replaced with LDL1117S33R (800 mA, 3.3 V output) to handle the 425 mA peak with margin.

---

## 2026-02-17 - Tolerance Analysis on Power System

**Objectives:** Identify the highest-risk feasibility item and demonstrate it meets specification through mathematical analysis.

**Work completed:**

Identified the 5 V rail (±100 mV spec) as the highest-risk item. Used Root-Sum-Square stacking for independent error sources (Eq. 4.1):

```text
Eq. 4.1:  σ_total = sqrt(σ_1² + σ_2² + ... + σ_n²)
```

Original buck converter (TPS62133): worst-case ±114.5 mV → only 15 mV margin against the ±100 mV spec. Switched to TPS62150: worst-case ±57.5 mV → 42.5 mV margin.

Battery sag at peak load (Eq. 4.2):

```text
Eq. 4.2:  ΔV_batt = I_load · R_internal
                  = 0.662 A · 0.10 Ω
                  = 66.2 mV
```

Documented failure modes: LDO dropout, battery undervoltage cutoff at 3.0 V, and thermal stress on the buck converter under sustained 662 mA load.

---

## 2026-02-24 - Block Diagram and Physical Design

**Objectives:** Finalize the system block diagram and lock in mechanical dimensions for the headband and desktop box.

**Work completed:**

Finalized the block diagram with color-coded voltage levels: battery (red), 5 V (orange), 3.3 V (yellow), I/O (green), wired data (blue), wireless data (purple).

Mechanical dimensions:

| Component | Dimensions |
|-----------|------------|
| Headband | 220 mm × 25 mm |
| Desktop box | 150 mm × 100 mm × 80 mm |

Component placement:
- **Headband:** ToF sensor (front-center), IMU (top), vibration motor (temple)
- **Desktop box:** battery, TP4056 charger, buck converter, LDO, ESP32, 2× LEDs, buzzer, ON/OFF switch, USB-C charging port

Cable between headband and box carries: 5 V power, I2C data lines (SDA/SCL), GND, GPIO control for vibration motor.

---

## 2026-03-10 - Breadboard Prototype with ESP32-C6

**Objectives:** Build a breadboard prototype to demonstrate basic functionality with ESP32, buttons, and LEDs.

**Work completed:**

Set up breadboard with ESP32 DevKit C. Installed all applications to laptop to program the ESP32.

```cpp
if (distanceBad) {
    if (!distanceWarningActive) {
      distanceWarningActive = true;
      distanceStartTime = currentMillis;
      Serial.println("[WARNING] Screen too close. Activating moderate feedback.");
    }
    
    if (!distanceAlarmActive && (currentMillis - distanceStartTime >= thresholdDelay)) {
      distanceAlarmActive = true;
      Serial.println("[ALARM] Distance threshold exceeded (3s). Activating full feedback.");
    }
  } else {
    if (distanceWarningActive || distanceAlarmActive) {
      Serial.println("[IDLE] Distance corrected.");
    }
    distanceWarningActive = false;
    distanceAlarmActive = false;
  }

  if (postureWarningActive || distanceWarningActive) {
    digitalWrite(ledWarning, HIGH);
  } else {
    digitalWrite(ledWarning, LOW);
  }

  digitalWrite(ledPostureAlarm, postureAlarmActive ? HIGH : LOW);
  digitalWrite(ledDistanceAlarm, distanceAlarmActive ? HIGH : LOW);
}
```

Above is a snippet with logic for the distance bad alarm. This turns on a warning LED for when a screen is too close. The state machine has three levels: idle (no LEDs), warning (warning LED on within first 3 s of bad posture), and alarm (dedicated alarm LED on after the 3 s threshold).

![Breadboard demo](Screenshot%202026-05-07%20034932.png)

---

## 2026-03-14 - App Scaffold and Connect Page

**Objectives:** Scaffold the React Native app using Expo Go and implement the first of three pages, the Connect page, with simulated connection logic.

**Work completed:**

Set up the Expo project with `npx create-expo-app screentime-headband`. The app uses Expo Router for file-based routing, and each `.tsx` file under `app/(tabs)/` automatically becomes a tab.

**Initial file structure:**

```text
app/
├── _layout.tsx              ← root layout
├── (tabs)/
│   ├── _layout.tsx          ← tab bar configuration
│   ├── index.tsx            ← Connect page
│   ├── explore.tsx          ← (placeholder for Graph)
│   └── stats.tsx            ← (placeholder for Stats)
```

**Tab bar: `(tabs)/_layout.tsx`:**

Configured the bottom tab bar with three entries (Connect, Graph, Stats) using emoji icons (📡 / 📊 / 🔢) and a dark color scheme (`#1a1a2e`) to match the planned chart styling.

**Page 1: `index.tsx` (Connect):**

Renders a connection status indicator and a single button that toggles between "Connect" and "Disconnect." For now the connection is purely simulated (a 2-second `setTimeout` flips a `useState` between three states). The interface is structured so the simulated logic can later be swapped out for a real BLE or WiFi call without touching the UI layout.

```tsx
const [status, setStatus] = useState<"disconnected" | "connecting" | "connected">("disconnected");

const handleConnect = () => {
  setStatus("connecting");
  setTimeout(() => setStatus("connected"), 2000);
};
```

**Verification:**

Ran `npx expo start` and scanned the QR code with Expo Go on the phone. App launches, the Connect tab renders, and the button cycles through "Disconnect → Connecting → Connected" states correctly. Other two tabs are still empty placeholders.

---

## 2026-03-19 - Graph Page with Fake Data

**Objectives:** Implement the Graph page with bar and line charts, using fake session data so the UI can be developed independently of the ESP32 firmware.

**Work completed:**

Installed charting dependencies:

```bash
npx expo install react-native-chart-kit react-native-svg
```

**Fake data model (`data/fakeSessions.ts`):**

Each session represents the elapsed time between startup or the last alarm event and the next alarm trigger, recorded for both posture and distance.

```ts
export const FAKE_SESSIONS = [
  { id: 1, timestamp: "10:00", postureTime: 320, distanceTime: 410 },
  { id: 2, timestamp: "10:30", postureTime: 280, distanceTime: 350 },
  { id: 3, timestamp: "11:00", postureTime: 410, distanceTime: 290 },
  // ...10 entries total for the initial demo
];
```

This single-record-with-both-fields model puts posture and distance as one entry. Distance and posture alarms are independent events with no shared timing (flagged as a known issue to fix before real data integration).

**Page 2: `explore.tsx` (Graph):**

Imports `BarChart` and `LineChart` from `react-native-chart-kit`. Renders three charts using `FAKE_SESSIONS`:

| Chart | Data | Color |
|-------|------|-------|
| Bar chart | `postureTime` per session | Blue |
| Bar chart | `distanceTime` per session | Red |
| Line chart | Both metrics over time, overlaid | Blue + Red |

Color coding (blue = posture, red = distance) will be consistent across all three pages so trends are easy to read at a glance.

```tsx
const labels = FAKE_SESSIONS.map(s => s.timestamp);
const postureData = FAKE_SESSIONS.map(s => s.postureTime);
const distanceData = FAKE_SESSIONS.map(s => s.distanceTime);
```

**Verification:**

Reloaded the app via Expo Go. All three charts render with the fake data. Bar heights match the values in `FAKE_SESSIONS`, and the overlaid line chart correctly shows both series.

---

## 2026-03-25 - Stats Page with Summary Statistics

**Objectives:** Implement the Stats page showing summary statistics and a raw session log table, completing the initial three-page app.

**Work completed:**

**Page 3: `stats.tsx` (Stats):**

Displays a scrollable table of all sessions with computed summary statistics above it. Statistics are calculated separately for `postureTime` and `distanceTime` and displayed side by side.

Statistics computed (Eq. 10.1):

```text
Eq. 10.1:  mean   = Σ x_i / n
           median = middle value of sorted x
           min    = smallest x_i
           max    = largest x_i
```

```tsx
const mean = (arr: number[]) => arr.reduce((a, b) => a + b, 0) / arr.length;

const median = (arr: number[]) => {
  const sorted = [...arr].sort((a, b) => a - b);
  const mid = Math.floor(sorted.length / 2);
  return sorted.length % 2 ? sorted[mid] : (sorted[mid - 1] + sorted[mid]) / 2;
};

const min = (arr: number[]) => Math.min(...arr);
const max = (arr: number[]) => Math.max(...arr);
```

The raw session log below the statistics shows session ID, timestamp, posture time, and distance time in a table:

| ID | Time | Posture (s) | Distance (s) |
|----|------|-------------|--------------|
| 1  | 10:00 | 320 | 410 |
| 2  | 10:30 | 280 | 350 |
| ... | ... | ... | ... |

**Verification:**

Reloaded the app. All three tabs now functional end-to-end with fake data:

- Connect: simulated connect/disconnect cycles correctly.
- Graph: all three charts render with `FAKE_SESSIONS` data.
- Stats: summary statistics match values computed by hand for the fake data, and the session log displays all 10 entries.

**Known issues to address later:**

- Session data model conflates posture and distance into one record (needs to become two independent arrays before integrating real data).
- No real device communication yet (all data is hardcoded).
- No data persistence between app launches (adding AsyncStorage planned alongside real data integration).

---

## 2026-03-28 - WiFi AP and HTTP Server

**Objectives:** Replace the planned BLE link with a WiFi HTTP server and define the data API for the mobile app.

**Work completed:**

Switched from BLE to WiFi HTTP for two reasons:
1. Expo Go does not support `react-native-ble-plx` without a custom dev client.
2. HTTP polling is simpler for a demo than implementing BLE GATT services.

ESP32 WiFi AP configuration:

| Parameter | Value |
|-----------|-------|
| SSID | Headband-AP |
| Auth mode | WPA2-PSK |
| IP address | 192.168.4.1 |
| Max connections | 1 |

Endpoints (Table 16.1):

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/ping` | GET | Connection test |
| `/distance` | GET | Distance alarm session log |
| `/posture` | GET | Posture alarm session log |

Hit a CMakeLists.txt issue: needed to add the following to `REQUIRES`:

```cmake
REQUIRES driver nvs_flash esp_wifi esp_http_server esp_timer esp_netif esp_event
```

None of these except `driver` were in my initial component list.

---

## 2026-03-30 - React Native App Scaffold with Expo Go

**Objectives:** Set up the React Native app with three navigation pages and verify the UI works on a physical phone.

**Work completed:**

Scaffolded the app with `npx create-expo-app screentime-headband`. Installed:
- `react-native-chart-kit` (graphing)
- `react-native-svg` (chart-kit dependency)

Three pages with bottom-tab navigation:

| Tab | Purpose |
|-----|---------|
| Connect | IP entry, connect/disconnect, status display |
| Graph | Bar charts (time-to-alarm per session), line chart over time |
| Stats | Raw session table with mean/median/min/max |

Started with hardcoded `FAKE_SESSIONS` array so the UI could be verified without depending on the ESP32 being online. App runs on phone via the Expo Go QR code.

---

## 2026-04-01 - App Crash from TabIcon Infinite Loop

**Objectives:** Diagnose and fix immediate-on-launch app crash.

**Work completed:**

App crashed immediately on launch. Two bugs:

1. `TabIcon` component recursively called itself:

```tsx
   function TabIcon({ emoji, color }: { emoji: string; color: string }) {
     return <TabIcon emoji={emoji} color={color} />;  // infinite loop
   }
```

   Fixed by returning a `<Text>` element with the emoji.

2. Chart code was placed in `stats.tsx` but should have been in `explore.tsx`. The two files had been swapped at some point.

Also noted React 19.1 / React Native 0.81 compatibility quirks with chart libraries, but `react-native-chart-kit` works fine once the recursion bug is fixed.

After fixes, all three tabs render and the fake-data demo works on the phone.

---

## 2026-04-03 - Breadboard Prototype with ESP32-C6

**Objectives:** Build a breadboard prototype to verify I2C communication with the VL53L0X before committing the PCB design.

**Work completed:**

Built breadboard with ESP32-C6 DevKit, VL53L0X breakout, vibration motor, and buzzer. First I2C attempts failed with timeout errors.

Two root causes identified:

1. **No external pull-ups.** Was relying on ESP32 internal pull-ups (~45 kΩ), which are too weak for reliable I2C. Standard pull-up calculation (Eq. 7.1):

```text
Eq. 7.1:  R_pullup = (V_dd - V_OL_max) / I_OL
                   = (3.3 - 0.4) / 3 mA
                   ≈ 970 Ω (minimum)
```

Standard practice is 4.7 kΩ at 400 kHz, which I added.

2. **GPIO0 is a strapping pin.** Was using GPIO0 for SDA, which has boot-time behavior that interferes with normal GPIO use on the C6. Switched to GPIO8/GPIO9.

After both fixes, I2C scanner detected the VL53L0X at 0x29.

![Breadboard with ToF](Screenshot%202026-05-07%20034925.png)

---

## 2026-04-09 - IMU Algorithm and Sensor Calibration Approach

**Objectives:** Decide on a tilt-detection algorithm that does not suffer from drift, and define a calibration procedure.

**Work completed:**

Initial design was dead reckoning (double-integrate accelerometer to get position), but integrated accelerometer noise drifts over time. Switched to gravity-referenced tilt, and read the accelerometer's pitch and roll angles directly (Eq. 8.1, 8.2):

```text
Eq. 8.1:  pitch = atan2(a_x, sqrt(a_y² + a_z²))
Eq. 8.2:  roll  = atan2(a_y, sqrt(a_x² + a_z²))
```

Drift is zero because gravity always points "down" in the sensor frame.

Calibration procedure:
1. At power-on, record baseline pitch, roll, and ToF distance for 2 seconds.
2. Compare live readings against the baseline.
3. Trigger warning if `|pitch - pitch_baseline| > 15°` for >3 s.

Recalibration button planned to handle baseline shifts when the user changes chair height or desk setup mid-session.

---

## 2026-04-14 - PCB Arrives, Initial Assembly

**Objectives:** Receive and populate the first PCB revision.

**Work completed:**

First PCB came in. Began component soldering:
- ESP32-S3-WROOM-1 module (hot-air reflow)
- Voltage regulators (hand-soldered)
- VL53L0X and ICM-42670-P sensors (hot-air reflow)
- Passives, headers, connectors (hand-soldered)

Visually the joints look acceptable but will know once power is applied.

![PCB and battery](pcb.jpg)

---

## 2026-04-20 - First Flash Attempt and Bootloader Issue

**Objectives:** Flash a blink test to the assembled PCB and verify basic GPIO control.

**Work completed:**

The USB-to-UART adapter has only 3V3, TXD, RXD, GND, 5V (no RTS or DTR pins). Auto-reset into bootloader does not work. Manual bootloader sequence:

1. Hold GPIO0 to GND with a jumper wire.
2. Press EN button to reset.
3. Release EN (keep GPIO0 grounded).
4. Run `idf.py -p COM5 flash`.
5. Release GPIO0 once "Connecting..." switches to "Writing at...".

Blink test ran but LED behavior was inverted. Tracing the schematic showed the LEDs are driven through SS8050 BJTs, making them active-low, GPIO HIGH turns the LED off, GPIO LOW turns it on. Updated firmware logic accordingly.

---

## 2026-04-21 - Power Rail Mystery: 3.03V at TP1, 14mV at ESP32 VDD

**Objectives:** Diagnose why the ESP32 will not boot reliably from battery power despite a working regulator output.

**Work completed:**

Multimeter readings:

| Test point | Voltage |
|------------|---------|
| Battery terminal | 3.95 V |
| Buck converter output | 4.98 V |
| LDO output (TP1) | 3.03 V |
| ESP32-S3 VDD pin | 0.014 V |

The PCB trace from TP1 to the ESP32 VDD pad shows continuity, so the trace itself is intact. The ESP32 module's VDD pad must not be making electrical contact despite visual appearance of a solder joint.

Also noted that the LDO output (3.03 V) is below the 3.3 V target. The LiPo at 3.95 V leaves only ~0.65 V of headroom over the 3.3 V target, and the LDO has a typical dropout of ~0.5 V. When the battery sags, the LDO loses regulation.

We tested if the power system worked better with the battery charging since the voltage would increase.

![alt text](Screenshot%202026-05-07%20034725.png)

---

## 2026-04-23 - ESP32 Module Reflow

**Objectives:** Restore the ESP32 VDD connection and verify the module functions on battery power.

**Work completed:**

Reflowed all pads on the ESP32-S3 module with extra flux and solder. Ground pins now show good continuity, but VDD still reads at the same low voltage.

Replaced the entire ESP32 module. New chip:
- VDD reads 3.187 V (within spec)
- Chip stays cool, confirming no internal short
- Successful flash on second attempt

Discovered a separate issue: the PCB stops working after the USB cable is unplugged, even though battery is connected. Traced this to a likely floating or inconsistent ground rather than a power supply problem (USB cable is providing a ground path that the PCB itself does not reliably provide).

---

## 2026-04-24 - I2C Sensor Scan on PCB

**Objectives:** Verify both sensors are reachable on the PCB I2C bus and confirm sensor identity.

**Work completed:**

Wrote an I2C scanner using native ESP-IDF I2C driver on GPIO12 (SDA) and GPIO13 (SCL). Results:

```text
Scanning I2C bus...
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
20: -- -- -- -- -- -- -- -- -- 29 -- -- -- -- -- --
60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
Found 1 device(s) on I2C bus
```

- VL53L0X detected at 0x29. Model ID register (0xC0) reads 0xEE (correct).
- ICM-42670-P **not** detected at 0x68 or 0x69.

Since the VL53L0X works, the I2C bus, pull-ups, and ESP32 I2C peripheral are all functional. The IMU specifically has bad solder joints.

Wrote VL53L0X SPAD calibration code. Calibration completes successfully (count=12, type=aperture), but actual ranging measurements time out. This pattern (basic register reads work, but full sensor operation fails) suggests AVDD or XSHUT pins on the VL53L0X are not fully connected, even though the I2C pins are.

---

## 2026-04-25 - Reflow Attempt Made Things Worse

**Objectives:** Reflow the IMU and VL53L0X to fix the bad solder joints found in the previous entry.

**Work completed:**

Attempted reflow of both sensors with a hot air gun. Result: **VL53L0X is now also undetected**. The reflow pushed a working part into a non-working state.

Likely causes:
1. Used too much heat: solder may have wicked away from pads.
2. Sensor shifted during heating: pins no longer aligned with pads.
3. Insufficient solder paste: produced cold joints.

Conclusion: "heat and hope" is not a real plan. Proper SMD reflow requires:
- Removing the part with hot air and tweezers.
- Cleaning pads with solder wick + isopropyl alcohol.
- Applying fresh paste in measured amounts (50-75% pad coverage).
- Carefully aligning the part.
- Controlled preheating before reflow temperature.

Without a hot plate and stencil, hand reflow of QFN-style sensor packages has a low success rate.

---

## 2026-04-26 - Pivot to Protoboard with Breakout Boards

**Objectives:** Salvage the demo by replacing the failed PCB-mounted sensors with breakout boards on a protoboard.

**Work completed:**

Decided to abandon the SMD-soldered sensors on the PCB. Switched to:
- Adafruit VL53L0X breakout board
- Separate ICM-42670-P breakout

Both wired to the ESP32-S3 PCB via the GPIO12/GPIO13 test points and the 3.3 V/GND test points. The PCB ESP32 still drives the firmware; only the sensors moved off-board.

The breakout boards have built-in 4.7-10 kΩ pull-ups, which is friendlier for I2C signal integrity than my PCB's 10 kΩ pull-ups combined with the long cable run to the headband.

The original PCB-mounted sensors remain a known issue, to be documented in the final report.

![protoboard](protoboard_with_sensors.jpg)

---

## 2026-04-27 - Real Data Integration with Separate Distance/Posture Sessions

**Objectives:** Replace fake data with live polling of the ESP32 endpoints, and persist sessions across app restarts.

**Work completed:**

Built a `SessionContext` provider that polls `/distance` and `/posture` every 3 seconds when connected.

Initially modeled each session as a single record holding both `timeToDistanceAlarm` and `timeToPostureAlarm`. Realized this is wrong since distance and posture alarms are independent events with no shared timing. Refactored data model:

```ts
type Session = {
  id: number;
  timestamp: string;
  timeToDistanceAlarm: number | null;
  timeToPostureAlarm: number | null;
};
```

App keeps two arrays: `distanceSessions` and `postureSessions`. Each entry has exactly one of the two timing fields populated.

Persistence via AsyncStorage, capped at 100 entries (oldest dropped when full) to bound memory.

Polling interval: 3 s. Storage key: `screentime_sessions`.

![Screenshot of data being read](Screenshot%202026-05-07%20034604.png)
In the image above, the readings on the ESP32 are being sent to the laptop through the USB port.

---

## 2026-04-28 - Final Integration and Lessons Learned

**Objectives:** Verify end-to-end system function and document key lessons for the final demo.

**Work completed:**

Full system flow verified:

```text
[Protoboard sensors] → [ESP32-S3 firmware: state machine IDLE → WARNING → ALARM]
                    → [WiFi AP serving session data]
                    → [React Native app polling and graphing]
```

![Final Design](final_design.jpg)

Key lessons:

- Breadboard prototypes hide real-world hardware problems. Trace capacitance, cable length, ground integrity, and pull-up sizing all behave differently on a PCB.
- LDO dropout headroom matters. A 3.7 V LiPo into a 3.3 V LDO has almost no margin once the battery sags under load.
- Strapping pins (GPIO0) and active-low transistor outputs both caused trouble, and schematic-level details that don't show up until firmware runs.
- SMD reflow is unforgiving. "Heat and hope" makes things worse without a hot plate, fresh paste, and good alignment.
- Switching from BLE to WiFi HTTP cost almost nothing for a demo and saved days of fighting Expo's BLE limitations.
- I2C bus scanning is the right first diagnostic. It tells you whether the bus, addresses, and wiring are sane before touching any sensor-specific code.
