#include <stdio.h>
#include <string.h>
#include "Wire.h"
#include <MPU6050_light.h>
#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>

#define LED_i 14     //interrupting led
#define LED_f 12     //falling led

hw_timer_t *My_timer = NULL;
TinyGPSPlus gps;
MPU6050 mpu(Wire);

static const int RXPin = 18, TXPin = 19;      //configure two pins, RX and TX, for UART1 connection with the GPS module
static const uint32_t GPSBaud = 9600;

unsigned long timer = 0;

volatile float Y_new =0 ,X_val = 0;       //Use the 'volatile' keyword when a variable is receiving values 
                                          //from peripherals, such as sensors, GPS, etc
volatile float lat,lng;
String latitude;
String longtitude;

bool isReply = false;   

const char phone[] = "0984324077";  //replace for your mobile phone number
String substring = "gimmeLoc";           //Send 'gimmeLoc', and it will reply with the location 
String message;

void IRAM_ATTR onTimer(){             //Timer task, to be executed every 30 seconds as configured
  digitalWrite(LED_i, !digitalRead(LED_i));     //Change the LED state and check the falling condition every 30 seconds
  isReply = check_angleY();     
}

void setup() {
  Serial.begin(115200);    //for debugging

  Wire.begin();          //start I2C
  byte status = mpu.begin();
  Serial.print(F("MPU6050 status: "));
  Serial.println(status);
  while (status != 0) { } // stop everything if could not connect to MPU6050
  Serial.println(F("Calculating offsets, do not move MPU6050"));
  delay(5000);
  mpu.calcOffsets(); // gyro and accelero
  Serial.println("Done!\n");

  pinMode(LED_i, OUTPUT);               //Every time the LED changes, the ESP will check whether it is falling or not
  pinMode(LED_f, OUTPUT);               //turn on if falling

  // //setup gps
  Serial1.begin(GPSBaud,SERIAL_8N1,RXPin,TXPin);

  //setup sim A7680C
  Serial2.begin(115200);
  Serial2.print("AT+CMGD=1,4\r"); //delete all
  delay(1000);

  //setup timer
  My_timer = timerBegin(0, 80, true);
  timerAttachInterrupt(My_timer, &onTimer, true);
  timerAlarmWrite(My_timer, 30000000, true);       //Interrupt every 30 seconds is equivalent to a count time of 30,000,000. 
  timerAlarmEnable(My_timer);
}

void loop() {
  mpu.update();

  if (Serial1.available() > 0)
    if (gps.encode(Serial1.read()))
        if (gps.location.isValid()){
          lat = gps.location.lat();       //read location
          lng = gps.location.lng();
        }

  int index = message.indexOf(substring);
  
  if((millis()-timer)>5000){   // Print data every 1000 milliseconds, equivalent to 1 second
    if (isReply==true){               //is falling
      timerStop(My_timer);            //stop timer interrupt for doing some tasks
      //Serial.println("falling now");
      digitalWrite(LED_f, HIGH);  

      latitude = String(lat,6);         //convert location data to string
      longtitude = String(lng,6);

      send_sms("xe cua ban gap phai su co, hoac dang bi tai nan. Link map: https://www.google.com/maps?q="+ latitude + "," + longtitude);       
      check_sms_responce();     //Send a message and show the response
      isReply = false;                //Set 'isReply' to false to send only one message => saving resources :))

      timerStart(My_timer);           //Restart the timer once all tasks are completed
    } else {                        
      digitalWrite(LED_f, LOW);        //is not falling

      X_val = abs(mpu.getAngleX());    //read angle data
      Y_new = abs(mpu.getAngleY());    //Use abs() for easy calculation

      displayInfo();

      read_sms();
      if (index != -1) replyLocation();                  
    }
   timer = millis();   //search "millis()"" for more details
  }
} 

bool check_angleY(){        
  if ( (Y_new > 17 && X_val < 90) || (Y_new > 70 && X_val > 90) ){ 
    return 1;
  } else return 0;
}

void send_sms(String sms){
  Serial2.print("AT+CMGF=1\r");  //Set text mode
  delay(1000);
  Serial2.print("AT+CMGS=\""+String(phone)+"\"\r"); //Send message
  delay(1000);
  Serial2.print(sms);//Text message
  Serial2.println((char)0x1A); //Ctrl+Z
}

void check_sms_responce(){
   if(Serial2.available()>0)
  {
    delay(60);
    message="";     //Clear the 'message' variable          
    while(Serial2.available())
    {
      message+=(char)Serial2.read();
    }
    Serial.print(message);
  }
}

void displayInfo()  //When using the ESP32 with a battery, you can comment out all the code in this function to improve memory performance
{                   //Remember to comment out the function named 'displayInfo()' in the main loop
  Serial.print("\nY_new = ");     
  Serial.println(Y_new);
  Serial.print(F("Location: ")); 
  Serial.print(lat, 6);
  Serial.print(F(","));
  Serial.print(lng, 6);
  Serial.println();
}

void read_sms(){
  Serial2.print("AT+CMGL=\"REC UNREAD\"\r");  //list unread
  delay(1000);
  check_sms_responce(); 
}

void replyLocation(){
  timerStop(My_timer);
  send_sms("Hey Sir, I found myself on the map! Here's the link: https://www.google.com/maps?q="+ latitude + "," + longtitude);
  delay(1000);
  Serial2.print("AT+CMGD=1,4\r"); //delete all for a one-time response
  delay(1000);
  timerStart(My_timer); 
}
