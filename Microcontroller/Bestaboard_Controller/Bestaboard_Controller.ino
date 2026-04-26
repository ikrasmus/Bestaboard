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
const int DATA = 12; //Serial Data
const int SHIFT = 11; //Clock for shift register shifting
const int STROBE = 10; //Latch shift register data
const int HOME = 9; //Home command.

//Define input locations
const int ALL_HOMED = 8; //All charac  ters are homed.
const int CHANGE_DELAY = 3;
const int DIAG_1 = 2;

//Constants
//Stepper Constants
const byte c_STEPPER_SEQUENCE [8] = {0b1110,0b1100,0b1101,0b1001,0b1011,0b0011,0b0111,0b0110}; 
const int c_STEPPER_COUNT = 2; //Number of stepper motors.
const int c_STEPPER_POS_COUNT = 4; //Number of stepper sequence positions.
const byte c_CHARS = 48; //Number of characters on the drum.
const char c_FLIPS[c_CHARS] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?-+=*$:;"; // Flip ordering on the drum.
const int c_STEP_ANGLES = 4096; // Number of step angles for the stepper, angle * gear ratio, 64 * 64 = 4096
const int c_STEP_TO_CHAR = c_STEP_ANGLES / c_CHARS; // Amount of steps to take to next charachter, 4096 / 48 = 85.33 (gets rounded down to 85)

//Delay timers
const int c_DELAY_SETTING [3] = {400, 4000, 400000};
const int c_DELAY_STARTUP = 2000; //Start up delay, called once in setup (ms).
const int c_DELAY_HOLD = 5000; //Hold time inbetween home and character modes (ms).

//Interrupt volatile variables
volatile int v_DELAY = c_DELAY_SETTING[0]; //Define time delay between pulses and servo sequence.

//---------------------SETUP---------------------//
void setup() {
  //Configure output pins.
  pinMode(DATA, OUTPUT);
  pinMode(SHIFT, OUTPUT);
  pinMode(STROBE, OUTPUT);
  pinMode(HOME, OUTPUT);

  //Initalize all pins as low.
  digitalWrite(DATA, LOW);
  digitalWrite(SHIFT, LOW);
  digitalWrite(STROBE, LOW);
  digitalWrite(HOME, LOW);

  //Configure input pins.
  pinMode(ALL_HOMED, INPUT);

  //Configure interrupt pins.
  pinMode(CHANGE_DELAY, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CHANGE_DELAY), change_Delay, FALLING);

  //Start up delay.
  delay(c_DELAY_STARTUP);

}

//---------------------LOOP---------------------//
void loop() {

//Initalize STEPPER Outputs (0-15)
static byte STEPPER_OUT[c_STEPPER_COUNT] = {c_STEPPER_SEQUENCE[0],c_STEPPER_SEQUENCE[0]}; //Stepper output, 4 bit value for each of the stepper coils.
static byte STEPPER_SEQ[c_STEPPER_COUNT] = {0, 0}; //Stepper sequence number (0-7).
static int STEPPER_ROT[c_STEPPER_COUNT] = {0,0}; //Stepper rotations required to get to the desired character. 


static int i_all_homed; //Homed signals are cascaded back towards controller, when all homed signal is received, every stepper is homed. 
static int Rotations; //Amount of rotations done during character rotation, used to determine when a flip stops rotation.
static bool Stop_Rotate; //Stops rotating when set to 1.
static bool Home_Char = 0; //Flip between rotating to home and a character. Home = 0, Character = 1 

// Hardcode in the desired characters.
STEPPER_ROT[0] = flip_Rotations('A');
STEPPER_ROT[1] = flip_Rotations('A');

// Initalize variables for next loop.
Rotations = -1; //Rotations starts at -1 as the first "rotation" is initalizing the steppers. 
Stop_Rotate = 0;

while(Stop_Rotate == 0){
  
  // Read the state of the pushbuttons:
  i_all_homed = digitalRead(ALL_HOMED);
  DEBUG_PRINT("All Homed?:")
  DEBUG_PRINTLN(i_all_homed);

  //If homing, send out the home command after rotating one character.
  if(Home_Char == 0){
    DEBUG_PRINTLN("HOMING");
    // Before sending the homing signal request a single characters worth of rotation
    // This is to address the problem of the first character still triggering the homing sensor.
    for (int k = c_STEPPER_COUNT - 1; k >= 0; k--){
      STEPPER_ROT[k] = flip_Rotations(c_FLIPS[0]);
    }

    if (Rotations > c_STEP_TO_CHAR){
      digitalWrite(HOME, 1);
    }
  }else{
    DEBUG_PRINTLN("CHARACHTER");
    digitalWrite(HOME, 0);
    
  }

  // Update data in shift register.
  // Loop for each stepper.
  for (int i = c_STEPPER_COUNT - 1; i >= 0; i--) {

    DEBUG_PRINT("Data Stepper Loop:")
    DEBUG_PRINTLN(i);
    DEBUG_PRINT("Data:");

    // Loop for each data point and send down the shift register, 4 data points per stepper.
    for(int j = c_STEPPER_POS_COUNT - 1; j >= 0; j--){
      digitalWrite(DATA, bitRead(STEPPER_OUT[i], j));
      pin_Pulse(SHIFT, v_DELAY, HIGH);

      DEBUG_PRINT(bitRead(STEPPER_OUT[i], j));
    }
    DEBUG_PRINTLN("");              
  }

  // Once data has been updated in the shift registers, strobe to update outputs.     
  pin_Pulse(STROBE, v_DELAY, HIGH);
  Rotations++;

  DEBUG_PRINT("Rotations:")
  DEBUG_PRINTLN(Rotations);

  // Default to stop rotation, override if continued rotation is necessary.
  Stop_Rotate = 1;

  // If more rotations are needed, update stepper sequence for next rotation
  for (int k = c_STEPPER_COUNT - 1; k >= 0; k--){

    if(Home_Char == 1)
    {
      // When doing charachter rotation, determine on a per charachter basis when to stop rotation.
      if(Rotations < STEPPER_ROT[k]){
        STEPPER_SEQ[k] = stepper_Seq_Rotate(&STEPPER_OUT[k], STEPPER_SEQ[k], 0);
        Stop_Rotate = 0;
      }
    } else {
      // When homing, guarantee one character's worth of rotation to get past the homing sensor,
      // otherwise continue rotating until the all homed signal has been received.
      if(Rotations < STEPPER_ROT[k] || i_all_homed != 1){
        STEPPER_SEQ[k] = stepper_Seq_Rotate(&STEPPER_OUT[k], STEPPER_SEQ[k], 0);
        Stop_Rotate = 0;
      } else {
        break;
      }
    }
  }

}

  //Switch modes and wait. 
  Home_Char = !Home_Char;
  delay(c_DELAY_HOLD);
}

//--------------------stepper_Rotate----------------------//
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

//--------------------rotations_Req----------------------//
// Calculate the amount of rotations required to reach a provided flip.
int flip_Rotations(char flip){
  int flip_Rotations = 0;
  for (int i = 0; i < c_CHARS; i++){
    if(flip == c_FLIPS[i]){
      flip_Rotations = (c_STEP_TO_CHAR * (i+1)) + round(i/3); // Handle whole and decimal parts individually. 
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

  if (interrupt_time - last_interrupt_time > 100) { // Debounce time.
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
