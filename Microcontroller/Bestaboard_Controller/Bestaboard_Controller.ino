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
const int ALL_HOMED = 8; // All charachters are homed.
const int CHANGE_DELAY = 3;
const int DIAG_1 = 2;

//Constants
//byte STEPPER_SEQUENCE [8] = {0b0001,0b0011,0b0010,0b0110,0b0100,0b1100,0b1000,0b1001};
const byte c_STEPPER_SEQUENCE [8] = {0b1110,0b1100,0b1101,0b1001,0b1011,0b0011,0b0111,0b0110}; //rotates
//const byte c_SERVO_SEQUENCE [8] = {0b1111,0b1111,0b1111,0b1111,0b0000,0b0000,0b0000,0b0000};

const int c_STEPPER_COUNT = 2;
const int c_STEPPER_POS_COUNT = 4;
const int c_DELAY_SETTING [3] = {500000, 5000, 500};

const int c_DELAY_STARTUP = 2000; //Start up delay, called once in setup (ms).
const int c_DELAY_HOLD = 5000; //Hold time inbetween home and charachter modes (ms).

//Interrupt volatile variables
volatile int v_DELAY = c_DELAY_SETTING[0]; //Define time delay between pulses and servo sequence.

void setup() {
  // Configure output pins.
  pinMode(DATA, OUTPUT);
  pinMode(SHIFT, OUTPUT);
  pinMode(STROBE, OUTPUT);
  pinMode(HOME, OUTPUT);

  // Configure input pins.
  pinMode(ALL_HOMED, INPUT);

  // Configure interrupt pins.
  pinMode(CHANGE_DELAY, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CHANGE_DELAY), change_delay, FALLING);

  // Start up delay.
  delay(c_DELAY_STARTUP);

}

void loop() {


//Initalize STEPPER Outputs (0-15)
static byte STEPPER_OUT[c_STEPPER_COUNT] = {c_STEPPER_SEQUENCE[0],c_STEPPER_SEQUENCE[0]}; 
//for (int k = 1; k >= 0; k--){ 
//  SERVO_OUT[k] = c_SERVO_SEQUENCE[1];
//}

//Initialze STEPPER Sequence Numbers (1-8)
static byte STEPPER_SEQ[c_STEPPER_COUNT] = {0, 0};
//for (int k = 1; k >= 0; k--){ 
//  SERVO_SEQ[k]=1;
//}

//Initialze Servo Rotation Counts
static int STEPPER_ROT[c_STEPPER_COUNT] = {2048, 2048};
  //SERVO_ROT[1]=8;
  //SERVO_ROT[0]=4096;

//DIAGNOSTIC
static int i_all_homed;

//Count
static int Rotations;

static bool Stop_Rotate;
static bool Home_Char = 0; //Flip between rotating to home and a charachter. Home = 0, Charachter = 1 


Rotations = -1;
Stop_Rotate = 0;

while(Stop_Rotate == 0){
  
  // Read the state of the pushbuttons:
  i_all_homed = digitalRead(ALL_HOMED);
  DEBUG_PRINT("All Homed?:")
  DEBUG_PRINTLN(i_all_homed);

  //If homing, send out the home command.
  if(Home_Char == 0){
    digitalWrite(HOME, 1);
    DEBUG_PRINTLN("HOMING");
  }else{
    digitalWrite(HOME, 0);
    DEBUG_PRINTLN("CHARACHTER");
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

  // Once data has been updated in the shift registers, strobe to update outputs     
  pin_Pulse(STROBE, v_DELAY, HIGH);
  Rotations++;
  DEBUG_PRINT("Rotations:")
  DEBUG_PRINTLN(Rotations);

  // Default to stop rotation, override if continued rotation is necessary.
  Stop_Rotate = 1;

  // If more rotations are needed, update stepper outputs for next rotation
  for (int k = c_STEPPER_COUNT - 1; k >= 0; k--){

    if(Home_Char == 1)
    {
      // When doing charachter rotation, determine on a per charachter basis 
      if(Rotations < STEPPER_ROT[k]){
        STEPPER_SEQ[k] = stepper_Rotate(&STEPPER_OUT[k], STEPPER_SEQ[k], 0);
        Stop_Rotate = 0;
      }
    }else{
      // When homing, continue rotating until the all homed signal has been received.
      if(i_all_homed != 1)
      {
        STEPPER_SEQ[k] = stepper_Rotate(&STEPPER_OUT[k], STEPPER_SEQ[k], 0);
        Stop_Rotate = 0;
      }else{
        break;
      }
    }
  }

}

  //Switch modes and wait. 
  Home_Char = !Home_Char;
  delay(c_DELAY_HOLD);
}

byte stepper_Rotate(byte* STEPPER, byte SEQ, bool DIR){
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
void change_delay(){
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();

  if (interrupt_time - last_interrupt_time > 100) { // Debounce time of 50 milliseconds
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
