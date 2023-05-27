#include <funshield.h>
constexpr int buttons[] = { button1_pin, button2_pin, button3_pin }; // Array of button pins

// Digit bytes
constexpr int digitsCustom[] = {
  0b11000000, // 0
  0b11111001, // 1
  0b10100100, // 2
  0b10110000, // 3
  0b10011001, // 4
  0b10010010, // 5
  0b10000010, // 6
  0b11111000, // 7
  0b10000000, // 8
  0b10010000  // 9
};

// Character bytes
constexpr byte LETTER_GLYPH[] {
  0b10001000,   // A
  0b10000011,   // b
  0b11000110,   // C
  0b10100001,   // d
  0b10000110,   // E
  0b10001110,   // F
  0b10000010,   // G
  0b10001001,   // H
  0b11111001,   // I
  0b11100001,   // J
  0b10000101,   // K
  0b11000111,   // L
  0b11001000,   // M
  0b10101011,   // n
  0b10100011,   // o
  0b10001100,   // P
  0b10011000,   // q
  0b10101111,   // r
  0b10010010,   // S
  0b10000111,   // t
  0b11000001,   // U
  0b11100011,   // v
  0b10000001,   // W
  0b10110110,   // ksi
  0b10010001,   // Y
  0b10100100,   // Z
};

// Animation array has a format of:
// rollAnim[2i] == Number of LED to display to
// rollAnim[2i+1] == Parts of the LED to light up
constexpr byte rollAnim[] {
  0x01,
  0b11111110,
  0x01,
  0b11111101,
  0x01,
  0b11111011,
  0x01,
  0b01111111,
  0x02,
  0b11101111,
  0x02,
  0b10111111,
  0x02,
  0b11111011,
  0x02,
  0b01111111,
  0x04,
  0b11110111,
  0x08,
  0b11110111,
};

constexpr byte EMPTY_GLYPH = 0b11111111; // Empty character
constexpr int buttonsCount = sizeof(buttons) / sizeof(buttons[0]); // Number of buttons
constexpr int digitsCount = sizeof(digitsCustom) / sizeof(digitsCustom[0]); // Number of Digits
constexpr int dice[] = {4, 6, 8, 10, 12, 20, 100}; // Array of dice
constexpr int diceCount = sizeof(dice) / sizeof(dice[0]); // Number of dice
constexpr int animSpeed = 80; // Speed of animation updating

bool buttonStates[buttonsCount]; // Last state of the buttons
bool currentButtonState = false; // Current state of a button
enum Mode { normal, config }; // Enum to differentiate between modes
Mode mode = config; // Variable that holds the current mode
int startTime = 0; // Time of the button1 press, used for seed
int activeDice = 5; // Current dice, used for random() maximum
int throws = 1; // Number of throws
int lastRoll = 0; // Last throw result
int ledDigitIndex = 0; // Index of the LED Digit currently displayed, from the left, starting at 0
int currentRadix = 1; // Currently displayed radix of the number shown
char displayString[] = "1d6 "; // String to display in config, this is the base setting
char *stringPointer = displayString; // Pointer to the char of the string to be displayed
int displayStringLength = (sizeof(displayString) / sizeof(displayString[0])) - 1; // Length of the string
int animStep = 0; // Current step of the animation
int prevStepTime = 0; // millis() of the last step


void setup() {
  pinMode(latch_pin, OUTPUT);
  pinMode(data_pin, OUTPUT);
  pinMode(clock_pin, OUTPUT);
  for (int i = 0; i < buttonsCount; i++) {
    pinMode(buttons[i], INPUT);
    buttonStates[i] = true;
  }
}

void display_loop() {
  switch (mode) {
    case normal:
      if (!buttonStates[0]) {
        // If button1 is pressed, play the animation
        rollAnimation();
      }
      else {
        // Display lastRoll, first radix on the right-most digit
        if (currentRadix > lastRoll) {
          currentRadix = 1;
          ledDigitIndex = 0;
        }
        displayDigit((lastRoll / currentRadix) % 10, 3 - ledDigitIndex);
        currentRadix *= 10;
        ledDigitIndex = (ledDigitIndex + 1) % 4;
      }
      break;

    case config:
      // Display the die configuration
      if (dice[activeDice] < 10) {
        // Empty space at the end if the die has only 1 digit
        sprintf(displayString, "%dd%d ", throws, dice[activeDice]);
      }
      else if (dice[activeDice] > 99) {
        // Display last 2 digits of the die if has more than 2 digits
        sprintf(displayString, "%dd%02d", throws, dice[activeDice] % 100);
      }
      else {
        // Die has 2 digits
        sprintf(displayString, "%dd%d", throws, dice[activeDice]);
      }
      if(*stringPointer == '\0'){
        // Reached the end of the string, reset the pointer
        stringPointer = displayString;
        ledDigitIndex = 0; // Has to be reset aswell
      }
      displayChar(*stringPointer, ledDigitIndex);
      stringPointer++;
      ledDigitIndex = (ledDigitIndex + 1) % 4;
      break;
  }
}

void displayDigit(int digit, byte pos) {
  digitalWrite(latch_pin, LOW);
  shiftOut(data_pin, clock_pin, MSBFIRST, digitsCustom[digit]);
  shiftOut(data_pin, clock_pin, MSBFIRST, 1 << pos);
  digitalWrite(latch_pin, HIGH);
}

void displayChar(char ch, byte pos)
{
  byte glyph = EMPTY_GLYPH;
  if (isAlpha(ch)) {
    glyph = LETTER_GLYPH[ ch - (isUpperCase(ch) ? 'A' : 'a') ];
  }
  else if (isDigit(ch)) {
    // ch has the ASCII value of the digit character, and '0' has the smallest value of the digits
    glyph = digitsCustom[ch - '0'];
  }
  digitalWrite(latch_pin, LOW);
  shiftOut(data_pin, clock_pin, MSBFIRST, glyph);
  shiftOut(data_pin, clock_pin, MSBFIRST, 1 << pos);
  digitalWrite(latch_pin, HIGH);
}

void displayByte(byte b, byte pos) {
  // Display a specific part of the LED digit
  digitalWrite(latch_pin, LOW);
  shiftOut(data_pin, clock_pin, MSBFIRST, b);
  shiftOut(data_pin, clock_pin, MSBFIRST, pos);
  digitalWrite(latch_pin, HIGH);
}

void button1_press() {
  int currentTime = millis();
  if (buttonStates[0]) {
    // Only if the button wasn't pressed before
    mode = normal;
    startTime = currentTime;
    prevStepTime = currentTime;
    animStep = 0; // Begin the animation
  }
}

void button2_press() {
  // Increment the number of throws
  if (buttonStates[1]) {
    // Only if the button wasn't pressed before
    mode = config;
    throws = (throws % 9) + 1;
  }
}

void button3_press() {
  // Switch to the next die on the list
  if (buttonStates[2]) {
    // Only if the button wasn't pressed before
    mode = config;
    activeDice = (activeDice + 1) % diceCount;
  }
}

void roll(int seed) {
  // seed is the duration of button1 press
  // Rolls the active dice throws-times.
  randomSeed(seed);
  lastRoll = 0;
  for (int i = 0; i < throws; i++) {
    lastRoll += random(1, dice[activeDice] + 1);
  }
}

void rollAnimation() {
  int currentTime = millis();
  int animSize = sizeof(rollAnim) / sizeof(rollAnim[0]); // Size of the animation array
  if (currentTime - prevStepTime > animSpeed) {
    // If the interval for updating has passed
    prevStepTime = currentTime;
    if (animStep + 2 == animSize) {
      // To loop the last 2 steps of the animation
      animStep = animSize - 6;
    }
    // Because of the format, the next part begins at the next even index
    animStep = (animStep + 2) % animSize;
  }

  // Display the current step of the animation
  displayByte(rollAnim[animStep + 1], rollAnim[animStep]);
}

void loop() {
  for (int i = 0; i < buttonsCount; i++) {
    currentButtonState = digitalRead(buttons[i]);
    switch (i) {
      case 0:
        if (!currentButtonState) {
          // button1 pressed
          button1_press();
        }
        else if (buttonStates[0] == false && mode == normal) {
          // button1 up
          roll(millis() - startTime);
        }
        break;
      case 1:
        if (!currentButtonState) {
          // button2 pressed
          button2_press();
        }
        break;
      case 2:
        if (!currentButtonState) {
          // button3 pressed
          button3_press();
        }
        break;
      default:
        break;
    }
    // Set the previous state to current state for this button
    buttonStates[i] = currentButtonState;
  }
  display_loop();
}
