#include <AccelStepper.h>
#include <MultiStepper.h>
#include <math.h>  // For M_PI and mathematical operations

#define rotInterfaceType AccelStepper::DRIVER
#define inOutInterfaceType AccelStepper::DRIVER

#define stepPin_rot 2
#define dirPin_rot 5
#define stepPin_InOut 3
#define dirPin_InOut 6

#define rot_total_steps 16000.0
#define inOut_total_steps 5760.0

#define BUFFER_SIZE 10  // Maximum number of theta-rho pairs in a batch

// Create stepper motor objects
AccelStepper rotStepper(rotInterfaceType, stepPin_rot, dirPin_rot);
AccelStepper inOutStepper(inOutInterfaceType, stepPin_InOut, dirPin_InOut);

// Create a MultiStepper object
MultiStepper multiStepper;

// Buffer for storing theta-rho pairs
float buffer[BUFFER_SIZE][2];  // Store theta, rho pairs
int bufferCount = 0;   // Number of pairs in the buffer
bool batchComplete = false;

void setup() {
    // Set maximum speed and acceleration
    rotStepper.setMaxSpeed(2000);  // Adjust as needed
    rotStepper.setAcceleration(1);  // Adjust as needed

    inOutStepper.setMaxSpeed(2000);  // Adjust as needed
    inOutStepper.setAcceleration(1);  // Adjust as needed
    // inOutStepper.setPinsInverted(true, false, false);

    // Add steppers to MultiStepper
    multiStepper.addStepper(rotStepper);
    multiStepper.addStepper(inOutStepper);

    // Initialize serial communication
    Serial.begin(115200);
    Serial.println("READY");
    homing();
}

void loop() {
    // Check for incoming serial commands or theta-rho pairs
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');

        // Ignore invalid messages
        if (input != "HOME" && !input.endsWith(";")) {
            Serial.println("IGNORED");
            return;
        }

        if (input == "HOME") {
            homing();
            return;
        }

        // If not a command, assume it's a batch of theta-rho pairs
        if (!batchComplete) {
            int pairIndex = 0;
            int startIdx = 0;

            // Split the batch line into individual theta-rho pairs
            while (pairIndex < BUFFER_SIZE) {
                int endIdx = input.indexOf(";", startIdx);
                if (endIdx == -1) break;  // No more pairs in the line

                String pair = input.substring(startIdx, endIdx);
                int commaIndex = pair.indexOf(',');

                // Parse theta and rho values
                float theta = pair.substring(0, commaIndex).toFloat();  // Theta in radians
                float rho = pair.substring(commaIndex + 1).toFloat();   // Rho (0 to 1)

                buffer[pairIndex][0] = theta;
                buffer[pairIndex][1] = rho;
                pairIndex++;

                startIdx = endIdx + 1; // Move to next pair
            }
            bufferCount = pairIndex;
            batchComplete = true;
        }
    }

    // Process the buffer if a batch is ready
    if (batchComplete && bufferCount > 0) {
        for (int i = 0; i < bufferCount; i++) {
            movePolar(buffer[i][0], buffer[i][1]);
        }
        bufferCount = 0;        // Clear buffer
        batchComplete = false;  // Reset batch flag
        Serial.println("READY");
    }
}

void homing() {
    Serial.println("HOMING");

    // Move inOutStepper inward for homing
    inOutStepper.setSpeed(-5000);  // Adjust speed for homing
    while (true) {
        inOutStepper.runSpeed();
        if (inOutStepper.currentPosition() <= -5760 * 1.1) {  // Adjust distance for homing
            break;
        }
    }
    inOutStepper.setCurrentPosition(0);  // Set home position
    Serial.println("HOMED");
}

void movePolar(float theta, float rho) {
    // Convert polar coordinates to motor steps
    long rotSteps = theta * (rot_total_steps / (2.0 * M_PI));  // Steps for rot axis
    long inOutSteps = rho * inOut_total_steps;                 // Steps for in-out axis

    // Calculate offset for inOut axis
    float revolutions = theta / (2.0 * M_PI);  // Fractional revolutions (can be positive or negative)
    long offsetSteps = revolutions * 1600;    // 1600 steps inward or outward per revolution

    // Apply the offset to the inout axis
    inOutSteps += offsetSteps;

    // Define target positions for both motors
    long targetPositions[2];
    targetPositions[0] = rotSteps;
    targetPositions[1] = inOutSteps;

    // Move both motors synchronously
    multiStepper.moveTo(targetPositions);
    unsigned long lastStepTime = 0;
    const unsigned long stepInterval = 10;  // 10 ms between checks

    multiStepper.runSpeedToPosition();  // Blocking call
}