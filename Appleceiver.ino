const int buttonPin = 4;
const int irPin = 5;
const int ledPin = 6;

//NFC IR receiver state machine
#define none      0
#define startLow  1
#define startHigh 2
#define bitLow    3
#define bitHigh   4
unsigned long startTime;
int rcvState = 0;
#define startLowMinT 8500 //us
#define startLowMaxT 9500
#define startHighMinT 4000
#define startHighMaxT 5000

unsigned long microsDelta(unsigned long last, unsigned long now);

void setup() {
  pinMode(buttonPin, INPUT);
  pinMode(irPin, INPUT);
  pinMode(ledPin, OUTPUT);
  Keyboard.begin();
}

void loop() {
  switch(rcvState) {
    case none:
      //if the input is low, start timing the start low pulse
      if (digitalRead(irPin) == LOW) {
        startTime = micros();
        rcvState++;
      }
      break;
    case startLow:
      //if the input is high, validate the length of the start low pulse and start timing the start high pulse
      if (digitalRead(irPin) == HIGH) {
        unsigned long duration = microsDelta(startTime, micros());
        if ((duration > startLowMinT) and (duration < startLowMaxT)) {
          startTime = micros();
          rcvState++;
        } else {
          //bad starting low pulse, abort
          rcvState = none;
        }
      }
      break;
    case startHigh:
      //if the input is low, validate the length of the start high pulse and start timing the first bit low pulse
      if (digitalRead(irPin) == LOW) {
        unsigned long duration = microsDelta(startTime, micros());
        if ((duration > startHighMinT) and (duration < startHighMaxT)) {
          startTime = micros();
          rcvState++;
          digitalWrite(ledPin, HIGH);
        } else {
          //bad starting high pulse, abort
          rcvState = none;
        }
      }

      break;
    case bitLow:
      break;
    case bitHigh:
      break;
  }
}

unsigned long microsDelta(unsigned long lastT, unsigned long nowT) {
  if (nowT > lastT) {
    return nowT - lastT;
  } else {
    return nowT - lastT + 4294967295;
  }
}
