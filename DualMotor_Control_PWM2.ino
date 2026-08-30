/****************************************************************************************************************************
  File: DualMotor_Control_PWM2.ino\
  By: Bryan Chan
  Modified from: Argument_Simple.ino Written by Khoi Hoang

  Requires TimerInterrupt 1.5.0 by Khoi Hoang
  Built by Khoi Hoang https://github.com/khoih-prog/TimerInterrupt
  Licensed under MIT license

 *****************************************************************************************************************************/

// These define's must be placed at the beginning before #include "TimerInterrupt.h"
// _TIMERINTERRUPT_LOGLEVEL_ from 0 to 4
// Don't define _TIMERINTERRUPT_LOGLEVEL_ > 0. Only for special ISR debugging only. Can hang the system.
#define TIMER_INTERRUPT_DEBUG         0
#define _TIMERINTERRUPT_LOGLEVEL_     0
#define USE_TIMER_1     false
#define USE_TIMER_2     true
#define USE_TIMER_3     false
#include "TimerInterrupt.h"

#if !defined(LED_BUILTIN)
  #define LED_BUILTIN     13
#endif
/* Battery voltage sense is scaled by 0.2123
 * ADC is 10bit so 20V is ~4.25V or 868 counts
 * 16V is ~3.39V or 695 counts
 * 
 */
#define LED1 2
#define LED2 4
#define SW1  3
#define Throttle A0
#define Throttle_SW 12
#define Shifter 13
#define DIR1 7
#define DIR2 8
#define PWM1 9
#define PWM2 10
#define Battery A2
#define ON 1
#define OFF 0
#define FWD 1
#define RVS 0
#define LowBatt 695
#define FET_Temp A1
#define Battery_Temp A3

char LowBatt_Test = 5;
unsigned int BattV_Count = 0;
char Debug_Count = 0;
char Debug_mode = false;
char Updated = false;
char LED_counter = 0;
char Throttle_Engaged = 0;
char Shifter_Dir = 1;
char PWM_Val = 0;
char PWM_Delay = 0;
unsigned int Throttle_Pos = 0;
char Prev_Dir = 1;
unsigned int outputPin1 = LED_BUILTIN;
unsigned int Drive_Temp  = 0;
unsigned int Batt_Temp = 0;

#define TIMER_INTERVAL_MS    100

void TimerHandler(void)
{
  Prev_Dir = Shifter_Dir;
  //static char PWM_Delay = 0;
  //static char PWM_Val = 0;
  // Every 100ms, this timer ISR will run to sample all inputs
  // and set the throttle output. Sampling ADC takes about ~100us
  // while it isn't best practice for ISR to take too long, this
  // is the only ISR for now so it shouldn't cause issues for now

  // 3 second count for blinking LED pattern from main
  if (LED_counter == 30)
    LED_counter = 0;
  else
    LED_counter++;

  Shifter_Dir = digitalRead(Shifter); // Switch is active high, FWD=high
  Throttle_Engaged = !(digitalRead(Throttle_SW)); // Switch is active low
  // Run motors if battery voltage is high enough
  // Battery test done in main loop after every ISR
  if((LowBatt_Test != 10) && (Throttle_Engaged))
  {
    Throttle_Pos = analogRead(Throttle);
    // Throttle voltage is between 1.5V to 3.5V
    // ADC value (assuming 5V ref) is 307 to 716
    if(Throttle_Pos<307)
      Throttle_Pos = 307; // Ignore low throttle that risk stalling the motor
    if(Throttle_Pos>716)
      Throttle_Pos = 716; // Cap value above  
    // use the MAP() function to get PWM value between 25%-100% (63-255)
    // that means motor will only brake when pedal is released  
    PWM_Val = map(Throttle_Pos, 307, 716, 63, 245);
    if(PWM_Delay==0)
    {
      //Set PWM outputs
      analogWrite(PWM1, PWM_Val);
      analogWrite(PWM2, PWM_Val);
    }
    else
      PWM_Delay--; // Wait to apply throttle
  }
  else
  {
    // Stop all drive controls
    analogWrite(PWM1, 0);
    analogWrite(PWM2, 0);
    // clear throttle position running average
  }

  // Changing drive direction need to start with stopping motors first
  if(Shifter_Dir != Prev_Dir)
  {
    if(Throttle_Engaged)
    {
      analogWrite(PWM1, 0);
      analogWrite(PWM2, 0);
      PWM_Delay = 10; // wait 1 second before enabling PWM again
    }
    
    if(Shifter_Dir)
    {
      digitalWrite(DIR1, FWD);
      digitalWrite(DIR2, FWD);
    }
    else
    {
      digitalWrite(DIR1, RVS);
      digitalWrite(DIR2, RVS);
    }
  }
  

  Updated = true;
  //Serial.print(F("TimerCount = ")); Serial.println(LED_counter);
}

void setup()
{
  Serial.begin(115200);
  while (!Serial);  // Always valid in UNO

  Serial.print(F("\nStarting Motor Control PWM on "));
  Serial.println(BOARD_TYPE);
  Serial.println(TIMER_INTERRUPT_VERSION);
  Serial.print(F("CPU Frequency = ")); Serial.print(F_CPU / 1000000); Serial.println(F(" MHz"));

  // Initialize outputs to brake state
  digitalWrite(PWM1, 0);
  digitalWrite(PWM2, 0);
  digitalWrite(DIR1, 1);
  digitalWrite(DIR2, 1);
  // Read battery voltage is not drained
  // low battery will be trapped in this loop
  // unless SW1 is held down
  do{
    digitalWrite(LED1, ON);
    delay(200);  // 200ms
    BattV_Count = analogRead(Battery); // ~100us
    if((BattV_Count > LowBatt) || !(digitalRead(SW1))){
      LowBatt_Test--;
      digitalWrite(LED1, OFF);
    }
    if(!(digitalRead(SW1)))
      Debug_Count++;
    delay(300);  // 300ms for about 2hz sampling
  }while(LowBatt_Test);
  // Debug_Count should be 5 if SW1 is held down during battery
  // testing, technially if the battery is low, the loop can 
  // accumulate 5 debug counts in any interval.
  // But we assume 5 debug counts means we are in debug mode
  if(Debug_Count == 5)
  {
    Serial.print(F("DEBUG MODE ENABLE, Last Battery Reading: "));
    Serial.println(BattV_Count);
    Debug_mode = ON;
  }
    
  // Timer0 is used for micros(), millis(), delay(), etc and can't be used
  // Select Timer 1-2 for UNO, 1-5 for MEGA, 1,3,4 for 16u4/32u4
  // Timer 1 is used for PWM output, so don't use for timer interrupt
  // Timer 2 is 8-bit timer, only for higher frequency
  
  ITimer2.init();

  if (ITimer2.attachInterruptInterval(TIMER_INTERVAL_MS, TimerHandler))
  {
    Serial.print(F("Starting  ITimer2 OK, millis() = ")); Serial.println(millis());  
  }
  else
    Serial.println(F("Can't set ITimer2. Select another freq. or timer"));

  delay(500); // wait half second, ISR will run a few loops
}

void loop()
{
  // All timed routines based on ISR counts must wait for 
  // the update flag so it only runs once per count.
  if(Updated)
  {
    // Check battery voltage everytime ISR runs to track low battery
    // It should reset if there are less than 5 consecutive violations
    // Once it exceeds 5 in a row, the count stays fixed until reset,
    // essentially latching.
    if((LowBatt_Test < 10) && (Debug_mode == OFF))
    {
      BattV_Count = analogRead(Battery);
      if(BattV_Count > LowBatt)
        LowBatt_Test = 0;
      else
        LowBatt_Test++;

      if(LowBatt_Test == 10)
      {
        Serial.println(F("Low Battery Detected, Shutdown drive routine."));
        digitalWrite(LED1, ON);
      }
    }

    
    // Check Temperature at 1Hz
    // Only after ISR run completes so ADC reads don't conflict
    if((LED_counter==0) || (LED_counter==10) || (LED_counter==20))
    {
      // 100k termistor + 25k resistor, 1V @ 25C, 2.86V @ 65C, 4.08V @ 100C
      // Read Motor driver temperature (expected 100k NTC3950)
      Drive_Temp = analogRead(FET_Temp);
      // Read Battery temperature
      Batt_Temp = analogRead(Battery_Temp);
      // If overtemp, use Temp_Test to halt operation
      if(Debug_mode)
      {
        // Convert voltage to temperature
        // S-H coefficient calculated from: https://www.bapihvac.com/wp-content/uploads/2010/11/Thermistor_100K.pdf
        // A = 0.82721; B = 2.08789; C = 0.80621
        
      }
    }


    // A 3 blink pattern repeating every 3 seconds based on ISR activity
    if((LED_counter==0) || (LED_counter==4) || (LED_counter==8))
    {
      digitalWrite(LED2, ON);
      //Serial.print(F("Blink LED2 @ ")); Serial.println(LED_counter);
    }
    if((LED_counter==2) || (LED_counter==6) || (LED_counter==10))
      digitalWrite(LED2, OFF);
      
    if(Debug_mode)
    {
      //digitalWrite(LED2, ON);
      if((LED_counter==0) || (LED_counter==15))
      {
        Serial.print(F("Debug... Shifter:")); Serial.print(int(Shifter_Dir));
        Serial.print(F("; Throttle_Eng:")); Serial.print(int(Throttle_Engaged));
        Serial.print(F("; Throttle:")); Serial.print(Throttle_Pos);
        Serial.print(F("; PWM:")); Serial.print(PWM_Val, DEC);
        Serial.print(F("d, ")); Serial.print(PWM_Val, HEX); Serial.println(F("h"));
        //Print temperatures
        Serial.print(F("FET Temp:")); Serial.print(5*(Drive_Temp/255));
        Serial.print(F("; Battery Temp:")); Serial.println(5*(Batt_Temp/255));
      }
    }
  
    Updated = false;
  }
}
