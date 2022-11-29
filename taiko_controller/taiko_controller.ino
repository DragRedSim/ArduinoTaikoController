#include "AnalogReadNow.h"

//#define DEBUG_OUTPUT
//#define DEBUG_OUTPUT_LIVE
//#define DEBUG_TIME
//#define DEBUG_DATA

#define ENABLE_KEYBOARD
#define ENABLE_NS_JOYSTICK

#define HAS_KEY_MATRIX

#ifdef ENABLE_KEYBOARD
  #include <Keyboard.h>
#endif

#ifdef ENABLE_NS_JOYSTICK
  #include "Joystick.h"
  const int led_pin[4] = {10, 16, 14, 15}; //modified for Pro Micro
  const int piezo_output[4] = {SWITCH_BTN_ZL, SWITCH_BTN_LCLICK, SWITCH_BTN_RCLICK, SWITCH_BTN_ZR};
#endif

#ifdef HAS_KEY_MATRIX
  int button_state[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  int button_cooldown[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  #ifdef ENABLE_KEYBOARD
    const int button_key[16] = {
      KEY_UP_ARROW,   KEY_RIGHT_ARROW,  KEY_DOWN_ARROW, KEY_LEFT_ARROW,
      'k',            'j',              'f',            'd',
      KEY_PAGE_DOWN,  KEY_PAGE_UP,      KEY_ESC,        ' ',
      KEY_F1,         'q',              0 /*Fn1*/,      0 /*Fn2*/
    };
  #endif
  #ifdef ENABLE_NS_JOYSTICK
    const int button[16] = {
      0 /*SWITCH_HAT_U*/, 0 /*SWITCH_HAT_R*/, 0 /*SWITCH_HAT_D*/, 0 /*SWITCH_HAT_L*/,
      SWITCH_BTN_X,       SWITCH_BTN_A,       SWITCH_BTN_B,       SWITCH_BTN_Y,
      SWITCH_BTN_L,       SWITCH_BTN_R,       SWITCH_BTN_SELECT,  SWITCH_BTN_START,
      SWITCH_BTN_CAPTURE, SWITCH_BTN_HOME,    0 /*Fn1*/,          0 /*Fn2*/
    }; //rows to 0-3, cols to 4-7 (all Dpad to 0, all face to 1, etc)
    const int hat_mapping[16] = {
      SWITCH_HAT_CENTER,  SWITCH_HAT_U,       SWITCH_HAT_R,       SWITCH_HAT_UR,
      SWITCH_HAT_D,       SWITCH_HAT_CENTER,  SWITCH_HAT_DR,      SWITCH_HAT_R,     //5=U+D, 7=U+D+R
      SWITCH_HAT_L,       SWITCH_HAT_UL,      SWITCH_HAT_CENTER,  SWITCH_HAT_U,     //10=L+R, 11=L+R+U
      SWITCH_HAT_DL,      SWITCH_HAT_L,       SWITCH_HAT_D,       SWITCH_HAT_CENTER //13=U+D+L, 14=L+R+D, 15=all
    };
  #endif
#endif

const int min_threshold = 15;
const long cd_length = 10000;
const float k_threshold = 1.5;
const float k_decay = 0.97;

const int pin[4] = {A0, A3, A1, A2}; //Lka, Ldon, Rdon, Rka
const int key[4] = {'d', 'f', 'j', 'k'};
const float sens[4] = {1.0, 1.0, 1.0, 1.0};

const int key_next[4] = {3, 2, 0, 1}; //sets next piezo to check

const long cd_stageselect = 200000;

bool stageselect = false;
bool stageresult = false;

float threshold = 20;
int raw[4] = {0, 0, 0, 0};
float level[4] = {0, 0, 0, 0};
long cd[4] = {0, 0, 0, 0};
bool down[4] = {false, false, false, false};
#ifdef ENABLE_NS_JOYSTICK
  uint8_t down_count[4] = {0, 0, 0, 0};
#endif

typedef unsigned long time_t;
time_t t0 = 0;
time_t delta_time = 0, sdt = 0;

const int mode_pins[2] = {8, 9}; //what we can use for other purposes!

void setup() {
  analogReference(DEFAULT);
  analogSwitchPin(pin[0]);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  #ifdef ENABLE_NS_JOYSTICK
    for (int i = 0; i < 8; ++i) pinMode(i, INPUT_PULLUP);
    for (int i = 0; i < 4; ++i) {
      pinMode(led_pin[i], OUTPUT);
      digitalWrite(led_pin[i], HIGH);
    }
  #endif
  #ifdef ENABLE_KEYBOARD
    Keyboard.begin(); //enable reporting keyboard outputs
  #endif
  t0 = micros();
  Serial.begin(9600);
}

void sample() {
  int prev[4] = {raw[0], raw[1], raw[2], raw[3]};
  raw[0] = analogRead(pin[0]);
  raw[1] = analogRead(pin[1]);
  raw[2] = analogRead(pin[2]);
  raw[3] = analogRead(pin[3]);
  for (int i=0; i<4; ++i)
    level[i] = abs(raw[i] - prev[i]) * sens[i];
}

void sampleSingle(int i) {
  int prev = raw[i];
  raw[i] = analogReadNow();
  level[i] = abs(raw[i] - prev) * sens[i];
  analogSwitchPin(pin[key_next[i]]);
}

void parseSerial() {
  static char command = -1;
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (command == -1) //no command in buffer
      command = c;
    else { //something already here
      switch (command) {
      case 'C': //respond with next buffered command code
        Serial.write('C');
        Serial.write(c);
        Serial.flush();
        break;
      case 'S': //set internal LED to stageselect
        stageselect = (c == '1');
        digitalWrite(LED_BUILTIN, stageselect ? HIGH : LOW);
        break;
      case 'R': //set internal LED to stageresult
        stageresult = (c == '1');
        digitalWrite(LED_BUILTIN, stageresult ? HIGH : LOW);
        break;
      }
      command = -1;
    }
  }
}

void loop_test() {
  sampleSingle(0);
  Serial.print(level[0]);
  Serial.print("\t");
  delayMicroseconds(500);
  sampleSingle(1);
  Serial.print(level[1]);
  Serial.print("\t");
  delayMicroseconds(500);
  sampleSingle(2);
  Serial.print(level[2]);
  Serial.print("\t");
  delayMicroseconds(500);
  sampleSingle(3);
  Serial.print(level[3]);
  Serial.println();
  delayMicroseconds(500);
}

void loop_test2() {
  Serial.print(analogRead(pin[0]));
  Serial.print("\t");
  delayMicroseconds(500);
  Serial.print(analogRead(pin[1]));
  Serial.print("\t");
  delayMicroseconds(500);
  Serial.print(analogRead(pin[2]));
  Serial.print("\t");
  delayMicroseconds(500);
  Serial.print(analogRead(pin[3]));
  Serial.println();
  delayMicroseconds(500);
}

void loop() {
  //loop_test2(); return;
  
  static int piezo_index = 0;

  #ifdef ENABLE_KEYBOARD
    parseSerial();
  #endif
  
  time_t t1 = micros();
  delta_time = t1 - t0; //delta time against previous loop or setup()
  sdt += delta_time; //sum delta
  t0 = t1; //reset for next loop
  
  float prev_level = level[piezo_index]; //read single piezo
  sampleSingle(piezo_index);
  float new_level = level[piezo_index];
  level[piezo_index] = (level[piezo_index] + prev_level * 2) / 3; //store sum of sample and prior value, multiplied by 2/3; increases gain over natural rate
  
  threshold *= k_decay;

  for (int i = 0; i != 4; ++i) {
    if (cd[i] > 0) { //cooldown
      cd[i] -= delta_time; //subtract delta
      if (cd[i] <= 0) {
        cd[i] = 0; //clamp to positive
        if (down[i]) {
          #ifdef ENABLE_KEYBOARD
            Keyboard.release(stageresult ? KEY_ESC : key[i]);
          #endif
          down[i] = false;
        }
      }
    }
  }
  
  int i_max = 0;
  int level_max = 0;
  
  for (int i = 0; i != 4; ++i) { //check stored levels of ALL piezos
    if (level[i] > level_max && level[i] > threshold) {
      level_max = level[i]; //loudest seen
      i_max = i; //which sensor was it on?
    }
  }

  if (i_max == piezo_index && level_max >= min_threshold) { //if we're currently looking at the loudest piezo
    if (cd[i_max] == 0) { //cooldown
      if (!down[i_max]) { //if loudest piezo is not down
        #ifdef DEBUG_DATA
          Serial.print(level[0], 1);
          Serial.print("\t");
          Serial.print(level[1], 1);
          Serial.print("\t");
          Serial.print(level[2], 1);
          Serial.print("\t");
          Serial.print(level[3], 1);
          Serial.print("\n");
        #endif
        #ifdef ENABLE_KEYBOARD
          Keyboard.press(stageresult ? KEY_ESC : key[i_max]); //esc or loudest piezo key
        #endif
        down[i_max] = true;
        #ifdef ENABLE_NS_JOYSTICK
          if (down_count[i_max] <= 2) down_count[i_max] += 2; //add 2 to the hold-down cycle count
        #endif
      }
      for (int i = 0; i != 4; ++i)
        cd[i] = cd_length; //add a cooldown to all piezos
        #ifdef ENABLE_KEYBOARD
          if (stageselect)
            cd[i_max] = cd_stageselect; //replace main cooldown with stageselect cooldown
        #endif
    }
    sdt = 0; //reset sum of delta times
  }

  if (cd[i_max] > 0) { //if loudest piezo is on cooldown
    threshold = max(threshold, level_max * k_threshold); //raise threshold to decayed loudest value
  }
  
  static time_t cycle_time = 0;
  static int cycle_count = 0;
  cycle_time += delta_time;
  cycle_count += 1;

  #ifdef HAS_KEY_MATRIX
    // 4x4 button scan, one row per cycle
    static int matrix_index = 3; //set so that it will roll to 0 on first cycle
    pinMode(matrix_index+4, INPUT_PULLUP); //disable output on indexed pin in driven matrix
    matrix_index = ((matrix_index+1)&3); //choose next pin in low matrix aligned to 4
    pinMode(matrix_index+4, OUTPUT); 
    digitalWrite(matrix_index+4, LOW); //write LOW to indexed driven pin in matrix
      
    int state;
    int* bs = button_state + (matrix_index << 2); //pointer to every fourth item in button_state[]
    int* bc = button_cooldown + (matrix_index << 2); //pointer to related item in button_cooldown[]
    for (int i = 0; i < 4; ++i) { //i == low matrix
      state = (digitalRead(i) == LOW);
      //TODO: rewrite in parallel LEDs mode
      //digitalWrite(led_pin[i], state ? LOW : HIGH);
      if (bc[i] != 0) { //cooldown has a value
        bc[i] -= cycle_time; //subtract delta time
        if (bc[i] < 0) bc[i] = 0; //clamp to positive
      }
      if (state != bs[i] && bc[i] == 0) { //read state is different from stored, bc has expired
        bs[i] = state; //store key state
        bc[i] = 15000; //set cooldown to 15000, only updated when row is polled
        #ifdef ENABLE_KEYBOARD
          if (state) {
            Keyboard.press(button_key[(matrix_index << 2) + i]);
          } else {
            Keyboard.release(button_key[(matrix_index << 2) + i]);
          }
        #endif
      } //hold state until cooldown has expired, only updated when polling that row
      #ifdef ENABLE_NS_JOYSTICK
        Joystick.Button |= (bs[i] ? button[(matrix_index << 2) + i] : SWITCH_BTN_NONE);
        //update button map; if down, apply button bit; if up, make no changes (OR 0x0)
      #endif
  }
  #endif
    
  #ifdef ENABLE_NS_JOYSTICK
    //if cycle_time > 32000, OR if cycle_time >8000 and a sensor is tripped
    if (cycle_time > 32000 || (cycle_time > 8000 && (down_count[0] || down_count[1] || down_count[2] || down_count[3]))) {
      for (int i = 0; i < 4; ++i) { // Sensors
        bool state = (down_count[i] & 1); //state = lowest bit of appropriate down_count; inverts every cycle, so on-off-on-off-on-off. Allows us to "buffer" hits.
        Joystick.Button |= (state ? piezo_output[i] : SWITCH_BTN_NONE); //if state is true, trigger output
        down_count[i] -= !!down_count[i]; //if value, subtract 1; if not, don't do anything
        //TODO: rewrite in parallel LEDs mode
        //digitalWrite(led_pin[i], state ? LOW : HIGH);
      }
      #ifdef HAS_KEY_MATRIX
        int hat_state = 0; //renamed to avoid scoping issues against previous subroutine (handling hits on piezos)
        for (int i = 0; i < 4; ++i) { // Buttons emulating hat
          hat_state |= (button_state[i] ? 1 << i : 0); //pack buttons 0-3 into bitfield in order 3210
        }
        Joystick.HAT = hat_mapping[hat_state]; //array stores resolutions of mutually exclusive buttons
      #endif
      Joystick.sendState();
      Joystick.Button = SWITCH_BTN_NONE; //clear all buttons, the map will be rebuilt next cycle
      #ifdef DEBUG_TIME
        if (cycle_count > 0)
          Serial.println((float)cycle_time/cycle_count);
      #endif
      cycle_time = 0;
      cycle_count = 0;
    }
  #endif

  #ifdef DEBUG_OUTPUT
    static bool printing = false;
    #ifdef DEBUG_OUTPUT_LIVE
      if (true)
    #else
      if (printing || (/*down[0] &&*/ threshold > 10)) //disabled by default, but will start if threshold triggers
    #endif
      {
        printing = true;
        Serial.print(level[0], 1);
        Serial.print("\t");
        Serial.print(level[1], 1);
        Serial.print("\t");
        Serial.print(level[2], 1);
        Serial.print("\t");
        Serial.print(level[3], 1);
        Serial.print("\t| ");
        Serial.print(cd[0] == 0 ? "  " : down[0] ? "# " : "* ");
        Serial.print(cd[1] == 0 ? "  " : down[1] ? "# " : "* ");
        Serial.print(cd[2] == 0 ? "  " : down[2] ? "# " : "* ");
        Serial.print(cd[3] == 0 ? "  " : down[3] ? "# " : "* ");
        Serial.print("|\t");
        Serial.print(threshold, 1);
        Serial.println();
        if(threshold <= 5){
          Serial.println();
          printing = false; //do not re-enable until threshold > 10
        }
      } 
  #endif

  level[piezo_index] = new_level; //set stored level of piezo checked this loop
  piezo_index = key_next[piezo_index]; //which piezo to check next; order is LK, RK, LD, RD

  long ddt = 300 - (micros() - t0); //300 less delta of now and start of loop
  if(ddt > 3) delayMicroseconds(ddt);  //delay to start next loop at ~300ms after last loop
}
