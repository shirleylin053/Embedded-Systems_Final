#include <Arduino_FreeRTOS.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.h>
#include <semphr.h>
#include <stdlib.h>
#include <string.h>
#include <Keypad.h>

#define KEY_ROWS 4 
#define KEY_COLS 4 
#define CARD_0 "0000"
#define CARD_1 "1111"
#define CARD_2 "2222"
#define CARD_3 "3333"
#define CARD_4 "4444"

int redPin = 6;
int greenPin = 5;
int bluePin = 3;

// 依照行、列排列的按鍵字元（二維陣列）
char keymap[KEY_ROWS][KEY_COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte colPins[KEY_COLS] = {9, 8, 7, 4}; 
byte rowPins[KEY_ROWS] = {13, 12, 11, 10}; 
Keypad myKeypad = Keypad(makeKeymap(keymap), rowPins, colPins, KEY_ROWS, KEY_COLS);

int joyStick_PIN = 2;
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

TaskHandle_t IRHandle;
SemaphoreHandle_t mux, printMux;


int state = 0;
char CARD = 0;
char input[5];
int inputCount = 0;
char key = 0;
int money[5] = {0, 1000, 5000, 7000, 10000};
char input_amount[6];
int amountCount = 0;
int isPress = 0;

void loop() {
  key = myKeypad.getKey();
  if (key){  
    xTaskResumeFromISR(IRHandle);
  }

}

void vTaskIR(void *pvParameters){
  for(;;){
      char ch = key;
      if(ch == '*'){ // cancel
        if(state == 1 || state == 2 || state == 4) {
          initialToState0();
        }
      }
      if(state == 0){ // Choose card
          if(ch == '0' || ch == '1' || ch == '2' || ch == '3' || ch == '4'){
            writeRGB(256,0,0);
            state = 1;
            CARD = ch;
          }
      }
      else if(state == 1){ // Input password
          if(ch == '0' || ch == '1' || ch == '2' || ch == '3' || ch == '4' || ch == '5' || ch == '6' || ch == '7' || ch == '8' || ch == '9'){
            if(inputCount <= 3)
              input[inputCount] = ch;
            inputCount++;
          }
          else if(ch == 'D'){ // press check
             if(inputCount == 4){
                if(strcmp(input,"CARD_"+CARD)){
                    state = 2;
                }else{
                    Serial.println(input);
                    state = 3;
                }
                input[0] = input[1] = input[2] = input[3] = 0;
                inputCount = 0;
            }
            else{
                state = 3;
            }
          }
          else if(ch == '#'){//Revise
            if(inputCount > 0)
                input[inputCount] = 0;
            inputCount --;
          }
          
      }
      else if(state == 2){ // Choose service
          if(ch == '1'){ // Withdrawal
            state = 4;
          }
          else if(ch == '2'){ // Balance
            state = 5;
          }
      }
      else if(state == 4){
          if(ch == '0' || ch == '1' || ch == '2' || ch == '3' || ch == '4' || ch == '5' || ch == '6' || ch == '7' || ch == '8' || ch == '9'){
            if(amountCount <= 4)
                input_amount[amountCount] = ch;
            amountCount++;
          }
          else if(ch == 'D'){// press check
            if(withdrawalMoney()){
              state = 7;  
            }
            else{
              state = 8;
            }
          }
          else if(ch == '#'){//Revise
            if(amountCount > 0){
              amountCount --; 
              input_amount[amountCount] = 0;
            }
          }
      }
      else if(state == 7){
          if(ch == 'D'){ // press check
            state = 6;
          }
      }
      else if(state == 5){
          if(ch == 'D'){ // press check
            state = 6;
          }
      }
      vTaskSuspend(IRHandle);
  }
}

void vTaskLCD(void *pvParameters){
    for(;;){
        lcd.clear();
        lcd.setCursor(0, 0); 
        if(state == 0){
            lcd.print(F("Please insert"));
            lcd.setCursor(0, 1); 
            lcd.print(F("   your card"));
        }
        else if(state == 1){
            if(inputCount == 0){
                lcd.print(F("Please input"));
                lcd.setCursor(0, 1); 
                lcd.print(F("   password"));
            }
            else{
                for(int i = 0; i<inputCount; i++)
                  lcd.print(F("*"));
            }
        }
        else if(state == 2){
            lcd.print(F("1:Withdrawal"));
            lcd.setCursor(0, 1); 
            lcd.print(F("2:Balance"));
        }
        else if(state == 3){
            lcd.print(F("Wrong password"));
            vTaskDelay(50);
            initialToState0();
        }
        else if(state == 4){
            lcd.print(F("Amount:"));
            lcd.setCursor(0, 1); 
            lcd.print(input_amount);
        }
        else if(state == 5){
            lcd.print(F("Balance: "));
            lcd.print(cardMoney(CARD));
        }
        else if(state == 6){
            lcd.print(F("Thank you!"));
            vTaskDelay(50);
            initialToState0();
        }
        else if(state == 7){
          lcd.print(F("Withdrawal:"));
          lcd.print(input_amount);
          lcd.setCursor(0, 1); 
          lcd.print(F("Balance:"));
          lcd.print(cardMoney(CARD));
        }
        else if(state == 8){
          lcd.print(F("Insufficient  "));
          lcd.setCursor(0, 1); 
          lcd.print(F("      balance!"));
          vTaskDelay(50);
          state = 6;
        }
        else if(state == 9){
          lcd.print(F("Machine"));
          lcd.setCursor(0, 1); 
          lcd.print(F("  maintenance..."));
        }
        vTaskDelay(50);
    }
}

void handle_click() { 
  static unsigned long last_int_time = 0;
  unsigned long int_time = millis(); // Read the clock
  if (int_time - last_int_time > 200 ) {  
    // Ignore when < 200 msec
    isPress = !isPress; 
  }
  last_int_time = int_time;
}

ISR(TIMER1_COMPA_vect){
  if(isPress) {
    if(state != 9){
       writeRGB(256,0,0);
       state = 9;
    }
  }
  else{
    if(state == 9){
      initialToState0();
      state = 0;
    }
  }
  
}

void setup(){
    Serial.begin(9600);

    // RGB
    pinMode(redPin, OUTPUT);
    pinMode(greenPin, OUTPUT);
    pinMode(bluePin, OUTPUT);

    pinMode(joyStick_PIN, INPUT_PULLUP);
    attachInterrupt(0, handle_click, RISING);

    Timer1_init(); 

    xTaskCreate(vTaskIR, "TaskIR", 96, NULL, 0, &IRHandle);
    xTaskCreate(vTaskLCD, "TaskLCD",  128, NULL, 0, NULL);

    lcd.begin(16, 2);   // initialize LCD
    lcd.backlight();    // open LCD backlight
    lcd.clear();
    lcd.setCursor(0, 0);
    writeRGB(0,256,0);

    vTaskSuspend(IRHandle);
    vTaskStartScheduler();
}

void Timer1_init(){
  noInterrupts(); // atomic access to timer reg.
  TCCR1A = 0;    TCCR1B = 0;    TCNT1 = 0;
  TCCR1B |= (1 << WGM12); // turn on CTC mode
  TCCR1B |= (1<< CS12) | (1<<CS10); // 1024 prescaler
  OCR1A = 7812;  // (16,000,000/1024)/2  = 7812
  TIMSK1 |= (1 << OCIE1A);
  interrupts(); // enable all interrupts
}

void writeRGB(int rValue, int gValue, int bValue){
  analogWrite(redPin, rValue);
  analogWrite(greenPin, gValue);
  analogWrite(bluePin, bValue);
}

int cardMoney(char card){
  if(card == '0') return money[0];
  else if(card == '1') return money[1];
  else if(card == '2') return money[2];
  else if(card == '3') return money[3];
  else if(card == '4') return money[4];
}

bool withdrawalMoney(){
    int i = atoi(input_amount);
    int card = -1;
    if(CARD == '0') card = 0;
    else if(CARD == '1') card = 1;
    else if(CARD == '2') card = 2;
    else if(CARD == '3') card = 3;
    else if(CARD == '4') card = 4;

    if(money[card] - i < 0) return false;
    else{
      money[card] -= i;
      return true;
    }
}

void initialToState0(){
    writeRGB(0,256,0);
    CARD = 0;
    input[0] = input[1] = input[2] = input[3] = 0;
    inputCount = 0;
    input_amount[0] = input_amount[1] = input_amount[2] = input_amount[3] = input_amount[4] =0;
    amountCount = 0;
    state = 0;
}
