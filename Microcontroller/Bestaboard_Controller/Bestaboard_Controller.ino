#include <math.h>;

#define DEBUG 0 // Set to 0 to disable debug output

#if DEBUG
#define DEBUG_PRINT(x) Serial.print(x);
#define DEBUG_PRINTLN(x) Serial.println(x);
#define DEBUG_BEGIN(x) Serial.begin(x);
#else
#define DEBUG_PRINT(x);
#define DEBUG_PRINTLN(x);
#define DEBUG_BEGIN(x);
#endif

//Define output locations
#define o_DATA 12 //Serial Data
#define o_SHIFT 11 //Clock for shift register shifting
#define o_STROBE 10 //Latch shift register data
#define o_HOME  9 //Home command.

//Define input locations
#define i_ALL_HOMED 8 //All characters are homed.
#define i_FLIP_OFFSET_4 7 //Flip offset switch #4.
#define i_FLIP_OFFSET_3 6 //Flip offset switch #3.
#define i_FLIP_OFFSET_2 5 //Flip offset switch #2.
#define i_FLIP_OFFSET_1 4 //Flip offset switch #1.
#define i_CHANGE_DELAY 3
const int DIAG_1 = 2;

//Constants
//Stepper Constants
const byte c_STEPPER_SEQUENCE [8] = {0b1110,0b1100,0b1101,0b1001,0b1011,0b0011,0b0111,0b0110};
const int c_STEPPER_COUNT = 4; //Number of stepper motors.
const int c_STEPPER_POS_COUNT = 4; //Number of stepper sequence positions.
const byte c_CHARS = 48; //Number of characters on the drum.
const char c_FLIPS[c_CHARS] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?-+=*$:;"; //Flip ordering on the drum.
const int c_STEP_ANGLES = 4096; // Number of step angles for the stepper, angle * gear ratio, 64 * 64 = 4096
const int c_STEP_TO_CHAR = c_STEP_ANGLES / c_CHARS; // Amount of steps to take to next charachter, 4096 / 48 = 85.33 (gets rounded down to 85)
const char c_WORDS[2][c_STEPPER_COUNT+1] = {
  {"YALE"},
  {"AIDC"}
};

//Delay timers
const int c_DELAY_SETTING [3] = {100, 150, 200};
const int c_DELAY_STARTUP = 2000; //Start up delay, called once in setup (ms).
const int c_DELAY_HOLD = 5000; //Hold time inbetween home and character modes (ms).

//Interrupt volatile variables
volatile int v_DELAY = c_DELAY_SETTING[0]; //Define time delay between pulses and servo sequence.

//---------------------SETUP---------------------//
void setup() {
  //Configure output pins.
  pinMode(o_DATA, OUTPUT);
  pinMode(o_SHIFT, OUTPUT);
  pinMode(o_STROBE, OUTPUT);
  pinMode(o_HOME, OUTPUT);

  //Initalize all output pins as low.
  digitalWrite(o_DATA, LOW);
  digitalWrite(o_SHIFT, LOW);
  digitalWrite(o_STROBE, LOW);
  digitalWrite(o_HOME, LOW);

  //Configure input pins.
  pinMode(i_ALL_HOMED, INPUT);
  pinMode(i_FLIP_OFFSET_4, INPUT_PULLUP);
  pinMode(i_FLIP_OFFSET_3, INPUT_PULLUP);
  pinMode(i_FLIP_OFFSET_2, INPUT_PULLUP);
  pinMode(i_FLIP_OFFSET_1, INPUT_PULLUP);

  //Configure interrupt pins.
  pinMode(i_CHANGE_DELAY, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(i_CHANGE_DELAY), change_Delay, FALLING);
  
  //Start up delay.
  delay(c_DELAY_STARTUP);

}

//---------------------LOOP---------------------//
void loop() {

//Initalize STEPPER data
static byte STEPPER_OUT[c_STEPPER_COUNT] = {c_STEPPER_SEQUENCE[0],c_STEPPER_SEQUENCE[0],c_STEPPER_SEQUENCE[0],c_STEPPER_SEQUENCE[0]}; //Stepper output, 4 bit value for each of the stepper coils.
static byte STEPPER_SEQ[c_STEPPER_COUNT] = {0, 0, 0, 0}; //Stepper sequence number (0-7).
static int STEPPER_ROT[c_STEPPER_COUNT] = {0, 0, 0, 0}; //Stepper rotations required to get to the desired character. 

//Input variables
static int i_all_homed; //Homed signals are cascaded back towards controller, when all homed signal is received, every stepper is homed.
static int i_flip_offset_4; //Flip offset switch #4, interally pulled high, when low add weighted offset to flip rotations.
static int i_flip_offset_3; //Flip offset switch #3, interally pulled high, when low add weighted offset to flip rotations.
static int i_flip_offset_2; //Flip offset switch #2, interally pulled high, when low add weighted offset to flip rotations.
static int i_flip_offset_1; //Flip offset switch #1, interally pulled high, when low add weighted offset to flip rotations.
const int flip_offset_weights[4] = {5,10,20,40}; //weights applied to flip offset switches.

//Variables
static int flip_offset = 0; //Adjustment to center on the selected character.
static int Rotations; //Amount of rotations done during character rotation, used to determine when a flip stops rotation.
static bool Stop_Rotate; //Stops rotating when set to 1.
static bool Home_Char = 0; //Flip between rotating to home and a character. Home = 0, Character = 1 
static int word_loop = 0; //Track the amount of times the function has looped while going to a word, used to switch words and identify a first pass.
static int j_word = 0; //Index for the active word.

// Initalize variables for next loop.
Rotations = -1; //Rotations starts at -1 as the first "rotation" is initalizing the steppers. 
Stop_Rotate = 0;

while(Stop_Rotate == 0){
  
  //Read Inputs
  i_flip_offset_4 = digitalRead(i_FLIP_OFFSET_4);
  i_flip_offset_3 = digitalRead(i_FLIP_OFFSET_3);
  i_flip_offset_2 = digitalRead(i_FLIP_OFFSET_2);
  i_flip_offset_1 = digitalRead(i_FLIP_OFFSET_1);
  i_all_homed = digitalRead(i_ALL_HOMED);

  DEBUG_PRINT("All Homed?:")
  DEBUG_PRINTLN(i_all_homed);

  //Calculate flip offset applied to every character.
  flip_offset = -1 * (!i_flip_offset_4*flip_offset_weights[3] + !i_flip_offset_3*flip_offset_weights[2] 
                    + !i_flip_offset_2*flip_offset_weights[1] + !i_flip_offset_1*flip_offset_weights[0]);

  //If homing, send out the home command after rotating one character.
  if(Home_Char == 0){
    DEBUG_PRINTLN("HOMING");
    //Before sending the homing signal request a single characters worth of rotation
    //This is to address the problem of the first character still triggering the homing sensor.
    //For the first pass of the loop don't, this allows the machine to power up at home, without moving off it.
    if(word_loop > 0){
      for (int k = c_STEPPER_COUNT - 1; k >= 0; k--){
        STEPPER_ROT[k] = flip_Rotations(c_FLIPS[0], 0);
      }

      if (Rotations > c_STEP_TO_CHAR){
        digitalWrite(o_HOME, 1);
      }
    }else{
      digitalWrite(o_HOME, 1);  
    }
  }else{
    DEBUG_PRINTLN("CHARACTER");

    j_word = word_loop % 2; //Select active word based on word loop.

    //Fill in STEPPER_ROT with the amount of flips required for each character to make the desired word.
    for (int k = c_STEPPER_COUNT - 1; k >= 0; k--){
      STEPPER_ROT[k] = flip_Rotations(c_WORDS[j_word][k], flip_offset);
    }

    digitalWrite(o_HOME, 0);
    
  }

  // Update data in shift register.
  // Loop for each stepper.
  for (int i = c_STEPPER_COUNT - 1; i >= 0; i--) {

    // Loop for each data point and send down the shift register, 4 data points per stepper.
    for(int j = c_STEPPER_POS_COUNT - 1; j >= 0; j--){
      digitalWrite(o_DATA, bitRead(STEPPER_OUT[i], j));
      pin_Pulse(o_SHIFT, v_DELAY, HIGH);

      DEBUG_PRINT(bitRead(STEPPER_OUT[i], j));
    }
    DEBUG_PRINTLN("");              
  }

  // Once data has been updated in the shift registers, strobe to update outputs.     
  pin_Pulse(o_STROBE, v_DELAY, HIGH);
  Rotations++;

  DEBUG_PRINT("Rotations:")
  DEBUG_PRINTLN(Rotations);

  // Default to stop rotation, override if continued rotation is necessary.
  Stop_Rotate = 1;

  // If more rotations are needed, update stepper sequence for next rotation
  for (int k = c_STEPPER_COUNT - 1; k >= 0; k--){

    if(Home_Char == 1)
    {
      // When doing character rotation, determine on a per charachter basis when to stop rotation.
      if(Rotations < STEPPER_ROT[k]){
        STEPPER_SEQ[k] = stepper_Seq_Rotate(&STEPPER_OUT[k], STEPPER_SEQ[k], 1);
        Stop_Rotate = 0;
      }
    } else {
      // When homing, guarantee one character's worth of rotation to get past the homing sensor,
      // otherwise continue rotating until the all homed signal has been received.
      if(Rotations < STEPPER_ROT[k] || i_all_homed != 1){
        STEPPER_SEQ[k] = stepper_Seq_Rotate(&STEPPER_OUT[k], STEPPER_SEQ[k], 1);
        Stop_Rotate = 0;
      } else {
        break;
      }
    }
  }

}

  //If a word has been completed, increment the loop, excluding incrementing when homing.
  if(Home_Char == 1){
    word_loop++;
  }

  //Switch modes and wait. 
  Home_Char = !Home_Char;
  delay(c_DELAY_HOLD);

}

//--------------------stepper_Seq_Rotate----------------------//
// Return the next stepper sequence.
byte stepper_Seq_Rotate(byte* STEPPER, byte SEQ, bool DIR){
  byte SEQ_NEXT;

  if(DIR){
    if(SEQ == 7){
      SEQ_NEXT = 0;
    }else{
      SEQ_NEXT = SEQ + 1;
    }
  }else{
    if(SEQ == 0){
      SEQ_NEXT = 7;
    }else{
      SEQ_NEXT = SEQ - 1;
    }
  }

  *STEPPER = c_STEPPER_SEQUENCE[SEQ_NEXT];
return SEQ_NEXT;
}

//--------------------flip_Rotations----------------------//
// Calculate the amount of rotations required to reach a provided flip character.
int flip_Rotations(char flip, int offset){
  int flip_Rotations = 0;
  for (int i = 0; i < c_CHARS; i++){
    if(flip == c_FLIPS[i]){
      flip_Rotations = (c_STEP_TO_CHAR * (i+1)) + round((i+1)/3) + offset; // Handle whole and decimal parts individually, allow for an offset. 
      break;
    }
  }
  return flip_Rotations;
}

//--------------------pin_Pulse----------------------//
// Pulse a value on a pin for a given delay.
void pin_Pulse(int pin, int delay, bool high){

  if(high == 1){
    digitalWrite(pin, HIGH);  
    delayMicroseconds(delay);
    digitalWrite(pin, LOW);
    delayMicroseconds(delay);   
  }else{
    digitalWrite(pin, LOW);  
    delayMicroseconds(delay);
    digitalWrite(pin, HIGH);
    delayMicroseconds(delay); 
  }

  return;
}

//--------------------Interrupts----------------------//
//--------------------change_Delay----------------------//
void change_Delay(){
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();

  if (interrupt_time - last_interrupt_time > 50) { // Debounce time.
      if (v_DELAY == c_DELAY_SETTING[0]){
        v_DELAY = c_DELAY_SETTING[1];
      }else if(v_DELAY == c_DELAY_SETTING[1]) {
        v_DELAY = c_DELAY_SETTING[2];
      }else{
        v_DELAY = c_DELAY_SETTING[0];  
      }
  }
  last_interrupt_time = interrupt_time;
}
