/*
 * Like all Arduino code - copied from somewhere else :)
 * So don't claim it as your own
 * Based on the code of Peten Paja -> http://petenpaja.blogspot.com/2013/04/first-project-arduino-spede.html?m=1
 * ---------------------------------
Modified version - 24/08 by skuydi
 * added for more button
 * added TM1637Display
 * added a switch on/off for the sound
 *
*/

#include <EEPROM.h>
#include <TM1637Display.h>

// Arduino pin connected to the speaker
const int tonePin = 2;

// Module connection pins (Digital Pins)
#define CLK 4
#define DIO 5

// Arduino Mega pins connected to latch, clock and data pins of the 74HC595 chip
const int latchPin = 7;
const int clockPin = 8;
const int dataPin = 9;

// Arduino pins connected to transistors controlling the digits
const int enableDigits[] = { 13, 10, 11, 12 };

// Arduino pins connected to leds
const int leds[] = { 22, 23, 24, 25, 26, 27, 28, 29 };

// Arduino pins connected to buttons
const int buttons[] = { 30, 31, 32, 33, 34, 35, 36, 37 };

// Frequencies of tones played when buttons are pressed
const int toneFreq[] = { 277, 311, 370, 415, 440, 466, 494, 523 };  // CS4, DS4, FS4, GS4, A4, AS4, B4, C5

// Segment bits for numbers 0-9
int digits[10] = {
  B1111101, // 0  ABCDEF-
  B1000001, // 1  -BC----
  B1011110, // 2  AB-DE-G
  B1010111, // 3  ABCD--G  
  B1100011, // 4  -BC--FG
  B0110111, // 5  A-CD-EF
  B0111111, // 6  A-BCDEF
  B1010001, // 7  ABC----
  B1111111, // 8  ABCDEFG
  B1110111, // 9  ABCD-FG
};

enum {
  STATE_START_MENU,
  STATE_GAME,
  STATE_GAME_OVER
};

int score = 0;
int led = 0;
int prevLed = 0;
int nextTimer = 0;
int level = 0;
int hiscore = 0;
int startMenuTimer = 0;
int prevButtonState[8] = { HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH };
int state = STATE_START_MENU;

// Read hiscore value from EEPROM
void readHiscore() {
  hiscore = (EEPROM.read(0) << 8) + EEPROM.read(1);
  
  // EEPROM initial value is FFFF
  if(hiscore == 0xffff)
    hiscore = 0;
}

// Write hiscore value to EEPROM
void writeHiscore() {
  EEPROM.write(0, hiscore >> 8);
  EEPROM.write(1, hiscore & 0xff);
}

TM1637Display display(CLK, DIO);

void setup() {
  delay(1000);
  display.setBrightness(0x0f);  // Switch off all segments 
  Serial.begin(9600); // open the serial port at 9600 bps:
  Serial.println("Initialisation...");
  Serial.println("Prêt à jouer !");
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  
  for(int i = 0; i < 4; i++)
    pinMode(enableDigits[i], OUTPUT);
  
  for(int i = 0; i < 8; i++)
    pinMode(leds[i], OUTPUT);

  // enable pull up resistors for button pins
  for(int i = 0; i < 8; i++)
    pinMode(buttons[i], INPUT_PULLUP);
    
  readHiscore();
}

// Updates display with current score.
// Flashes 4 digits quickly on the display.
// Display is turned off if enable is false.
void updateDisplay(int score, boolean enable) {
  int s = score;
  for(int pos = 0; pos < 4; pos++) {
    int digit = s % 10;
    s /= 10;

    // turn off all digits
    for( int i = 0; i < 4; i++ )
      digitalWrite(enableDigits[i], LOW);
      delayMicroseconds(500);

      // set latch low so digits won't change while sending bits 
      digitalWrite(latchPin, LOW);

      // shift out the bits
      shiftOut(dataPin, clockPin, MSBFIRST, digits[digit]);  
  
      // set latch high so the LEDs will light up
      digitalWrite(latchPin, HIGH);
      delayMicroseconds(500);
 
    // turn on one digit
    if(enable)
      digitalWrite(enableDigits[pos], HIGH);

    delayMicroseconds(2000);
  }
}

// Updates the start menu. Switch between previous score and hiscore on the display.
// Start a new game when a button is pressed. Clear the hiscore is all buttons are held down for 2 seconds.
void startMenu() {
  int s = score;

  // flick between previous score and hiscore display
  startMenuTimer = (startMenuTimer + 1) % 2000;
  if(startMenuTimer >= 1000)
    s = hiscore;

  updateDisplay(s, startMenuTimer < 975 || (startMenuTimer > 1000 && startMenuTimer < 1975));
  
  // read button state
  int buttonState = 0;
  for(int i = 0; i < 8; i++)
    if(digitalRead(buttons[i]) == LOW)
      buttonState |= 1<<i;

  // reset hiscore if all buttons are held down for 2 seconds
  static long resetHiscoreTimer = 0;
  if(buttonState == 255) {
    if(resetHiscoreTimer == 0)
      resetHiscoreTimer = millis();
    if(millis() - resetHiscoreTimer > 2000) {
      updateDisplay(0, false);
      tone(tonePin, 500, 500);
      hiscore = 0;
      writeHiscore();
      delay(700);
      resetHiscoreTimer = 0;
    }
  } else {
    resetHiscoreTimer = 0;
  }

  // start new game if a single button is pressed for 100ms
  static int startNewGameTimer = 0;
  if(buttonState > 0 && buttonState < 255 && (buttonState & (buttonState - 1)) == 0) {
    if(startNewGameTimer == 0)
      startNewGameTimer = millis();
    if(millis() - startNewGameTimer > 50) {  
      // start new game
      updateDisplay(score, false);
      delay(2000);
      startNewGame();
      startNewGameTimer = 0;
    }
  } else {
    startNewGameTimer = 0;
  }
}

// Prepares game state for a new game.
void startNewGame() {
  state = STATE_GAME;
  score = 0;
  level = -1;
  led = -1;
  prevLed = -1;
  nextTimer = 0;
 
  for(int i = 0; i < 8; i++) 
    prevButtonState[i] = HIGH;

  // set random seed, so that every game has a different sequence
  randomSeed(millis());
}

void playGame() {
  // update time
  nextTimer--;
  
  if(nextTimer < 0) {
    // game over if player is too slow
    if(led >= 0) {
      gameOver();
      return;
    }

    led = random(8);
    // make consequent same leds less probable
    if(led == prevLed && random(10) < 6)
      led = random(8);
    prevLed = led;
    nextTimer = max(150 * pow(1.6, -level*0.05), 10);
    level++;
    tone(tonePin, toneFreq[led], nextTimer * 8);
    score = level;
  }
  
  // update leds
  for(int i = 0; i < 8; i++)
    digitalWrite(leds[i], led == i || (digitalRead(buttons[i]) == LOW && nextTimer > 5) ? HIGH : LOW);
    updateDisplay(score, true);
  
  // read input   
  for(int i = 0; i < 8; i++) {
    int but = digitalRead(buttons[i]);
    if(but == LOW && prevButtonState[i] == HIGH) {
      // ignore button press if time since last press is too short
      if( led >= 0 ) { //&& millis() - lastButtonPress > 50 ) { 
        // correct button pressed?
        if( i == led ) {
          score++;
          showScore();
          led = -1;  // turn off led
        } else {
          gameOver();
        }
        //lastButtonPress = millis();
        noTone(tonePin);
      }
    }
    prevButtonState[i] = but;
  }
}

void showScore(){
display.showNumberDec(score, false); 
}

// Game over. Play a game over sound and blink score.
void gameOver() {
  tone(tonePin, 250, 2500);

  // new hiscore?
  if(score > hiscore) {
    hiscore = score;
    writeHiscore();
  }
  Serial.print("Score: ");
  Serial.println(score);
  display.showNumberDec(score, false);    // Affichage du score
  delay(5000);
  display.clear();
  Serial.print("High score: ");
  Serial.println(hiscore);
  display.showNumberDec(hiscore, false);  // Affichage du highscore

  // turn on leds
  for(int i = 0; i < 8; i++)
    digitalWrite(leds[i], HIGH);
    
  // flash display
  for(int i = 0; i < 70*5; i++) {
    if(i == 70*2)
      tone(tonePin, 200, 2000);    
    boolean enable = 1 - (i/60) & 1;
    updateDisplay(score, enable);
    
  }
  
  // turn off leds
  for(int i = 0; i < 8; i++)
    digitalWrite(leds[i], LOW);
    updateDisplay(score, false);
    Serial.println("Prêt à jouer !");
    Serial.println(" ");
  
  // enter menu
  //delay(1000);
  state = STATE_START_MENU;
  startMenuTimer = 0;
}

// Main loop. Update menu, game or game over depending on current state.
void loop() {
  if(state == STATE_START_MENU)
    startMenu();
  else if(state == STATE_GAME)
    playGame();
  else
    gameOver();
}
