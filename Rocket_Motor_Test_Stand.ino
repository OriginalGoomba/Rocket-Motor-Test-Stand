/*
  SD card attached to SPI bus as follows:
** MOSI - pin 11 (51 MEGA) on Arduino Uno/Duemilanove/Diecimila
** MISO - pin 12 (50 MEGA) on Arduino Uno/Duemilanove/Diecimila
** CLK - pin 13 (52 MEGA) on Arduino Uno/Duemilanove/Diecimila
** CS - (53 MEGA) depends on your SD card shield or module.
**
** This link might be useful: http://lab.dejaworks.com/arduino-nano-sd-card-connection/
*/

#include <SPI.h>
#include <SD.h>
#include "HX711.h"

//scale
HX711 scale(2, 3);                                                                  //Starts the HX711 Scale with DT on pin 2, and SCK on pin 3

//LED and Button pins (Launch Sequence)
const int buttonPin = 5;                                                            //The number of the pushbutton pin
const int redPin = 7;                                                               //The number of the RGB pin (RED)
const int greenPin =  8;                                                            //The number of the RGB pin (GREEN)
const int bluePin = 9;                                                              //The number of the RGB pin (BLUE)
const int relayPin = 6;
int buttonState = 0;                                                                //Variable for reading the pushbutton status

//Scale
float calibration_factor = -247;                                                    //This calibration factor is adjusted according to my load cell (10kg)
float units;                                                                        //The basic unit for the scale, grams. Easily converted, however.
const int testInterval = 400;                                                       //Arbitrary value for now I suppose (100 = 6.05 sec)

//SD Card
const int chipSelect = 4;                                                           //4 for UNO and 53 for MEGA
char fileName[] = "Test_00.CSV";                                                    //Base filename for logging.

//Timing
const int countdownTime = 5;                                                        //Amount of time in seconds the countdown lasts.
extern volatile unsigned long timer0_millis;                                        //This variable is used to reset the internal clock on the arduino for every launch.
double time;                                                                        //Used to count the time in lilliseconds for logging on SD card. double works fine in this application. Runs are short.

void setup() {
  Serial.begin(9600);
  pinMode(greenPin, OUTPUT);
  pinMode(redPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  pinMode(buttonPin, INPUT);
  pinMode(relayPin, OUTPUT);
  //Scale Setup
  scale.set_scale();                                                                //Sets the scale
  scale.tare();                                                                     //Resets the scale to 0

  long zero_factor = scale.read_average();                                          //Gets a baseline reading
  Serial.print("Zero factor: ");                                                    //This can be used to remove the need to tare the scale. Useful in permanent scale projects.
  Serial.println(zero_factor);
  Serial.print("Test interval: ");                                                  //Displays the variable test interval for testing.
  Serial.println(testInterval);
  scale.set_scale(calibration_factor);                                              //Adjust to this calibration factor (This is not really necessary but may be useful in your own set up)

  //SD Setup
  while (!Serial) {
    ;                                                                               //Waits for serial port to connect. Needed for native USB port only, while plugged in for testing
  }
  
  Serial.println("Initializing SD card...");
  if (!SD.begin(chipSelect)) {                                                      //See if the card is present and can be initialized:
    Serial.println("Card failed, or not present");
    while (!SD.begin(chipSelect)) {
      Serial.println("Double check SD Card connection!");
      //throws error LED
      setColor(255, 0, 205);
      delay(500);
      setColor(0, 0, 0);
    }
  }
  Serial.println("Card initialized.\n");
  delay(1000);
  Serial.println("Waiting for launch.\n");
}

void setColor(int red, int green, int blue) {                                       //RGB LED function for ease of use
#ifdef COMMON_ANODE
  red = 255 - red;
  green = 255 - green;
  blue = 255 - blue;
#endif
  analogWrite(redPin, red);
  analogWrite(greenPin, green);
  analogWrite(bluePin, blue);
}

char fileGenerator(char fileName[]) {                                               //Generates a new file string for every press of the launch button
  Serial.println("Checking if a test log already exists...");
  if (SD.exists(fileName)) {                                                        //Check before modifying target filename.
    Serial.println("Creating a new test log...");
    for (int i = 1; i <= 99; i++) {                                                 //The filename exists so this increments the 2 digit filename index.
      fileName[5] = i / 10 + '0';
      fileName[6] = ceil(i % 10 + '0');
      if (!SD.exists(fileName)) {                                                   //Honestly not really sure if this does anything
        break;
      }
    }
  }
  if (!SD.exists(fileName)) {
    Serial.println("File name is unique, keeping original file name...");
  }
  return fileName;
}

void launchSeq() {
  Serial.println("LAUNCH SEQUENCE ACTIVATED");
  for (int i = 0; i < countdownTime; i++) {                                         //Counts down 1 second for the value held by countdownTimer
    setColor(255, 0, 0);
    delay(500);
    setColor(0, 0, 0);
    delay(500);
  }
  scale.tare();                                                                     //Tares the scale right before motor firing for accuracy
}

void thrustLog() {                                                                  //This Logs the time as well as the strain gauge value
  File dataFile = SD.open(fileName, FILE_WRITE);
  noInterrupts ();                                                                  //
  timer0_millis = 0;                                                                //Resets the inner clock on the arduino for easy excel processing (above)
  interrupts ();                                                                    //
  if (dataFile) {
    for (int i = 0; i < testInterval; i++) {
      units = scale.get_units(), 10;
      time = millis() / 1000.0;                                                     //Converts time to seconds
      if (units < 0)  {                                                             //If the gauge reads a negative value, it is displayed as zero instead
        units = 0.00;
      }
      dataFile.print(time);
      dataFile.print(",");
      dataFile.println(units);
      Serial.print("Printing time and load cell values to SD Card: ");
      Serial.print(time);
      Serial.print(", ");
      Serial.println(units);
    }
  } else {
    Serial.println("error opening test file. Test results unable to be logged!");
    //throws error LED
    setColor(255, 0, 205);
    delay(500);
    setColor(0, 0, 0);
  }
  Serial.println("Test file logging Finished.");
  dataFile.close();
}


void electricFuse() {                                                               //Activates the rocket motor fuse and calls the thrustLog() function
  setColor(0, 0, 255);
  fileGenerator(fileName);                                                          //As soon as the electric fuse ignites, the logger starts recording
  digitalWrite(relayPin, HIGH);
  //switch on relay/transistor for electric fuse.
  //delay(1000);                                                                      //Two second delay so the fuse has time to ignite motor
  //switch off relay
  
  thrustLog(); //TODO: might need to switch on and off relay inside the thrust log... Might be best to call this inside the thrust log function.
}

void loop() {                                                                        
  setColor(0, 255, 0);
  buttonState = digitalRead(buttonPin);                                            
  if (SD.begin(chipSelect) == true) {                                               //SD Card must be inserted prior to allowing a launch.
    if (buttonState == HIGH)  {                                                     //If an SD Card is detected as well as a button press, the launch sequence will commence
      launchSeq();
      digitalWrite(greenPin, LOW);
      electricFuse();
      delay(1000); // reset timer
      digitalWrite(relayPin, LOW);
      Serial.println("Ready to launch again.");
    }
  } else {                                                                          //If no SD Card is detected an error indicator LED will flash until the SD Card is initialized.
    while (!SD.begin(chipSelect)) {
      Serial.println("Double check SD Card connection!");
      //throw error LED
      setColor(255, 0, 205);
      delay(500);
      setColor(0, 0, 0);
    }
    Serial.println("SD Card connection reestablished!\nWaiting for launch.");
  }
}
