const int buttonPin = 4;
const int irPin = 5;
const int ledPin = 6;

//NEC IR receiver state machine
#define none      0
#define startLow  1
#define startHigh 2
#define bitLow    3
#define bitHigh   4
#define stopLow   5
#define repHigh   6
unsigned long startTime;
int rcvState = 0;
int rcvBit = 31;
unsigned int rcvMsgL;
unsigned int rcvMsgR;
boolean newMsg = false;
boolean tmpNewMsg = false;
boolean repMsg = false;
boolean tmpRepMsg = false;
#define startLowMinT 8800 //us
#define startLowMaxT 9200
#define startHighMinT 4300
#define startHighMaxT 4700
#define bitLowMinT 380
#define bitLowMaxT 780
#define bit0HighMinT 380
#define bit0HighMaxT 780
#define bit1HighMinT 1500
#define bit1HighMaxT 1900
#define repHighMinT 2000
#define repHighMaxT 2400

int repInstances = 0;
#define repHoldoff 4 //100ms each

#define appleAddress  0x77E1  
#define applePlus     0x509D
#define applePrevious 0x909D
#define applePlay     0xA09D
#define appleNext     0x609D
#define appleMinus    0x309D
#define appleMenu     0xC09D

//these might be teensy
#define plusAct     KEY_UP
#define previousAct KEY_LEFT
#define playAct     KEY_ENTER
#define nextAct     KEY_RIGHT
#define minusAct    KEY_DOWN
#define menuAct     KEY_BACKSPACE

#define ledPersist 75000 //us
int ledStarted = 0;

unsigned long microsDelta(unsigned long last, unsigned long now);
void decodeApple(unsigned int address, unsigned int message);

int lastRcvState = 99;

void setup() {
  pinMode(buttonPin, INPUT);
  pinMode(irPin, INPUT);
  pinMode(ledPin, OUTPUT);
  Keyboard.begin();
  Keyboard.set_modifier(0);
//  Serial.begin(9600);
}

void loop() {
  //state transition tracing
  /*if (rcvState != lastRcvState) {
    Serial.print(rcvState, DEC);
    Serial.print("s\n");
    lastRcvState = rcvState;
  }*/
  
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
          tmpNewMsg = false;
          tmpRepMsg = false;
          rcvState++;
        } else {
          //bad starting low pulse, abort
//          Serial.print(duration, DEC);
//          Serial.print(" BADSTARTL!\n");
          rcvState = none;
        }
      }
      break;
    case startHigh:
      //if the input is low, validate the length of the start high pulse and start timing the first bit low pulse, or detect a repet code and watch the stop low pulse
      if (digitalRead(irPin) == LOW) {
        unsigned long duration = microsDelta(startTime, micros());
        if ((duration > startHighMinT) and (duration < startHighMaxT)) {
          startTime = micros();
          rcvMsgL = 0;
          rcvMsgR = 0;
          rcvBit = 31;
          tmpNewMsg = true;
          rcvState++;
        } else if ((duration > repHighMinT) and (duration < repHighMaxT)) {
          tmpRepMsg = true;
          rcvState = stopLow;          
        } else {
          //bad starting high pulse, abort
//          Serial.print(duration, DEC);
//          Serial.print(" BADSTARTH!\n");
          rcvState = none;
        }
      }
      break;
    case bitLow:
      //if the input is high, validate the length of the bit low pulse and start timing the bit high pulse
      if (digitalRead(irPin) == HIGH) {
        unsigned long duration = microsDelta(startTime, micros());
        if ((duration > bitLowMinT) and (duration < bitLowMaxT)) {
          startTime = micros();
          rcvState++;
        } else {
          //bad bit low pulse, abort
//          Serial.print(duration, DEC);
//          Serial.print(" BADBITL!\n");
          rcvState = none;
        }
      }
      break;
    case bitHigh:
      //if the input is low, validate the length of the bit high pulse, determine the value and start timing the next bit low pulse if there is a next bit else watch the stop low pulse
      if (digitalRead(irPin) == LOW) {
        unsigned long duration = microsDelta(startTime, micros());
        if ((duration > bit0HighMinT) and (duration < bit0HighMaxT)) {
          startTime = micros();
          if (rcvBit--) {
            rcvState--;
          } else {
            rcvState++;
          }
        } else if ((duration > bit1HighMinT) and (duration < bit1HighMaxT)) {
          startTime = micros();
          rcvMsgR |= (1 << (rcvBit % 16));
          if (rcvBit--) {
            rcvState--;
          } else {
            rcvState++;
          }
        } else {
          //bad bit high pulse, abort
//          Serial.print(duration, DEC);
//          Serial.print(" BADBITH!\n");
          rcvState = none;
        }
        if (rcvBit == 15) {
          rcvMsgL = rcvMsgR;
          rcvMsgR = 0;
        }
      }
      break;
    case stopLow:
      //forget timing, just wait for the in put to go high
      if (digitalRead(irPin) == HIGH) {
        rcvState = none;
        newMsg = tmpNewMsg;
        repMsg = tmpRepMsg;
      }
      break;
  }
  
  if (newMsg == true) {
//    Serial.print(rcvMsgL, HEX);
//    Serial.print(rcvMsgR, HEX);
//    Serial.print("\n");
      decodeApple(rcvMsgL, rcvMsgR);
      newMsg = false;
    repInstances = 0;
  } else if (repMsg == true) {
    if (++repInstances < repHoldoff) {
//      Serial.print(rcvMsgL, HEX);
//      Serial.print(rcvMsgR, HEX);
//      Serial.print("n\n");
    } else {
      decodeApple(rcvMsgL, rcvMsgR);
//      Serial.print(rcvMsgL, HEX);
//      Serial.print(rcvMsgR, HEX);
//      Serial.print("r\n");
    }
    repMsg = false;
  }
  
  int difference = microsDelta(ledStarted, micros());
  if (difference > (unsigned long) ledPersist) {
    digitalWrite(ledPin, LOW);
  }  
}

unsigned long microsDelta(unsigned long lastT, unsigned long nowT) {
  if (nowT > lastT) {
    return nowT - lastT;
  } else {
    return nowT - lastT + 4294967295;
  }
}

void decodeApple(unsigned int address, unsigned int message) {
  if (address != appleAddress) {
    return;
  }
  switch (message) {
    case applePlus:
      Keyboard.set_key1(plusAct);
      break;
    case applePrevious:
      Keyboard.set_key1(previousAct);
      break;
    case applePlay:
      Keyboard.set_key1(playAct);
      break;
    case appleNext:
      Keyboard.set_key1(nextAct);
      break;
    case appleMinus:
      Keyboard.set_key1(minusAct);
      break;
    case appleMenu:
      Keyboard.set_key1(menuAct);
      break;
  }
  digitalWrite(ledPin, HIGH);
  ledStarted = micros();
  Keyboard.send_now();
  Keyboard.set_key1(0);
  Keyboard.send_now();
}
