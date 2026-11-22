//Last Updated 11/15/26
//CODE DOES NOT FULLY WORK. 
//I am keeping for documentation purposes, since I rewrote many of notes into here

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

//Set up LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

//Set up other components
//Leds: 
const int RED_LED    = 32;
const int YELLOW_LED = 33;
const int GREEN_LED  = 25;
const int BLUE_LED   = 26;

//Buttons
const int RED_BUTTON    = 19;
const int YELLOW_BUTTON = 18;
const int GREEN_BUTTON  = 17;
const int BLUE_BUTTON   = 16;

//Vibrating motor/Buzzer driver control.
const int Buzzer = 27;

//Global Variables for the game mechanics
const int EASY_ROUNDS = 5;
const int BASE_LENGTH = 4;
const unsigned long RESPONSE_TIMEOUT = 30000;
const unsigned long DEBOUNCE_MS = 35;

//Color IDs: 
//setting up each set of 4-color component up as enum to easily loop 
//through a int 4 for loop when changing them to Input to Output and vice versa
enum Color {
  RED = 0,
  YELLOW,
  GREEN,
  BLUE
};

//So ledPin[0] will call RED_LED
const int ledPins[4] = {
  RED_LED, YELLOW_LED, GREEN_LED, BLUE_LED
};

const int buttonPins[4] = {
  RED_BUTTON, YELLOW_BUTTON, GREEN_BUTTON, BLUE_BUTTON
};

const char* colorNames[4] = {
  "Red", "Yellow", "Green", "Blue"
};

//Hard starts at 4 colors each, then increases by 1 color every 3 rounds.
//To protect from overflow, the maximum number of colors in a sequence is set to 200
const int MAX_SEQUENCE = 200;
int sequence[MAX_SEQUENCE];

//Game state variables 
int score = 0;
int rounds = 0;
int extraPatterns = 0;
bool gameRunning = true;
bool hardMode = false;


void setup() {
  Serial.begin(115200); //115200 baud rate for ESP32s

 //LEDs initialized as outputs and on low
  for (int i = 0; i < 4; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  //Buttons set up with internal pull-up resistors to prevent small changes 
  //in voltage/small touches from activating an input
  for (int i = 0; i < 4; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP); //This means Low is when the Button is on, and High is when it is off
  }

  pinMode(Buzzer, OUTPUT);
  digitalWrite(Buzzer, LOW);

  //LCD intializariton
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  //the seed for the random number pattern for the color sequence is generated 
  //using the current time in microseconds so that it can be different for every single game. 
  //Using just a random seed with randint() caused the game to repeat a lot of patterns. 
  randomSeed(micros()); 

  //Starting dialogue on LCD screen
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Memory Game");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(1500);

  //Choose difficulty of Games
  while (gameRunning) {
    chooseDifficulty();
    playGame();
    showFinalScore();
    askReplay();
  }

  // Replay declined: shut down game outputs and LCD
  allLedsOff();
  lcd.clear();
  lcd.noBacklight();

  // Deep sleep approximates powering off the game.
  // USB power to the ESP32 itself remains connected.
  esp_deep_sleep_start();
}

void loop() {
  // Game runs from setup().
}

/* Button Handling
    readButton() uses digitalRead in a loop to read all four button. 
    Then, return an number corresponding to the button pressed. 
    0 = Red, 1 = Yellow, 2 = Green, 3 = Blue
    -1 is returned if no button press is read. 
*/ 
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
  //If no button was pressed, -1 is returned
  return -1;
}

//Waits indefinitely for a button signal, with debounce time.
int waitForButton() {
  int button;
  while (button == -1) { //Each of the colors return 0 1 2 or 3, but if the button has no signal, it returns -1
    button = readButton(100);
  }
  return button;
}

//Turns off all LEDs
void allLedsOff() {
  for (int i = 0; i < 4; i++) {
    digitalWrite(ledPins[i], LOW);
  }
}

//Turns on 1 LED at a time
void flashColor(int color, int durationMs) {
  digitalWrite(ledPins[color], HIGH);
  delay(durationMs);
  digitalWrite(ledPins[color], LOW);
}

//Displaying the random color sequence on the LEDs
void showSequence(int sequenceLength) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Watch closely!");
  delay(700);

  for (int i = 0; i < sequenceLength; i++) {
    flashColor(sequence[i], 500);
    delay(250);
  }
  allLedsOff();
}

/* Deciding Difficulty
  Two modes: EASY and HARD
  - Easy Mode: 5 rounds of 4-color sequences. 
    Game ends when all 5 rounds are done, regardless of you won that round or not
    Score/5 is displayed on LCD after. 
    Is selected by pressing the Green button. 
  
  - Hard mode: inifnite rounds of color sequences. 
    Ends when you lose a round
    Starts with 4 colors, then increases the # of colors by 1, every 3 rounds.
    Selected by pressing the Red Button. 
*/
void chooseDifficulty() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Choose mode:");
  lcd.setCursor(0, 1);
  lcd.print("Green=Easy Red=Hard"); //Cuts off screen, but is enough to understand. Maybe fix later

  bool buttonRecieved = false;
  //Loops until it senses a signal from Red or Green Button
  while (!buttonRecieved) {
    int button = waitForButton();

    if (button == GREEN) {
      hardMode = false;
      lcd.clear();
      lcd.print("Easy Mode");
      delay(1000);
      buttonRecieved = true;
    }

    if (button == RED) {
      hardMode = true;
      lcd.clear();
      lcd.print("Hard Mode");
      delay(1000);
      buttonRecieved = true;
    }
  }
}

// playGame() is the main game code
void playGame() {
  score = 0;
  rounds = 0;
  extraPatterns = 0;

  //Easy mode: only 5 rounds.
  //Hard mode: continue infinitely until a mistake is made.
  
  while (hardMode || rounds < EASY_ROUNDS) {
    int sequenceLength = BASE_LENGTH + extraPatterns;

    //Protect the sequence array from overflow, max is 200
    if (sequenceLength > MAX_SEQUENCE) {
      sequenceLength = MAX_SEQUENCE;
    }

    //Generate a random sequence for this round.
    for (int i = 0; i < sequenceLength; i++) {
      sequence[i] = random(0, 4);
    }
    showSequence(sequenceLength);

    //Write LCD screen to prommpt player to respond
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Repeat sequence");
    lcd.setCursor(0, 1);
    lcd.print("Round ");
    lcd.print(rounds + 1);
    delay(500);

    //Check if correct
    bool correct = checkSequence(sequenceLength);
    // Wrong button was pressed
    if (!correct) {
      loseGame();
      return;
    } else { //Successful round
      score++;
      rounds++;
    }

  //Display score:
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

//Player Input functions
bool checkSequence(int sequenceLength) {
  for (int i = 0; i < sequenceLength; i++) {
    //Prints how many inputs the user has entered already
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
    //Flashes LED of same color so that Player can confirm that they put in the color they intended tlo
    flashColor(button, 150);
  }
  return true;
}

//Lose scenario
void loseGame() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Wrong!");
  lcd.setCursor(0, 1);
  lcd.print("Game over!");

  // Two short Buzz sounds 
  for (int i = 0; i < 2; i++) {
    tone(Buzzer,500);
    delay(250);
    noTone(Buzzer);
    delay(250);
  }
  delay(1000);
}

//Displays the final score
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

//Replay prompt function: Asks user whether or not they want to replay
//Player responds using the Red Or Green button
void askReplay() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Play again?");
  lcd.setCursor(0, 1);
  lcd.print("Green=Yes Red=No");

  bool buttonRecieved = false;
  while (!buttonRecieved) {
    //Plays another game
    int button = waitForButton();
    if (button == GREEN) {
      buttonRecieved = true;
    }

    //Ends game and turns off gameRunning state
    if (button == RED) {
      gameRunning = false;
      buttonRecieved = true;
    }
  }
}
