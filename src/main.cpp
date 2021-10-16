#include <Arduino.h>
#include <Wire.h>

const int buzz = 9;
const int rled = 4;
const int gled = 5;
const int btn = 7;
int prevState = HIGH;

const int MPU_addr = 0x68; // I2C address of the MPU-6050
int16_t AcX, AcY, AcZ, Tmp, WyX, WyY, WyZ;
float ax = 0, ay = 0, az = 0, wx = 0, wy = 0, wz = 0;
boolean fall = false;     //stores if a fall has occurred
boolean trigger1 = false; //stores if first trigger (lower threshold) has occurred
boolean trigger2 = false; //stores if second trigger (upper threshold) has occurred
boolean trigger3 = false; //stores if third trigger (orientation change) has occurred
byte trigger1count = 0;   //stores the counts past since trigger 1 was set true
byte trigger2count = 0;   //stores the counts past since trigger 2 was set true
byte trigger3count = 0;   //stores the counts past since trigger 3 was set true
int angleChange = 0;

// Function Templates;
void mpu_read();
void forceStart();
void fall_mode();
bool btnMode();

void setup()
{
    Serial.begin(9600);
    Wire.begin();
    Wire.beginTransmission(MPU_addr);
    Wire.write(0x6B); // PWR_MGMT_1 register
    Wire.write(0);    // set to zero (wakes up the MPU-6050)
    Wire.endTransmission(true);

    pinMode(rled, OUTPUT);
    pinMode(gled, OUTPUT);
    pinMode(btn, INPUT_PULLUP);
}

void loop()
{
    forceStart();
    mpu_read();
    ax = (AcX - 2050) / 16384.00;
    ay = (AcY - 77) / 16384.00;
    az = (AcZ - 1947) / 16384.00;
    wx = (WyX + 270) / 131.07;
    wy = (WyY - 351) / 131.07;
    wz = (WyZ + 136) / 131.07;

    // Calculating the magnitude of the acceleration on the 3 axis
    float Raw_Amp = pow(pow(ax, 2) + pow(ay, 2) + pow(az, 2), 0.5);     // Raw Values from the accelerometer
    int Amp = Raw_Amp * 10; // Mulitiplied by 10 as values are between 0 to 1, enables a controlled more distinguish value
    
    if (Amp <= 2 && trigger2 == false)
    { 
        //if AM breaks lower threshold (0.4g)
        trigger1 = true;
    }

    if (trigger1 == true)
    {
        trigger1count++;
        if (Amp >= 12)
        { 
            //if AM breaks upper threshold (3g)
            trigger2 = true;
            trigger1 = false;
            trigger1count = 0;
        }
    }
    if (trigger2 == true)
    {
        trigger2count++;
        angleChange = pow(pow(wx, 2) + pow(wy, 2) + pow(wz, 2), 0.5);
        // Serial.println(angleChange);
        if (angleChange >= 30 && angleChange <= 400)
        { 
            //if orientation changes by between 80-100 degrees
            trigger3 = true;
            trigger2 = false;
            trigger2count = 0;
            // Serial.println(angleChange);
        }
    }
    if (trigger3 == true)
    {
        trigger3count++;
        if (trigger3count >= 10)
        {
            // Calculates the angular velocity i.e. change in angle after the UFT has been triggered
            angleChange = pow(pow(wx, 2) + pow(wy, 2) + pow(wz, 2), 0.5);
            delay(10);
            // Serial.println(angleChange);
            if ((angleChange >= 0) && (angleChange <= 10))
            { 
                // if orientation changes remains between 0-10 degrees
                fall = true;
                trigger3 = false;
                trigger3count = 0;
            }
            else
            { 
                // user regained normal orientation
                // Non-Fatal Fall as the user is able to move again
                
                trigger3 = false;
                trigger3count = 0;

                Serial.println("non-fatal fall\n"); // Non-Fatal Fall Data send to app
                fall_mode();
            }
        }
    }
    if (fall == true)
    { 
        // A Hard Fall is Detected
        Serial.println("Fatal Fall\n"); // Fatal Fall Data send to app
        fall_mode();
        fall = false;
    }
    if (trigger2count >= 6)
    { 
        //allow 0.5s for orientation change
        trigger2 = false;
        trigger2count = 0;
    }
    if (trigger1count >= 6)
    { 
        //allow 0.5s for AM to break upper threshold
        trigger1 = false;
        trigger1count = 0;
    }
    delay(100);
}

void mpu_read()
{
    Wire.beginTransmission(MPU_addr);
    Wire.write(0x3B); // starting with register 0x3B (ACCEL_XOUT_H)
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_addr, 14, true); // request a total of 14 registers
    AcX = Wire.read() << 8 | Wire.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
    AcY = Wire.read() << 8 | Wire.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
    AcZ = Wire.read() << 8 | Wire.read(); // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
    Tmp = Wire.read() << 8 | Wire.read(); // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
    WyX = Wire.read() << 8 | Wire.read(); // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
    WyY = Wire.read() << 8 | Wire.read(); // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
    WyZ = Wire.read() << 8 | Wire.read(); // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)
}

void forceStart(){
    if(btnMode()){
        // Manual Alert System On
        Serial.println("Fatal Fall\n"); // Default to a Fatal Fall Type - Data send to app
        fall_mode();
    }
}

void fall_mode()
{
    bool yes = true;
    while (yes)
    {
        if (btnMode()) // Manual Alert System Off
        {
            yes = false;

            // Sends Normal Mode to the app
            Serial.println("Normal\n");

            digitalWrite(rled, LOW);    // Turn Off red Led

            for(int i = 0; i < 2; i++){
                // Flash green for safety on
                digitalWrite(gled, HIGH);
                delay(100);
                digitalWrite(gled, LOW);
            }
        }

        // Alert System 
        tone(buzz, 1000);

        // Red Light On
        digitalWrite(rled, HIGH);
        delay(200);
        digitalWrite(rled, LOW);
        delay(200);
    }

    // Turn Alarm Buzzer Off
    noTone(buzz);
}

bool btnMode()
{
    int btnState = digitalRead(btn);
    if (btnState == HIGH && prevState == LOW)
    {
        prevState = btnState;
        return true;
    }
    prevState = btnState;
    return false;
}