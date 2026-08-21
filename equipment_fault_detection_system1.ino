const int loadPin = A0;
const int tempPin = A1;

// DIP 1 = D5 = FORCE OFF (force all extra fault detection mechanisms and fault correction mechanisms off)
// DIP 2 = D4 = FORCE ON (force all extra fault detection mechanisms off, and fault corrections on)
// LEDS don't count as 'extra' fault detection mechanisms, they are fundamental, and always tell what's happening, unless latched

const int forceOffPin = 5;
const int forceOnPin  = 4;

const int motorPin = 6;
const int resetButton = 7;

const int greenLED  = 8;
const int yellowLED = 9;
const int redLED    = 10;

const int buzzerPin = 2;


// machine states for sensors, didn't use enum, since they don't work in TinkerCAD
const int NORMAL       = 0;
const int WARNING      = 1;
const int FAULT        = 2;
const int SENSOR_ERROR = 3;


// Modes of operation, depending on dip switches
const int MODE_NORMAL    = 0;
const int MODE_FORCE_ON  = 1;
const int MODE_FORCE_OFF = 2;
const int MODE_INVALID   = 3;


// Causes of faults
const int NO_FAULT         = 0;
const int LOAD_OVERLOAD    = 1;
const int OVER_TEMPERATURE = 2;
const int SENSOR_FAILURE   = 3;


// warning and fault limits, as well as reset limits
// reset limits tell the system when to change its state
// limits are put there, since sensors usually
// give values varying by little amount due to noise and other factors
// this makes it so that values varying and hovering on both sides' limit don't cause immediate change
const int LOAD_WARNING_LIMIT = 150;
const int LOAD_FAULT_LIMIT   = 200;

const int LOAD_WARNING_RESET = 145;
const int LOAD_FAULT_RESET   = 195;

const int TEMP_WARNING_LIMIT = 150;
const int TEMP_FAULT_LIMIT   = 200;

const int TEMP_WARNING_RESET = 145;
const int TEMP_FAULT_RESET   = 195;


// how many recordings to sample, before changing the values
const int SAMPLE_COUNT = 5;


// report interval is for serial monitor
const unsigned long SAMPLE_INTERVAL = 100;
const unsigned long REPORT_INTERVAL = 500;


// Initializing sensor values
int loadValue = 0;
int temperatureValue = 0;


// Initializing sensor states
int loadState = NORMAL;
int tempState = NORMAL;


// Initializing machine states
int machineState = NORMAL;


// Initializing fault states
int faultLatched = 0;
int faultCause = NO_FAULT;


// Initializing timings
unsigned long lastSampleTime = 0;
unsigned long lastReportTime = 0;


// Takes average for the no of sample counts, then forwards changes
int readFilteredSensor(int pin) {

    long total = 0;

    for (int i = 0; i < SAMPLE_COUNT; i++) {

        total += analogRead(pin);
    }

    return total / SAMPLE_COUNT;
}


// Checks if the value received from the sensor is within
// the valid 0-255 range
// values outside this range are treated as a sensor error
bool validSensorValue(int value) {

    if (value <= 0)
        return false;

    if (value >= 255)
        return false;

    return true;
}


// Converts the raw analog reading from 0-1023
// into the equivalent 0-255 sensor value
// the factor is approximately 1023 / 255
float calibrateLoad(int rawValue) {
    return rawValue / 4.0118;
}


// Same conversion is used for the temperature sensor
// since both sensors use the same 0-255 input range
float calibrateTemperature(int rawValue) {
    return rawValue / 4.0118;
}
//It seems like there's something wrong with the conversion logic,
//or perhaps the PWM output itself
//The maximum and minimum values are less than 255 and more than 0 respecitvely


// Determines the current load state
// previousState is used so that the reset limits can provide hysteresis
int getLoadState(int load, int previousState) {

    // invalid values mean that the sensor itself has malfunctioned
    if (!validSensorValue(load))
        return SENSOR_ERROR;


    // if the sensor has recovered from an error,
    // start evaluating it again from the normal state
    if (previousState == SENSOR_ERROR)
        previousState = NORMAL;


    // when currently normal, use the warning and fault limits
    if (previousState == NORMAL) {

        if (load >= LOAD_FAULT_LIMIT)
            return FAULT;

        if (load >= LOAD_WARNING_LIMIT)
            return WARNING;

        return NORMAL;
    }


    // when currently in warning, the warning reset limit
    // must be crossed before returning to normal
    if (previousState == WARNING) {

        if (load >= LOAD_FAULT_LIMIT)
            return FAULT;

        if (load < LOAD_WARNING_RESET)
            return NORMAL;

        return WARNING;
    }


    // when already in fault, the value must go below
    // the fault reset limit before leaving the fault state
    if (load < LOAD_FAULT_RESET)
        return WARNING;

    return FAULT;
}


// Determines the current temperature state
// previousState is used so that the reset limits can provide hysteresis
int getTemperatureState(int temperature, int previousState) {

    // invalid values mean that the sensor itself has malfunctioned
    if (!validSensorValue(temperature))
        return SENSOR_ERROR;


    // if the sensor has recovered from an error,
    // start evaluating it again from the normal state
    if (previousState == SENSOR_ERROR)
        previousState = NORMAL;


    // when currently normal, use the warning and fault limits
    if (previousState == NORMAL) {

        if (temperature >= TEMP_FAULT_LIMIT)
            return FAULT;

        if (temperature >= TEMP_WARNING_LIMIT)
            return WARNING;

        return NORMAL;
    }


    // when currently in warning, the warning reset limit
    // must be crossed before returning to normal
    if (previousState == WARNING) {

        if (temperature >= TEMP_FAULT_LIMIT)
            return FAULT;

        if (temperature < TEMP_WARNING_RESET)
            return NORMAL;

        return WARNING;
    }


    // when already in fault, the value must go below
    // the fault reset limit before leaving the fault state
    if (temperature < TEMP_FAULT_RESET)
        return WARNING;

    return FAULT;
}


// Determines the overall state of the machine
// the most severe condition always takes priority
int determineMachineState() {

    // sensor failure takes priority because the system
    // cannot reliably determine the actual machine condition
    if (loadState == SENSOR_ERROR ||
        tempState == SENSOR_ERROR)

        return SENSOR_ERROR;


    // any actual sensor fault means the machine is in fault
    if (loadState == FAULT ||
        tempState == FAULT)

        return FAULT;


    // if there is no fault, a warning is still shown
    // when either sensor is in its warning range
    if (loadState == WARNING ||
        tempState == WARNING)

        return WARNING;


    return NORMAL;
}


// Determines what caused the machine to enter a fault
int determineFaultCause() {

    // sensor failure is treated separately from an actual
    // load or temperature fault
    if (loadState == SENSOR_ERROR ||
        tempState == SENSOR_ERROR)

        return SENSOR_FAILURE;


    // load fault takes priority if both sensors are faulty
    if (loadState == FAULT)
        return LOAD_OVERLOAD;


    // otherwise the fault was caused by temperature
    if (tempState == FAULT)
        return OVER_TEMPERATURE;


    return NO_FAULT;
}


// Updates the machine state and keeps faults latched
// once a fault has occurred, it stays active until reset
void updateMachineState(int requestedState) {

    // both an actual fault and a sensor error are latched
    // so that the operator has to acknowledge the condition
    if (requestedState == FAULT ||
        requestedState == SENSOR_ERROR) {

        faultLatched = 1;

        machineState = requestedState;

        faultCause = determineFaultCause();

        return;
    }


    // if a previous fault is still latched,
    // don't allow normal sensor readings to clear it
    if (faultLatched == 1) {

        machineState = FAULT;

        return;
    }


    machineState = requestedState;
}


// Checks whether the reset button has been pressed
// reset clears the latched fault and returns the machine to normal
void checkReset() {

    if (digitalRead(resetButton) == HIGH) {

        faultLatched = 0;

        faultCause = NO_FAULT;

        machineState = NORMAL;
    }
}


// Determines the operating mode from the two DIP switches
// INPUT_PULLUP means:
// DIP OFF = HIGH
// DIP ON  = LOW
int getOperatingMode() {

    int forceOff = digitalRead(forceOffPin);
    int forceOn  = digitalRead(forceOnPin);


    // both switches OFF means normal operation
    // sensors control the machine normally
    if (forceOff == HIGH &&
        forceOn == HIGH)

        return MODE_NORMAL;


    // force ON overrides the sensors
    // motor/correction system stays active regardless of sensor values
    if (forceOff == HIGH &&
        forceOn == LOW)

        return MODE_FORCE_ON;


    // force OFF overrides the sensors
    // motor/correction system stays inactive regardless of sensor values
    if (forceOff == LOW &&
        forceOn == HIGH)

        return MODE_FORCE_OFF;


    // both switches cannot be active at the same time
    // so this combination is treated as invalid
    return MODE_INVALID;
}


// Controls the three LEDs according to the machine state
// LEDs are fundamental indicators and are not disabled by DIP overrides
void displayState(int state) {

    // green means the machine is operating normally
    if (state == NORMAL) {

        digitalWrite(greenLED, HIGH);
        digitalWrite(yellowLED, LOW);
        digitalWrite(redLED, LOW);
    }


    // yellow means the machine is operating in warning
    else if (state == WARNING) {

        digitalWrite(greenLED, LOW);
        digitalWrite(yellowLED, HIGH);
        digitalWrite(redLED, LOW);
    }


    // red means either a fault or sensor error has occurred
    else {

        digitalWrite(greenLED, LOW);
        digitalWrite(yellowLED, LOW);
        digitalWrite(redLED, HIGH);
    }
}


// Controls the motor/fault correction system
// the motor activates only for an actual machine fault
void controlMotor(int state, int mode) {

    // force ON
    // the motor stays active regardless of sensor values
    if (mode == MODE_FORCE_ON) {

        digitalWrite(motorPin, HIGH);

        return;
    }


    // force OFF
    // the motor stays inactive regardless of sensor values
    if (mode == MODE_FORCE_OFF) {

        digitalWrite(motorPin, LOW);

        return;
    }


    // invalid DIP combination
    // safest behavior is to keep the correction system OFF
    if (mode == MODE_INVALID) {

        digitalWrite(motorPin, LOW);

        return;
    }


    // normal operation
    // motor activates only when an actual load or temperature fault exists
    // SENSOR_ERROR does not activate the motor
    if (loadState == FAULT ||
        tempState == FAULT) {

        digitalWrite(motorPin, HIGH);
    }

    else {

        digitalWrite(motorPin, LOW);
    }
}


// Controls the buzzer according to machine state and DIP overrides
// any DIP override disables the buzzer
void controlBuzzer(int state, int mode) {

    // any DIP override disables the buzzer
    if (mode != MODE_NORMAL) {

        digitalWrite(buzzerPin, LOW);

        return;
    }


    // no warning or fault means no buzzer
    if (state == NORMAL) {

        digitalWrite(buzzerPin, LOW);

        return;
    }


    // warning uses an intermittent buzzer
    // the buzzer changes state every 300 ms
    if (state == WARNING) {

        if ((millis() / 300) % 2 == 0)
            digitalWrite(buzzerPin, HIGH);

        else
            digitalWrite(buzzerPin, LOW);

        return;
    }


    // fault or sensor error keeps the buzzer continuously active
    digitalWrite(buzzerPin, HIGH);
}


// Prints the current machine state to the serial monitor
void printState(int state) {

    if (state == NORMAL)
        Serial.print("NORMAL");

    else if (state == WARNING)
        Serial.print("WARNING");

    else if (state == FAULT)
        Serial.print("FAULT");

    else
        Serial.print("SENSOR ERROR");
}


// Prints the current DIP operating mode to the serial monitor
void printOperatingMode(int mode) {

    if (mode == MODE_NORMAL)
        Serial.print("NORMAL");

    else if (mode == MODE_FORCE_ON)
        Serial.print("FORCE ON");

    else if (mode == MODE_FORCE_OFF)
        Serial.print("FORCE OFF");

    else
        Serial.print("INVALID");
}


// Prints the cause of the current fault to the serial monitor
void printFaultCause(int cause) {

    if (cause == NO_FAULT)
        Serial.print("NONE");

    else if (cause == LOAD_OVERLOAD)
        Serial.print("LOAD OVERLOAD");

    else if (cause == OVER_TEMPERATURE)
        Serial.print("OVER TEMPERATURE");

    else
        Serial.print("SENSOR FAILURE");
}


// Sets up all inputs and outputs
void setup() {

    pinMode(greenLED, OUTPUT);
    pinMode(yellowLED, OUTPUT);
    pinMode(redLED, OUTPUT);

    // DIP switches use the internal pull-up resistors
    pinMode(forceOffPin, INPUT_PULLUP);
    pinMode(forceOnPin, INPUT_PULLUP);

    pinMode(resetButton, INPUT);

    pinMode(motorPin, OUTPUT);

    pinMode(buzzerPin, OUTPUT);

    // start the motor and buzzer in the OFF state
    digitalWrite(motorPin, LOW);
    digitalWrite(buzzerPin, LOW);

    Serial.begin(9600);
}


// Main program loop
void loop() {

    unsigned long currentTime = millis();


    // sensors are sampled at the defined sample interval
    if (currentTime - lastSampleTime >= SAMPLE_INTERVAL) {

        lastSampleTime = currentTime;


        // read and filter the load sensor
        int rawLoad =
            readFilteredSensor(loadPin);


        // read and filter the temperature sensor
        int rawTemperature =
            readFilteredSensor(tempPin);


        // convert the raw analog readings into 0-255 values
        loadValue =
            (int)calibrateLoad(rawLoad);

        temperatureValue =
            (int)calibrateTemperature(rawTemperature);


        // determine the current state of each sensor
        loadState =
            getLoadState(loadValue, loadState);

        tempState =
            getTemperatureState(
                temperatureValue,
                tempState
            );


        // combine both sensor states into one machine state
        int requestedState =
            determineMachineState();


        // update and latch the machine state
        updateMachineState(requestedState);
    }

    // check for a reset before controlling the outputs
    checkReset();



    // read the DIP switches and determine the current mode
    int operatingMode =
        getOperatingMode();



    // LEDs always show the actual machine state
    displayState(machineState);


    // motor follows the fault correction rules and DIP overrides
    controlMotor(
        machineState,
        operatingMode
    );


    // buzzer follows the warning/fault rules and DIP overrides
    controlBuzzer(
        machineState,
        operatingMode
    );


    // periodically print the complete system status
    if (currentTime - lastReportTime >= REPORT_INTERVAL) {

        lastReportTime = currentTime;


        Serial.print("Load: ");
        Serial.print(loadValue);

        Serial.print(" | Temperature: ");
        Serial.print(temperatureValue);

        Serial.print(" | State: ");
        printState(machineState);

        Serial.print(" | Mode: ");
        printOperatingMode(operatingMode);

        Serial.print(" | Motor: ");


        // force ON always keeps the motor active
        if (operatingMode == MODE_FORCE_ON) {

            Serial.print("ON");
        }


        // force OFF and invalid modes keep the motor inactive
        else if (operatingMode == MODE_FORCE_OFF ||
                 operatingMode == MODE_INVALID) {

            Serial.print("OFF");
        }


        // in normal mode, the motor is active only
        // when an actual sensor fault is present
        else if (loadState == FAULT ||
                 tempState == FAULT) {

            Serial.print("ON");
        }

        else {

            Serial.print("OFF");
        }


        Serial.print(" | Buzzer: ");


        // any DIP override disables the buzzer
        if (operatingMode != MODE_NORMAL) {

            Serial.print("OFF");
        }


        // warning produces an intermittent buzzer
        else if (machineState == WARNING) {

            Serial.print("INTERMITTENT");
        }


        // actual fault or sensor error produces a continuous buzzer
        else if (machineState == FAULT ||
                 machineState == SENSOR_ERROR) {

            Serial.print("ON");
        }

        else {

            Serial.print("OFF");
        }


        Serial.print(" | Latched: ");
        Serial.print(faultLatched);

        Serial.print(" | Cause: ");
        printFaultCause(faultCause);

        Serial.println();
    }
}