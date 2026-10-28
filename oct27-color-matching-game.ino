/*
 Color Sequence Game for ESP32 - Group 8
 --------------------------------------------------------
 4 LEDs   : Red, Green, Blue, Yellow
 4 Buttons: one per color
 LCD      : 16x2 I2C (address 0x27 - change to 0x3F if yours doesn't work)


 WIRING (ESP32 DevKit, GPIO numbers)
 LEDs (each through a resistor to GND: ~100-150 ohm for red/yellow/green,
 ~47 ohm for blue):
   Red    -> GPIO 32
   Green  -> GPIO 25
   Blue   -> GPIO 26
   Yellow -> GPIO 33


 Buttons (one leg to the GPIO, other leg to GND - internal pull-ups used,
 no external resistors needed):
   Red    -> GPIO 19
   Green  -> GPIO 16
   Blue   -> GPIO 17
   Yellow -> GPIO 18


 LCD (I2C): GND -> GND, VCC -> 5V (VIN pin when powered by USB),
            SDA -> GPIO 21, SCL -> GPIO 22


 NOTES
 - ESP32 GPIOs are 3.3V ONLY. Never connect 5V or 12V to a GPIO.
 - Most I2C LCD backpacks need 5V for a readable display. If SDA/SCL
   pull-ups on your backpack go to 5V, use a bidirectional level shifter
   (or remove the backpack's pull-up resistors and rely on 3.3V pull-ups).
 - If you use a 3.3V-rated LCD, power it from the 3V3 pin instead.


 LIBRARY: "LiquidCrystal I2C" (Frank de Brabander). If it fails to compile
 on ESP32, install the ESP32-compatible fork "LiquidCrystal_I2C" by
 johnrickman or "hd44780" by Bill Perry.
*/


#include <Wire.h>
#include <LiquidCrystal_I2C.h>


#define SDA_PIN 21
#define SCL_PIN 22


LiquidCrystal_I2C lcd(0x27, 16, 2);


const byte ledPins[4] = {32, 33, 25, 26};   // Red, Yellow, Green, Blue
const byte btnPins[4] = {19, 17, 16, 18}; //R, Y , G , B


const byte MAX_LEVEL = 50;
byte sequence[MAX_LEVEL];


byte level = 0;
unsigned int score = 0;
unsigned int highScore = 0;


const unsigned long INPUT_TIMEOUT = 5000;  // ms the player has per press


// ---------- helpers ----------


void showStatus() {
 lcd.clear();
 lcd.setCursor(0, 0);
 lcd.print("Level: ");
 lcd.print(level);
 lcd.setCursor(0, 1);
 lcd.print("Points: ");
 lcd.print(score);
}


void allLeds(bool state) {
 for (byte i = 0; i < 4; i++) digitalWrite(ledPins[i], state);
}


void flashLed(byte i, unsigned int onTime) {
 digitalWrite(ledPins[i], HIGH);
 delay(onTime);
 digitalWrite(ledPins[i], LOW);
 delay(150);
}


void playSequence() {
 // Gets faster as the level goes up
 int onTime = 600 - ((int)level * 20);
 if (onTime < 150) onTime = 150;
 delay(500);
 for (byte i = 0; i < level; i++) {
   flashLed(sequence[i], onTime);
 }
}


// Returns pressed button index (0-3), or -1 on timeout
int waitForPress() {
 unsigned long start = millis();
 while (millis() - start < INPUT_TIMEOUT) {
   for (byte i = 0; i < 4; i++) {
     if (digitalRead(btnPins[i]) == LOW) {
       delay(20);                              // debounce
       if (digitalRead(btnPins[i]) == LOW) {
         digitalWrite(ledPins[i], HIGH);       // light LED while held
         while (digitalRead(btnPins[i]) == LOW) { delay(1); }
         digitalWrite(ledPins[i], LOW);
         delay(20);
         return i;
       }
     }
   }
   delay(1);   // lets the ESP32 watchdog/background tasks run
 }
 return -1;
}


void successAnimation() {
 for (byte i = 0; i < 4; i++) {
   digitalWrite(ledPins[i], HIGH);
   delay(80);
 }
 delay(200);
 allLeds(LOW);
}


void gameOver(bool won) {
 for (byte n = 0; n < 3; n++) {
   allLeds(HIGH);
   delay(250);
   allLeds(LOW);
   delay(250);
 }
 if (score > highScore) highScore = score;


 lcd.clear();
 lcd.setCursor(0, 0);
 lcd.print(won ? "YOU WIN!" : "GAME OVER");
 lcd.setCursor(0, 1);
 lcd.print("Score:");
 lcd.print(score);
 lcd.print(" Hi:");
 lcd.print(highScore);
 delay(3500);
}


void waitToStart() {
 lcd.clear();
 lcd.setCursor(0, 0);
 lcd.print("Color Sequence");
 lcd.setCursor(0, 1);
 lcd.print("Press any button");


 while (true) {
   for (byte i = 0; i < 4; i++) {
     if (digitalRead(btnPins[i]) == LOW) {
       randomSeed(esp_random());               // ESP32 hardware RNG
       while (digitalRead(btnPins[i]) == LOW) { delay(1); }
       delay(50);
       return;
     }
   }
   delay(1);
 }
}


// ---------- main ----------


void setup() {
 for (byte i = 0; i < 4; i++) {
   pinMode(ledPins[i], OUTPUT);
   pinMode(btnPins[i], INPUT_PULLUP);
 }
 Wire.begin(SDA_PIN, SCL_PIN);
 lcd.init();
 lcd.backlight();
}


void loop() {
 waitToStart();
 level = 0;
 score = 0;
 bool playing = true;
 bool won = false;


 while (playing) {
   level++;
   sequence[level - 1] = random(0, 4);


   showStatus();
   playSequence();


   lcd.setCursor(0, 1);
   lcd.print("Your turn!      ");


   for (byte i = 0; i < level; i++) {
     int pressed = waitForPress();
     if (pressed != sequence[i]) {
       playing = false;   // wrong button or timeout
       break;
     }
   }


   if (playing) {
     score += level * 10;       // points for completing this level
     showStatus();
     lcd.setCursor(11, 0);
     lcd.print("OK!");
     successAnimation();
     delay(600);


     if (level >= MAX_LEVEL) {
       won = true;
       playing = false;
     }
   }
 }


 gameOver(won);
}
