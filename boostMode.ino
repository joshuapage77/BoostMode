#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif
#include "math.h"

// Pin definitions
#define LED_DATA_PIN        7  // to data in of first pixel
#define BOOST_BUTTON_PIN    8  // momentary switch mounted on steering wheel
#define MODE_BUTTON_PIN     6  // future use
#define BOOST_RELAY_PIN     10 // connects to relay adding additional 6v in paralell

// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS 5

// States - some have an initialization state they base through first
#define S_INIT                 0
#define S_CHARGED_I            1
#define S_CHARGED              2
#define S_BOOSTING_I           3
#define S_BOOSTING             4
#define S_CHARGING_I           5
#define S_CHARGING             6
#define S_CHARGING_DEPLEATED_I 7
#define S_CHARGING_DEPLEATED   8
#define S_DEPLETED_I           9
#define S_DEPLETED             10

#define MAX_CHARGE  100

// Speed that charge changes during different states (milliseconds)
// I like the 3/4/6 ratio.
#define I_CHARGE             200
#define I_DEPLETED_CHARGE    150
#define I_BOOST              100

// 255 is max for pixel, but you can lower the max here. I'm using 25
#define MAX_BRIGHT    255
// used to determin led to light
#define DASH_RATIO    ((float)(NUMPIXELS) / (float)MAX_CHARGE)
#define LED_BUCKET_SIZE ((float)MAX_CHARGE / (float)NUMPIXELS)
// used to determin brightness based on charge level of bucket
#define BRIGHT_RATIO  (float)MAX_BRIGHT / LED_BUCKET_SIZE

// When setting up the NeoPixel library, we tell it how many pixels,
// and which pin to use to send signals
// Though Hole NeoPixel: use NEO_RGB instead of NEO_GRB
Adafruit_NeoPixel pixels(NUMPIXELS, LED_DATA_PIN, NEO_RGB + NEO_KHZ800);


int readValue = 0;
int state = S_INIT;
int charge = MAX_CHARGE;
// tokens are used with method timeElapse to abstract time syncing logic
unsigned long chargeToken = 0;
unsigned long pixelToken = 0;
unsigned long serialToken = 0;

// Call this function with token variable set to 0. That first time it will
// mark the start in this variable and return false on each call until the the
// number of milliseconds in interval has elapsed at which point it marks the timeElapse
// again and returns true. Use in an if statement to tie events to time instead of cycles
bool timeElapse(unsigned long *token, long interval) {
  if (*token == 0) {
    *token = millis();
  } else {
    if (millis() - *token >= interval) {
      *token = millis();
      return true;
    }
  }
  return false;
}

// not currently using this
void color32ToRGB (uint32_t color, byte *red, byte *green, byte *blue) {
  *red = (uint8_t)(color >> 16);
  *green = (uint8_t)(color >> 8);
  *blue = (uint8_t)color;
}

void updateBoostMeter(bool red, bool green, bool blue, bool force) {
  if (force || timeElapse(&pixelToken, 50)) {
    int highPixel = (float)charge * DASH_RATIO;
    int p_bright = charge - highPixel * LED_BUCKET_SIZE;
    byte bright = (float)p_bright * BRIGHT_RATIO;
    uint32_t color = pixels.Color(red * bright, green * bright, blue * bright);
    for(int i=0; i<=highPixel; i++) {
      if (i == highPixel) pixels.setPixelColor(i, color);
      else pixels.setPixelColor(i, pixels.Color(red * MAX_BRIGHT, green * MAX_BRIGHT, blue * MAX_BRIGHT));
    }
    for (int i=highPixel+1; i<NUMPIXELS; i++) {
      pixels.setPixelColor(i, pixels.Color(0, 0, 0));
    }
    pixels.show();
    Serial.print("State: ");
  }
}
void forceUpdateBoostMeter(bool red, bool green, bool blue) {
  updateBoostMeter(red, green, blue, true);
}
void updateBoostMeter(bool red, bool green, bool blue) {
  updateBoostMeter(red, green, blue, false);
}

void setup() {
  //Serial.begin(9600);
  pinMode(BOOST_BUTTON_PIN, INPUT);
  pinMode(MODE_BUTTON_PIN, INPUT);
  pinMode(BOOST_RELAY_PIN, OUTPUT);
  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
}

// will need this abstracted for the future
int getInput(int pin) {
  return digitalRead(pin);
}


void loop() {
  int readValue = getInput(BOOST_BUTTON_PIN);

  switch(state) {
    case S_INIT:
      charge = MAX_CHARGE;
      // TODO: do night rider with boost output
      state = S_CHARGED_I;
      break;
    case S_CHARGED_I:
      forceUpdateBoostMeter(false, true, false);
      state = S_CHARGED;
      break;
    case S_CHARGED:
      if (readValue == HIGH) {
        state = S_BOOSTING_I;
      }
      break;
    case S_BOOSTING_I:
      digitalWrite(BOOST_RELAY_PIN, HIGH);
      state = S_BOOSTING;
      chargeToken = 0;
    case S_BOOSTING:
      updateBoostMeter(true, true, false);
      if (readValue == LOW) {
        digitalWrite(BOOST_RELAY_PIN, LOW);
        state = S_CHARGING_I;
      } else {
        if (timeElapse(&chargeToken, I_BOOST)) {
          charge--;
        }
        //TODO: charge < 5 trigger warning beep
        if (charge == 0) {
          digitalWrite(BOOST_RELAY_PIN, LOW);
          state = S_DEPLETED_I;
        }
      }
      break;
    case S_CHARGING_I:
      state = S_CHARGING;
      chargeToken = 0;
    case S_CHARGING:
      updateBoostMeter(false, true, false);
      if (timeElapse(&chargeToken, I_CHARGE)) {
        charge++;
      }
      if (charge == MAX_CHARGE) {
        state = S_CHARGED_I;
      }
      if (readValue == HIGH) {
        state = S_BOOSTING_I;
      }
    break;
    case S_DEPLETED_I:
    case S_DEPLETED:
      //TODO: sound, lights flash, bla bla
      state = S_CHARGING_DEPLEATED_I;
      break;
    case S_CHARGING_DEPLEATED_I:
      chargeToken = 0;
      state = S_CHARGING_DEPLEATED;
    case S_CHARGING_DEPLEATED:
      updateBoostMeter(true, false, false);
    if (timeElapse(&chargeToken, I_DEPLETED_CHARGE)) {
        charge++;
      }
      if (charge == MAX_CHARGE) {
        state = S_CHARGED_I;
      }
      break;
  }
}

