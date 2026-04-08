//#include <AccelStepper.h>
//#include <MultiStepper.h>

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
//byte SERVO_SEQUENCE [8] = {0b0001,0b0011,0b0010,0b0110,0b0100,0b1100,0b1000,0b1001};
const byte c_SERVO_SEQUENCE [8] = {0b1110,0b1100,0b1101,0b1001,0b1011,0b0011,0b0111,0b0110};
//byte SERVO_SEQUENCE3 [4] = {0b0011,0b0101,0b1100,0b1010};
//byte SERVO_SEQUENCE4 [4] = {0b1110,0b1101,0b1011,0b0111};
const int c_SERVO_COUNT = 2;
const int c_SERVO_POS_COUNT = 4;
const int c_DELAY_SETTING [3] = {50000, 5000, 1000};

//Interrupt variables
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
}

void loop() {

//Initalize Servo Outputs (0-15)
byte SERVO_OUT[2]; 
//for (int k = 1; k >= 0; k--){ 
//  SERVO_OUT[k]=0;
//}

//Initialze Servo Sequence Numbers (1-8)
byte SERVO_SEQ[2];
//for (int k = 1; k >= 0; k--){ 
//  SERVO_SEQ[k]=1;
//}

//Initialze Servo Rotation Counts
int SERVO_ROT[2] = {2,2};
  //SERVO_ROT[1]=8;
  //SERVO_ROT[0]=4096;

//DIAGNOSTIC
int i_all_homed;

//Count
int Rotations = 0;

bool Stop_Rotate = 0;
bool Home_Char = 0; //Flip between rotating to home and a charachter. Home = 0, Charachter = 1 

while(Stop_Rotate == 0){
  
  // Read the state of the pushbuttons:
  i_all_homed = digitalRead(ALL_HOMED);

  //If homing, send out the home command.
  //if(Home_Char == 0){
    digitalWrite(HOME, 1);
  //}else{
  //  digitalWrite(HOME, 0);
  //}

  // Update data in shift register.
  // Loop for each servo.
  for (int i = c_SERVO_COUNT - 1; i >= 0; i--) {

    // Loop for each data point and send down the shift register, 4 data points per servo.
    for(int j = c_SERVO_POS_COUNT - 1; j >= 0; j--){
      digitalWrite(DATA, bitRead(SERVO_OUT[i], j));
      pin_Pulse(SHIFT, v_DELAY, HIGH);
    }              
  }

  // Once data has been updated in the shift registers, strobe to update outputs     
  pin_Pulse(STROBE, v_DELAY, HIGH);
  Rotations++;

  // Default to stop rotation, override if continued rotation is necessary.
  Stop_Rotate = 1;

  // If more rotations are needed, update servo outputs for next rotation
  for (int k = c_SERVO_COUNT - 1; k >= 0; k--){

    if(Home_Char == 1)
    {
      // When doing charachter rotation, determine on a per charachter basis 
      if(Rotations < SERVO_ROT[k]){
        SERVO_SEQ[k] = servo_Rotate(&SERVO_OUT[k], SERVO_SEQ[k], 1);
        Stop_Rotate = 0;
      }
    }else{
      // When homing, continue rotating until the all homed signal has been received.
      if(i_all_homed != 1)
      {
        Rotations = 0;
        Stop_Rotate = 0;
      }else{
        break;
      }
      SERVO_SEQ[k] = servo_Rotate(&SERVO_OUT[k], SERVO_SEQ[k], 1);
    }
  }
}

//delay(5000);
}

byte servo_Rotate(byte* SERVO, byte SEQ, bool DIR){
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

  *SERVO = c_SERVO_SEQUENCE[SEQ_NEXT];
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

  if (v_DELAY == c_DELAY_SETTING[0]){
    v_DELAY = c_DELAY_SETTING[1];
  }else if(v_DELAY == c_DELAY_SETTING[1]) {
    v_DELAY = c_DELAY_SETTING[2];
  }else{
    v_DELAY = c_DELAY_SETTING[0];  
  }

}