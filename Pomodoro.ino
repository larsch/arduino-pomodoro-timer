#define DELAY 250

#include <FastLED.h>

// This code is not very generic and is tailored to my particular
// wiring of a 4-digit common anode 7-segment LED display. It's simple
// and fast though.

// Segment to bit mapping (bit 0-5 are later translated to PORTD2-7
// while bit 6 and 7 are translated to PORTC4 and 5). The 7-segment
// cathodes are connected to these.
#define SEG_A _BV(1)
#define SEG_B _BV(6)
#define SEG_C _BV(5)
#define SEG_D _BV(2)
#define SEG_E _BV(0)
#define SEG_F _BV(3)
#define SEG_G _BV(7)
#define SEG_P _BV(4)

// Numbers
#define DIG_0 (SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F)
#define DIG_1 (SEG_B|SEG_C)
#define DIG_2 (SEG_A|SEG_B|SEG_D|SEG_E|SEG_G)
#define DIG_3 (SEG_A|SEG_B|SEG_C|SEG_D|SEG_G)
#define DIG_4 (SEG_B|SEG_C|SEG_F|SEG_G)
#define DIG_5 (SEG_A|SEG_C|SEG_D|SEG_F|SEG_G)
#define DIG_6 (SEG_A|SEG_C|SEG_D|SEG_E|SEG_F|SEG_G)
#define DIG_7 (SEG_A|SEG_B|SEG_C)
#define DIG_8 (SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F|SEG_G)
#define DIG_9 (SEG_A|SEG_B|SEG_C|SEG_D|SEG_F|SEG_G)

// Array of digits
const uint8_t dig[10] = {
  DIG_0, DIG_1, DIG_2, DIG_3, DIG_4,
  DIG_5, DIG_6, DIG_7, DIG_8, DIG_9
};

// Anodes are connected to A0-A3
#define DIGIT_MASK 0x0F

void disableDigits()
{
  DDRC &= ~DIGIT_MASK; // set to input
  PORTC &= ~DIGIT_MASK;
}

void enableDigit(int i)
{
  PORTC |= (0x08 >> i);
  DDRC |= (0x08 >> i);
}

void setDigit(uint8_t segMask)
{
  uint8_t cMask = (segMask >> 2) & 0x30; // Bits for PORTC
  uint8_t dMask = segMask << 2; // Bits for PORTD

  PORTC = (PORTC & 0xCF) | (0x30 ^ cMask); // set cathodes to output
  DDRC = (DDRC & 0xCF) | cMask; // set cathodes low
  PORTD = (PORTD & 0x03) | (0xFC ^ dMask); // set cathodes to output
  DDRD = (DDRD & 0x03) | dMask;
}

CRGB led;

static uint32_t endTime;
#define RUN_TIME 60 * 25000ull + 999
#define PAUSE_TIME 60 * 5000ull + 999

#define RESET_TIME 3000

enum State { IDLE, RUN, PAUSE } state = IDLE;

void startRunMode()
{
  endTime = millis() + RUN_TIME;
  state = RUN;
  led = CRGB(0, 255, 0);
  FastLED.show();
}

void startPauseMode()
{
  endTime = millis() + PAUSE_TIME;
  state = PAUSE;
  led = CRGB(128, 0, 0);
  FastLED.show();
}

void startIdleMode()
{
  endTime = millis();
  state = IDLE;
  led = CRGB(64, 128, 0);
  FastLED.show();
}

// Display all 4 digits one by one by enabling each digit in turn with
// the correct cathodes pulled low in output mode. Must be called
// often (100's of times per second)
void updateDigits(uint16_t timeLeft)
{
  int minutes = timeLeft / 60;
  int seconds = timeLeft % 60;

  setDigit(dig[minutes / 10]);
  enableDigit(0);
  delayMicroseconds(250);
  disableDigits();

  setDigit(dig[minutes % 10] | SEG_P);
  enableDigit(1);
  delayMicroseconds(250);
  disableDigits();

  setDigit(dig[seconds / 10]);
  enableDigit(2);
  delayMicroseconds(250);
  disableDigits();

  setDigit(dig[seconds % 10]);
  enableDigit(3);
  delayMicroseconds(250);
  disableDigits();
}

void setup()
{
  disableDigits();
  FastLED.addLeds<APA106, 8, GRB>(&led, 1);
  FastLED.setBrightness(255);
  Serial.begin(115200);

  PORTB |= _BV(PORTB1); // Pull-up on PINB1 for button

  startIdleMode();
}

static int lastTimeLeft = 0;
static uint16_t shiftRegister = 0;
static uint32_t lastUpTime = 0;

void loop()
{
  int32_t timeLeft = int32_t(endTime - millis());
  if (timeLeft != lastTimeLeft) {
    if (state == RUN && timeLeft < 0) {
      startPauseMode();
    } else if (state == PAUSE && timeLeft < 0) {
      startIdleMode();
    }
    lastTimeLeft = timeLeft;
  }

  // Simple de-bounce
  uint8_t input = (PINB >> 1) & 1;
  shiftRegister = (shiftRegister << 1) | input | 0xe000;
  if (state == IDLE && shiftRegister == 0xf000) {
    startRunMode();
  }

  // Allow reset in RUN mode only by holding button
  if (state == RUN) {
    if (shiftRegister != 0xe000) {
      lastUpTime = millis();
    }
    if (int32_t(millis() - lastUpTime) > RESET_TIME) {
      startIdleMode();
    }
  }

  // Show digits in RUN/PAUSE mode
  if (state != IDLE) {
    updateDigits(timeLeft / 1000);
  } else {
    disableDigits();
  }
}
