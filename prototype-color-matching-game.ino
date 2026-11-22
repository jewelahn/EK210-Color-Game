#include <Wire.h>
#include <LiquidCrystal_I2C.h>

//Last Updated 10/28/26 for the Prototype Demo
// =====================================================
// LCD CONFIGURATION
// =====================================================
LiquidCrystal_I2C lcd(0x27, 16, 2);


// =====================================================
// PIN CONFIGURATION: ESP32-WROOM
// =====================================================


// LEDs/Users/jewelahn/Documents/Arduino/oct28-color-matching-game/oct28-color-matching-game.ino
const int RED_LED    = 32;
const int YELLOW_LED = 33;
const int GREEN_LED  = 25;
const int BLUE_LED   = 26;


// Buttons (connect each button between GPIO and GND)
const int RED_BUTTON    = 19;
const int YELLOW_BUTTON = 18;
const int GREEN_BUTTON  = 17;
const int BLUE_BUTTON   = 16;


// Optional vibrating motor driver control.
// Change this pin if your motor driver uses another GPIO.
const int MOTOR_PIN = 27;


// =====================================================
// GAME SETTINGS
// =====================================================
const int EASY_ROUNDS = 5;
const int BASE_LENGTH = 4;


const unsigned long RESPONSE_TIMEOUT = 30000;
const unsigned long DEBOUNCE_MS = 35;


// Color IDs
enum Color {
 RED = 0,
 YELLOW,
 GREEN,
 BLUE
};


const int ledPins[4] = {
 RED_LED, YELLOW_LED, GREEN_LED, BLUE_LED
};


const int buttonPins[4] = {
 RED_BUTTON, YELLOW_BUTTON, GREEN_BUTTON, BLUE_BUTTON
};


const char* colorNames[4] = {
 "Red", "Yellow", "Green", "Blue"
};


// Hard mode can grow beyond the initial 4 colors.
const int MAX_SEQUENCE = 200;
int sequence[MAX_SEQUENCE];


// Game state
int score = 0;
int rounds = 0;
int extraPatterns = 0;


bool gameRunning = true;
bool hardMode = false;


// =====================================================
// INITIALIZATION
// =====================================================
void setup() {
 Serial.begin(115200);


// Initialize LEDs
 for (int i = 0; i < 4; i++) {
   pinMode(ledPins[i], OUTPUT);
   digitalWrite(ledPins[i], LOW);
 }


 // Initialize buttons using internal pull-up resistors
 for (int i = 0; i < 4; i++) {
   pinMode(buttonPins[i], INPUT_PULLUP);
 }


 // Motor driver output
 pinMode(MOTOR_PIN, OUTPUT);
 digitalWrite(MOTOR_PIN, LOW);


 // Initialize I2C LCD
 Wire.begin(21, 22);
 lcd.init();
 lcd.backlight();


 randomSeed(micros());


 lcd.clear();
 lcd.setCursor(0, 0);
 lcd.print("Memory Game");
 lcd.setCursor(0, 1);
 lcd.print("Starting...");
 delay(1500);


 // Choose difficulty and play games
 while (gameRunning) {
   chooseDifficulty();
   playGame();
   showFinalScore();
   askReplay();
 }


 // Replay declined: shut down game outputs and LCD
 allLedsOff();
 digitalWrite(MOTOR_PIN, LOW);
 lcd.clear();
 lcd.noBacklight();


 // Deep sleep approximates powering off the game.
 // USB power to the ESP32 itself remains connected.
 esp_deep_sleep_start();
}


void loop() {
 // Game runs from setup().
}


// =====================================================
// BUTTON HANDLING
// Returns RED, YELLOW, GREEN, BLUE or -1 on timeout.
// =====================================================
int readButton(unsigned long timeoutMs) {
 unsigned long startTime = millis();


 while (millis() - startTime < timeoutMs) {
   for (int i = 0; i < 4; i++) {
     if (digitalRead(buttonPins[i]) == LOW) {
       delay(DEBOUNCE_MS);


       if (digitalRead(buttonPins[i]) == LOW) {
         // Wait for release to avoid counting one hold twice.
         while (digitalRead(buttonPins[i]) == LOW) {
           delay(5);
         }


         delay(DEBOUNCE_MS);
         return i;
       }
     }
   }


   delay(1);
 }


 return -1;
}


// Wait indefinitely for a button, with debounce.
int waitForButton() {
 int button;


 do {
   button = readButton(100);
 } while (button == -1);


 return button;
}


// =====================================================
// LED HELPERS
// =====================================================
void allLedsOff() {
 for (int i = 0; i < 4; i++) {
   digitalWrite(ledPins[i], LOW);
 }
}


void flashColor(int color, int durationMs) {
 digitalWrite(ledPins[color], HIGH);
 delay(durationMs);
 digitalWrite(ledPins[color], LOW);
}


// =====================================================
// DIFFICULTY SELECTION
// =====================================================
void chooseDifficulty() {
 lcd.clear();
 lcd.setCursor(0, 0);
 lcd.print("Choose mode:");
 lcd.setCursor(0, 1);
 lcd.print("Green=Easy Red=Hard");


 while (true) {
   int button = waitForButton();


   if (button == GREEN) {
     hardMode = false;
     lcd.clear();
     lcd.print("Easy Mode");
     delay(1000);
     return;
   }


   if (button == RED) {
     hardMode = true;
     lcd.clear();
     lcd.print("Hard Mode");
     delay(1000);
     return;
   }


   // Blue and yellow do not select difficulty.
 }
}


// =====================================================
// START A GAME
// =====================================================
void playGame() {
 score = 0;
 rounds = 0;
 extraPatterns = 0;


 // Easy mode: exactly 5 rounds.
 // Hard mode: continue until an error or timeout.
 while (hardMode || rounds < EASY_ROUNDS) {
   int sequenceLength = BASE_LENGTH + extraPatterns;


   // Protect the sequence array from overflow.
   if (sequenceLength > MAX_SEQUENCE) {
     sequenceLength = MAX_SEQUENCE;
   }


   // Generate a random sequence for this round.
   for (int i = 0; i < sequenceLength; i++) {
     sequence[i] = random(0, 4);
   }


   // Show the sequence to the player.
   showSequence(sequenceLength);


   // Prompt for the player's response.
   lcd.clear();
   lcd.setCursor(0, 0);
   lcd.print("Repeat sequence");
   lcd.setCursor(0, 1);
   lcd.print("Round ");
   lcd.print(rounds + 1);


   delay(500);


   bool correct = checkSequence(sequenceLength);


   if (!correct) {
     // Wrong button or response timeout.
     loseGame();
     return;
   }


   // Successful round
   score++;
   rounds++;


   lcd.clear();
   lcd.setCursor(0, 0);
   lcd.print("Correct!");
   lcd.setCursor(0, 1);
   lcd.print("Score: ");
   lcd.print(score);


   delay(1000);


   // Easy mode ends after 5 successful rounds.
   if (!hardMode && rounds == EASY_ROUNDS) {
     return;
   }


   // Hard mode adds one extra color every 3 rounds.
   if (hardMode && rounds % 3 == 0) {
     extraPatterns++;
   }
 }
}


// =====================================================
// DISPLAY THE RANDOM COLOR SEQUENCE
// =====================================================
void showSequence(int sequenceLength) {
 lcd.clear();
 lcd.setCursor(0, 0);
 lcd.print("Watch carefully");
 delay(700);


 for (int i = 0; i < sequenceLength; i++) {
   flashColor(sequence[i], 500);
   delay(250);
 }


 allLedsOff();
}


// =====================================================
// CHECK PLAYER INPUT
// =====================================================
bool checkSequence(int sequenceLength) {
 for (int i = 0; i < sequenceLength; i++) {
   lcd.setCursor(0, 1);
   lcd.print("Input ");
   lcd.print(i + 1);
   lcd.print("/");
   lcd.print(sequenceLength);
   lcd.print("       ");


   int button = readButton(RESPONSE_TIMEOUT);


   // Timeout or incorrect button
   if (button == -1 || button != sequence[i]) {
     return false;
   }


   // Brief visual confirmation of the player's input
   flashColor(button, 150);
 }


 return true;
}


// =====================================================
// LOSING CONDITION AND VIBRATION MOTOR
// =====================================================
void loseGame() {
 lcd.clear();
 lcd.setCursor(0, 0);
 lcd.print("Wrong or timeout");
 lcd.setCursor(0, 1);
 lcd.print("Game over!");


 // Two short motor pulses
 for (int i = 0; i < 2; i++) {
   digitalWrite(MOTOR_PIN, HIGH);
   delay(250);
   digitalWrite(MOTOR_PIN, LOW);
   delay(250);
 }


 delay(1000);
}


// =====================================================
// DISPLAY FINAL SCORE
// =====================================================
void showFinalScore() {
 lcd.clear();
 lcd.setCursor(0, 0);


 if (!hardMode && rounds == EASY_ROUNDS) {
   lcd.print("You Win!");
 } else {
   lcd.print("Game Lost");
 }


 lcd.setCursor(0, 1);
 lcd.print("Score: ");
 lcd.print(score);


 delay(2500);
}


// =====================================================
// REPLAY PROMPT
// Green = replay, Red = exit
// =====================================================
void askReplay() {
 lcd.clear();
 lcd.setCursor(0, 0);
 lcd.print("Play again?");
 lcd.setCursor(0, 1);
 lcd.print("Green=Yes Red=No");


 while (true) {
   int button = waitForButton();


   if (button == GREEN) {
     return;  // setup() loop starts another game
   }


   if (button == RED) {
     gameRunning = false;
     return;
   }
 }
}
