#define IR_PIN  5
#define LED_PIN 6

//NEC IR receiver state machine
//State machine states
#define NONE_HIGH       0
#define START_LOW       1
#define START_HIGH      2
#define BIT_LOW         3
#define BIT_HIGH        4
#define STOP_LOW        5
#define REPEAT_HIGH     6
//variables and flags
unsigned long start_time;
int rcv_state = NONE_HIGH;
int rcv_bit = 31;
unsigned int rcv_msg_l;
unsigned int rcv_msg_r;
boolean new_msg = false;
boolean tmp_new_msg = false;
boolean rep_msg = false;
boolean tmp_rep_msg = false;
//timing constraints
#define START_LOW_MIN_T         8800 //us
#define START_LOW_MAX_T         9200
#define START_HIGH_MIN_T        4300
#define START_HIGH_MAX_T        4700
#define BIT_LOW_MIN_T           380
#define BIT_LOW_MAX_T           780
#define BIT_0_HIGH_MIN_T        380
#define BIT_0_HIGH_MAX_T        780
#define BIT_1_HIGH_MIN_T        1500
#define BIT_1_HIGH_MAX_T        1900
#define REP_HIGH_MIN_T          2000
#define REP_HIGH_MAX_T          2400

//repeat code handler definitions
int rep_instances = 0;
#define REP_HOLDOFF 4 //100ms each

//Apple remote definitions
//Address
#define A1156_ADDRESS   0x77E1  
//Button codes
#define A1156_PLUS      0x509D
#define A1156_PREVIOUS  0x909D
#define A1156_PLAY      0xA09D
#define A1156_NEXT      0x609D
#define A1156_MINUS     0x309D
#define A1156_MENU      0xC09D

//Key assignments, these might be teensy only
#define PLUS_ACT        KEY_UP
#define PREVIOUS_ACT    KEY_LEFT
#define PLAY_ACT        KEY_ENTER
#define NEXT_ACT        KEY_RIGHT
#define MINUS_ACT       KEY_DOWN
#define MENU_ACT        KEY_BACKSPACE

//recevie indicator LED definitions
#define LED_PERSIST     50000 //us
unsigned long led_started = 0;

//X kill feature
#define X_KILL_CONTROLLER  0x77E1
#define X_KILL_BUTTON      0xC09D
#define X_KILL_TIMEOUT     150000 //150ms (100ms repeat on controller)
#define X_KILL_DURATION    100 //10s with 100ms repeat
int x_kill_pending       = 0;
unsigned long x_kill_started = 0;

//prototypes
unsigned long micros_delta(unsigned long last_t, unsigned long now_t);
void decode_apple(unsigned int address, unsigned int message);

void setup() {
  pinMode(IR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  Keyboard.begin();
  Keyboard.set_modifier(0);
  Serial.begin(9600);
}

void loop() {
  
  switch(rcv_state) {
    case NONE_HIGH:
      /*if the input is low, start timing the start low pulse*/
      if (digitalRead(IR_PIN) == LOW) {
        start_time = micros();
        rcv_state = START_LOW;
      }
      break;
    case START_LOW:
      /*if the input is high, validate the length of the start low
       pulse and start timing the start high pulse.*/
      if (digitalRead(IR_PIN) == HIGH) {
        unsigned long duration = micros_delta(start_time, micros());
        if ((duration > START_LOW_MIN_T) and (duration < START_LOW_MAX_T)) {
          start_time = micros();
          tmp_new_msg = false;
          tmp_rep_msg = false;
          rcv_state = START_HIGH;
        } else {
          //bad starting low pulse, abort
          rcv_state = NONE_HIGH;
        }
      }
      break;
    case START_HIGH:
      /*if the input is low, validate the length of the start high
       pulse and start timing the first bit low pulse, or detect a
       repet code and watch the stop low pulse.*/
      if (digitalRead(IR_PIN) == LOW) {
        unsigned long duration = micros_delta(start_time, micros());
        if ((duration > START_HIGH_MIN_T) and (duration < START_HIGH_MAX_T)) {
          start_time = micros();
          rcv_msg_l = 0;
          rcv_msg_r = 0;
          rcv_bit = 31;
          tmp_new_msg = true;
          rcv_state = BIT_LOW;
        } else if ((duration > REP_HIGH_MIN_T) and (duration < REP_HIGH_MAX_T)) {
          tmp_rep_msg = true;
          rcv_state = STOP_LOW;          
        } else {
          //bad starting high pulse, abort
          rcv_state = NONE_HIGH;
        }
      }
      break;
    case BIT_LOW:
      /*if the input is high, validate the length of the bit low pulse
       and start timing the bit high pulse.*/
      if (digitalRead(IR_PIN) == HIGH) {
        unsigned long duration = micros_delta(start_time, micros());
        if ((duration > BIT_LOW_MIN_T) and (duration < BIT_LOW_MAX_T)) {
          start_time = micros();
          rcv_state = BIT_HIGH;
        } else {
          //bad bit low pulse, abort
          rcv_state = NONE_HIGH;
        }
      }
      break;
    case BIT_HIGH:
      /*if the input is low, validate the length of the bit high pulse,
       determine the value and start timing the next bit low pulse if there
       is a next bit else watch the stop low pulse.*/
      if (digitalRead(IR_PIN) == LOW) {
        unsigned long duration = micros_delta(start_time, micros());
        if ((duration > BIT_0_HIGH_MIN_T) and (duration < BIT_0_HIGH_MAX_T)) {
          start_time = micros();
          if (rcv_bit--) {
            rcv_state = BIT_LOW;
          } else {
            rcv_state = STOP_LOW;
          }
        } else if ((duration > BIT_1_HIGH_MIN_T) and (duration < BIT_1_HIGH_MAX_T)) {
          start_time = micros();
          rcv_msg_r |= (1 << (rcv_bit % 16));
          if (rcv_bit--) {
            rcv_state = BIT_LOW;
          } else {
            rcv_state = STOP_LOW;
          }
        } else {
          //bad bit high pulse, abort
          rcv_state = NONE_HIGH;
        }
        if (rcv_bit == 15) {
          rcv_msg_l = rcv_msg_r;
          rcv_msg_r = 0;
        }
      }
      break;
    case STOP_LOW:
      /*Forget timing, just wait for the in put to go high*/
      if (digitalRead(IR_PIN) == HIGH) {
        rcv_state = NONE_HIGH;
        new_msg = tmp_new_msg;
        rep_msg = tmp_rep_msg;
      }
      break;
  }
  
  if (new_msg == true) {
      decode_apple(rcv_msg_l, rcv_msg_r);
      new_msg = false;
    rep_instances = 0;
  } else if (rep_msg == true) {
    if (++rep_instances < REP_HOLDOFF) {
    } else {
      decode_apple(rcv_msg_l, rcv_msg_r);
      decode_x_kill(rcv_msg_l, rcv_msg_r);
    }
    rep_msg = false;
  }
  
  //turn the LED off if it's been long enough
  if (led_started) {
    unsigned long difference = micros_delta(led_started, micros());
    if (difference > LED_PERSIST) {
      digitalWrite(LED_PIN, LOW);
      led_started = 0;
    }
  }
  
  //Forget X kill if it's been long enough (user let go of button)
  if (x_kill_pending) {
    unsigned long difference = micros_delta(x_kill_started, micros());
    if (difference > X_KILL_TIMEOUT) {
      x_kill_pending = 0;
    }
  }
}

unsigned long micros_delta(unsigned long last_t, unsigned long now_t) {
  if (now_t > last_t) {
    return now_t - last_t;
  }
  //micros() wrapped, happens ~ every 70 mins
  return now_t - last_t + 4294967295;
}

void decode_apple(unsigned int address, unsigned int message) {
  if (address != A1156_ADDRESS) {
    return;
  }
  switch (message) {
    case A1156_PLUS:
      Keyboard.set_key1(PLUS_ACT);
      break;
    case A1156_PREVIOUS:
      Keyboard.set_key1(PREVIOUS_ACT);
      break;
    case A1156_PLAY:
      Keyboard.set_key1(PLAY_ACT);
      break;
    case A1156_NEXT:
      Keyboard.set_key1(NEXT_ACT);
      break;
    case A1156_MINUS:
      Keyboard.set_key1(MINUS_ACT);
      break;
    case A1156_MENU:
      Keyboard.set_key1(MENU_ACT);
      break;
  }
  digitalWrite(LED_PIN, HIGH);
  led_started = micros();
  Keyboard.send_now();
  Keyboard.set_key1(0);
  Keyboard.send_now();
  return;
}

void decode_x_kill(unsigned int address, unsigned int message) {
  if ((address == X_KILL_CONTROLLER) and (message == X_KILL_BUTTON)) {
    x_kill_started = micros();
    x_kill_pending++;
  }
  if (x_kill_pending > X_KILL_DURATION) {
    //KILL X NOW
    Keyboard.set_modifier(MODIFIERKEY_CTRL | MODIFIERKEY_ALT);
    Keyboard.set_key1(KEY_BACKSPACE);
    Keyboard.send_now();
    Keyboard.set_modifier(0);
    Keyboard.set_key1(0);
    Keyboard.send_now();
    x_kill_pending = 0;
  }
}
