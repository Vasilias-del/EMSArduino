#include <NTPtimeESP.h>
#include <ESP8266WiFi.h>
#include <FirebaseArduino.h>
#include <Wire.h>
#include "I2Cdev.h"
#include "MPU6050.h"
#include "Math.h"
#include <ezTime.h>
#include "MAX30100_PulseOximeter.h"

#define REPORTING_PERIOD_MS     1000
PulseOximeter pox;

uint32_t tsLastReport = 0;

#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include "Wire.h"
#endif

// Firebase Credentials
#define FIREBASE_HOST "elderlymonitoringsystem-2f6b7.firebaseio.com"
#define FIREBASE_AUTH "your_firebase_auth"

// WIFI Credentials
const char* ssid = "neyl_2.4G";
const char* password= "pneyl060883";
Timezone Malaysia;
String date; String dTime;
String dmonth;

// LM35
float temp, tempf;
const float pinA0 = A0;

float bpm2;
float bpm [] = {80.0731, 70.4623, 75.7876, 85.9809, 84.9555, 86.1212, 89.1787, 90.5631, 91.7321, 99.2979, 97.0108, 95.3108, 98.9139, 93.0879, 100.1777, 110.4313, 105.3329, 120.1113, 99.0824, 98.1056, 115.2478, 122.5797, 96.8999, 80.9145, 85.1222, 110.8711, 112.0989, 90.5588, 89.7277, 93.6998, 124.5114, 130.2222, 87.9914, 88.1256, 90.6752, 97.0001, 104.7001, 105.8654, 108.9807, 80.1432, 79.5478, 75.9842, 60.6709, 66.2846, 63.5874, 80.5842, 91.2364, 90.8423, 84.8741, 88.2587, 84.8234, 85.6547, 76.0231, 77.0077, 79.9645, 78.0002, 90.0054, 92.7894, 94.9845, 100.3645};
int i = 0;

// MPU6050
MPU6050 accelgyro;
int16_t ax, ay, az;
int16_t gx, gy, gz;
int agx, agy, agz;
#define OUTPUT_READABLE_ACCELGYRO

void onBeatDetected()
{
//    Serial.println("Beat!");
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  // Establishing WIFIF Connection
  Serial.println();
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  Serial.println();
  Serial.print("Connecting");

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  //  Time
  waitForSync();
  Malaysia.setLocation("Asia/Kuala_Lumpur");
  //  Serial.println("UTC: " + UTC.dateTime());
  //  Timezone Malaysia;
  //  Serial.println("Malaysia time: " + Malaysia.dateTime());
  
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println();

  Serial.println("Wifi Connected Successfully!");
  Serial.print("WeMos IP Address : ");
  Serial.println(WiFi.localIP());
  
  // Establishing Firebase Connection
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  // join I2C bus (I2Cdev library doesn't do this automatically)
  #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
      Wire.begin();
  #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
      Fastwire::setup(400, true);
  #endif

  // initialize device
  Serial.println("Initializing I2C devices...");
  accelgyro.initialize();

  // verify connection
  Serial.println("Testing device connections...");
  Serial.println(accelgyro.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");

  // Initialize the PulseOximeter instance and register a beat-detected callback
  pox.begin();
  pox.setOnBeatDetectedCallback(onBeatDetected);
}

void loop() {
  // put your main code here, to run repeatedly:

//  if(i==59) {
//    i = 0;
//  }
  // Temp Calibration
  temp = analogRead(pinA0); 
  temp = (temp/1023)*325;
  tempf = temp*1.8+32;
  

  // Accelerometer 
  accelgyro.getAcceleration(&ax, &ay, &az);
  agx = ax / 16384; agy = ay / 16384; agz = az / 16384;
  double g = sqrt(agx * agx + agy * agy + agz * agz);
  String dateTime(Malaysia.dateTime("d-m-y"));

  pox.update();
  
  String timestamp = String(now());
  
  date = dateTime;

  if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
      Serial.print("Heart rate:");
      Serial.print(pox.getHeartRate());
      Serial.print("bpm ");
      Serial.print(" Celcius:");
      Serial.print(temp);
      Serial.print("°C");
      Serial.print(" Fahrenheit:");
      Serial.print(tempf);
      Serial.println("°F");
      
      #ifdef OUTPUT_READABLE_ACCELGYRO
          // display tab-separated accel/gyro x/y/z values
          Serial.print("a/g:\t");
          Serial.print(agx); Serial.print("\t");
          Serial.print(agy); Serial.print("\t");
          Serial.print(agz+1); Serial.print("\t");
          Serial.println(g+1);

          if(g>0.99) {
             Serial.println("Fall Detected!");
             String fallDate = String(Malaysia.dateTime("d-m-y H:i:s"));
             Serial.println("Last Detected Fall: "+ fallDate);
             
             Firebase.setInt("sensors/data/g", 2);
             Firebase.setString("sensors/data/fall", "Last Detected Fall: "+ fallDate);
    
             ///////////////// Notification Segment
             Firebase.setInt("notification/"+timestamp+"/timestamp",(timestamp.toInt()*-1));
             Firebase.setString("notification/"+timestamp+"/msg", "Fall Detected!");
             Firebase.setString("notification/"+timestamp+"/datetime", String(Malaysia.dateTime("d-m-y H:i:s")));
             Firebase.setString("trigger/notification", timestamp);
          }
      #endif
      
      // Sending data to report
      if(temp<=37) {
        Firebase.setString("sensors/data/condition", "Healthy");
      } else {
        Firebase.setString("sensors/data/condition", "Unhealthy");
        if (temp<45){
          ///////////////// Report Segment
          bpm2 = pox.getHeartRate();
          Firebase.setFloat("report/"+String(Malaysia.dateTime("d-m-y"))+"/bpm", bpm2);
          Firebase.setString("report/"+String(Malaysia.dateTime("d-m-y"))+"/condition", "Unhealthy");
          Firebase.setString("report/"+String(Malaysia.dateTime("d-m-y"))+"/datetime", String(Malaysia.dateTime("d-m-y H:i:s")));
          Firebase.setFloat("report/"+String(Malaysia.dateTime("d-m-y"))+"/temperature", temp);
          Firebase.setInt("report/"+String(Malaysia.dateTime("d-m-y"))+"/timestamp", (timestamp.toInt()*-1));
         ///////////////// Report Segment
       }
      }
      
      ////////////////// sending data to sensors/data
      bpm2 = pox.getHeartRate();
      Firebase.setFloat("report/"+String(Malaysia.dateTime("d-m-y"))+"/bpm", bpm2);
      Firebase.setString("sensors/data/date", String(Malaysia.dateTime("d-m-y H:i:s")));
      Firebase.setInt("sensors/data/g", (int)g);
      Firebase.setFloat("sensors/data/temperature", round(temp*100.00)/100.00);
      Firebase.setInt("sensors/data/x", agx);
      Firebase.setInt("sensors/data/y", agy);
      Firebase.setInt("sensors/data/z", agz);
      ////////////////// sending data to sensors/data

      tsLastReport = millis();
    }
//      i++;
//      delay(1000);
}
