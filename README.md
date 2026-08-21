# Fault Detection and Correction System

A practical fault detection and correction system built using two Arduino Unos.

The system is divided into four layers, where each layer has a separate purpose. The equipment generates the parameters that need to be monitored, the interfacing layer conditions and transfers those signals, the fault detection layer identifies faults, and the fault correction layer takes action to correct them.

## System Architecture

The system consists of four main layers:

1. **Equipment Layer**
2. **Interfacing Layer**
3. **Fault Detection Layer**
4. **Fault Correction Layer**

### 1. Equipment Layer

Acts as the equipment whose parameters need to be monitored and corrected.

Currently, potentiometers are used to simulate equipment parameters such as:

- Load
- Temperature

An Arduino Uno is used to read these parameters and generate equivalent PWM signals.

The potentiometers allow the equipment conditions to be changed manually, making it possible to simulate normal operation, warning conditions, and fault conditions.

### 2. Fault Detection Layer

Detects faults in the parameters received from the equipment.

Currently, the system monitors:

- Load
- Temperature

Each parameter has:

- Normal range
- Warning limit
- Fault limit
- Reset limit

The system also uses hysteresis through separate fault and reset limits. This prevents small fluctuations around a limit from repeatedly changing the system state.

The possible states are:

- `NORMAL`
- `WARNING`
- `FAULT`
- `SENSOR_ERROR`

The fault detection system also identifies the cause of a fault:

- Load overload
- Over temperature
- Sensor failure

### 3. Interfacing Layer

Connects the equipment to the fault detection system.

The equipment Arduino generates PWM signals corresponding to the simulated equipment parameters. These signals are then received by the second Arduino.

This layer is responsible for the signal interfacing and conditioning required between the equipment and fault detection system.

The interfacing layer allows the fault detection system to work with signals representing real equipment parameters instead of directly depending on the potentiometers.

### 4. Fault Correction Layer

Takes action when an actual equipment fault is detected.

Currently, a motor is used to represent the fault correction mechanism.

The motor is activated when:

- Load reaches the fault limit
- Temperature reaches the fault limit

A sensor failure does **not** activate the motor, since a sensor malfunction is not itself an equipment fault that should trigger the correction mechanism.

The system also includes DIP-switch overrides:

- **Force ON**: Motor remains ON regardless of sensor values.
- **Force OFF**: Motor remains OFF regardless of sensor values.
- **Normal**: Motor is controlled by the detected equipment fault.
- **Invalid**: Both override switches active; motor remains OFF.

The buzzer is also overridden by the DIP switches.

## System Flow

```text
             ┌─────────────────────┐
             │   Equipment Layer   │
             │                     │
             │  Potentiometers     │
             │  Load / Temperature │
             └──────────┬──────────┘
                        │
                        │ PWM
                        ▼
             ┌─────────────────────┐
             │ Interfacing Layer   │
             │                     │
             │ Signal Conditioning │
             │ Signal Transfer     │
             └──────────┬──────────┘
                        │
                        │ Conditioned Signal
                        ▼
             ┌─────────────────────┐
             │ Fault Detection     │
             │      Layer          │
             │                     │
             │ Normal / Warning    │
             │ Fault / Sensor Error│
             └──────────┬──────────┘
                        │
                        │ Fault
                        ▼
             ┌─────────────────────┐
             │ Fault Correction    │
             │      Layer          │
             │                     │
             │ Motor / Buzzer      │
             │ DIP Overrides       │
             └─────────────────────┘
```