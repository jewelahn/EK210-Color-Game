/*
  ESP32 + 16x2 I2C LCD + 2 LEDs + 2 push buttons

  Wiring:
    LCD SDA  -> GPIO21
    LCD SCL  -> GPIO22   (the "SEL" in your message is assumed to be SCL)
    LCD VCC  -> 5V (VIN), GND -> GND
    Red LED   -> GPIO23 (through a ~220-330 ohm resistor to GND)
    Green LED -> GPIO19 (through a ~220-330 ohm resistor to GND)
    Button 1  -> GPIO18 and GND  (toggles red LED)
    Button 2  -> GPIO5  and GND  (toggles green LED)
    (Buttons use the internal pull-up, so no external resistors needed.)

  Library: "LiquidCrystal I2C" by Frank de Brabander (install from Library Manager)
  If the LCD stays blank, change LCD_ADDR to 0x3F or run an I2C scanner.
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------- Pins ----------
const int PIN_SDA   = 21;
const int PIN_SCL   = 22;
const int PIN_RED   = 23;
const int PIN_GREEN = 19;
const int PIN_BTN1  = 18;   // change if your buttons use other pins
const int PIN_BTN2  = 5;

// ---------- LCD ----------
const uint8_t LCD_ADDR = 0x27;
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

// ---------- State ----------
bool redOn   = false;
bool greenOn = false;

// ---------- Debounce ----------
struct Button {
  int pin;
  bool lastReading;
  bool stableState;
  unsigned long lastChange;
};

Button btn1 = {PIN_BTN1, HIGH, HIGH, 0};
Button btn2 = {PIN_BTN2, HIGH, HIGH, 0};
const unsigned long DEBOUNCE_MS = 40;

// Returns true once per press (on falling edge, since buttons are active LOW)
bool buttonPressed(Button &b) {
  bool reading = digitalRead(b.pin);
  bool pressed = false;

  if (reading != b.lastReading) {
    b.lastChange = millis();
    b.lastReading = reading;
  }

  if ((millis() - b.lastChange) > DEBOUNCE_MS && reading != b.stableState) {
    b.stableState = reading;
    if (b.stableState == LOW) pressed = true;
  }
  return pressed;
}

void updateOutputs() {
  digitalWrite(PIN_RED,   redOn   ? HIGH : LOW);
  digitalWrite(PIN_GREEN, greenOn ? HIGH : LOW);
}

void updateDisplay() {
  lcd.setCursor(0, 0);
  lcd.print("RED  : ");
  lcd.print(redOn ? "ON " : "OFF");
  lcd.print("      ");

  lcd.setCursor(0, 1);
  lcd.print("GREEN: ");
  lcd.print(greenOn ? "ON " : "OFF");
  lcd.print("      ");
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);

  Wire.begin(PIN_SDA, PIN_SCL);
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("ESP32 Ready");
  lcd.setCursor(0, 1);
  lcd.print("BTN1=Red BTN2=Grn");
  delay(1500);
  lcd.clear();

  updateOutputs();
  updateDisplay();
}

void loop() {
  bool changed = false;

  if (buttonPressed(btn1)) {
    redOn = !redOn;
    changed = true;
    Serial.printf("Red LED %s\n", redOn ? "ON" : "OFF");
  }

  if (buttonPressed(btn2)) {
    greenOn = !greenOn;
    changed = true;
    Serial.printf("Green LED %s\n", greenOn ? "ON" : "OFF");
  }

  if (changed) {
    updateOutputs();
    updateDisplay();
  }
}
