/** @file Ardu_Clock.ino
  @brief Ardu_Clock - Main
  @author by Nikita7131 nikitamartynenko77@gmail.com Ukraine
  @details Arduino ide 2.0 LGT8F328 !!! internal 16 MHz !!! не забудь поставити такі значення, бо можуть упасти всі тайминги
  @date 04.2023
*/
#include <Wire.h>
#define RTC_Add 0x68

byte maxBrightest = 255;
#define only_smooth_changed_segments
//#define brightCRT


struct Display_Data {
  uint8_t Bufer[5]; ///< "піксельний" буфер екрану
  uint8_t DataBright[5]; ///< буфер яскравості сегменту
  unsigned int DataUDP = 200; ///< час оновлення сегменту  
};Display_Data disp; ///<  екран 

const uint8_t dispNumbMasks[] = {
  0b01111110,  ///<  0
  0b00110000,  ///<  1
  0b01101101,  ///<  2
  0b01111001,  ///<  3
  0b00110011,  ///<  4
  0b01011011,  ///<  5
  0b01011111,  ///<  6
  0b01110000,  ///<  7
  0b01111111,  ///<  8
  0b01111011   ///<  9
}; ///< маска цифр 0 - 9

struct Time {
  uint8_t second, minute, hour, day, date, month, year;
  bool error;
};Time time; ///< час  

uint32_t timerDispDataUDP = 0; // таймер оновлень часу 

class button {
  public:
    button (byte pin) {
      _pin = pin;
      pinMode(_pin, INPUT_PULLUP);
    }
    bool click() {
      bool btnState = digitalRead(_pin);
      if (!btnState && !_flag && millis() - _tmr >= 100) {
        _flag = true;
        _tmr = millis();
        return true;
      }
      if (!btnState && _flag && millis() - _tmr >= 500) {
        _tmr = millis ();
        return true;
      }
      if (btnState && _flag) {
        _flag = false;
        _tmr = millis();
      }
      return false;
    }
  private:
    byte _pin;
    uint32_t _tmr;
    bool _flag;
};

button btn1(5); 
button btn2(4);

byte Nmenu;

static uint32_t buttStateTimer = 0;  
static byte backTimer = 0;  
static bool btn1State;
static bool btn2State;

void setup() {

  Serial.begin(115200);

  Wire.begin();
 // setTime(0,45,12, 5, 1,5,23);

  dispBegin(); 
  
}

void loop() {

   if (buttStateTimer < millis()) {
     buttStateTimer = millis() + 200;

      btn1State = btn1.click();
      btn2State = btn2.click();

      if(btn1State && btn2State){
        backTimer = 100;
        Nmenu < 2 ? Nmenu ++ : Nmenu = 0;
      }
      else if(btn1State){
        backTimer = 100;
        if(Nmenu == 1){
          time.hour -- ;
        }else{
          time.minute --;
        }
        setTime(0,time.minute, time.hour, 5, 1,5,23);
         
      }
      else if(btn2State){
        backTimer = 100;
        if(Nmenu == 1){ 
          time.hour ++;
        }else{
          time.minute ++;
        }
         setTime(0,time.minute, time.hour, 5, 1,5,23);
         
      }
      backTimer > 0 ? backTimer -- : Nmenu = 0;
   }

  if (millis() - timerDispDataUDP >= 500) {
    readTime (&time.second, &time.minute, &time.hour, &time.day, &time.date, &time.month, &time.year, &time.error);
    do {
      timerDispDataUDP += 500;
      if (timerDispDataUDP < 500) break;  // переповнення uint32_t
    } while (timerDispDataUDP < millis() - 500); // захист від прокуску 
  }

}
/** @brief Ініцалізація таймерів 1,2,3 для виводу інформації і можливості керувати яскравістю кожного сегменту окремо 
  <br> Timer 1: Output A (D9),  Output B (D10); Mode : PCPWM, Frequency: 31.37255 kHz; Сегмент 1 , Сегмент 2
  <br> Timer 3: Output A (D11), Output B (D3);  Mode : PCPWM, Frequency: 31.37255 kHz; Сегмент 3 , Сегмент 4
  <br> Timer 3: Output B (D3), Interrupt on Timer Overflow;  Mode : PCPWM, Frequency: 31.37255 kHz;  Сегмент ":" , динамічна індикація  */
void dispBegin () {
  noInterrupts();

  // https://dbuezas.github.io/arduino-web-timers/#mcu=LGT8F328P&timer=1&CompareOutputModeB=set-up%2C+clear-down&OCR1B=177&FCPU_UI=16Mhz&CompareOutputModeA=set-up%2C+clear-down&OCR1A=85
  TCCR1A = 1 << COM1A1 | 1 << COM1A0 | 1 << COM1B1 | 1 << COM1B0 | 1 << WGM10;
  TCCR1B = 1 << CS10;
  DDRB = 1 << DDB1 | 1 << DDB2;
  OCR1A = 0;  // 1  D9
  OCR1B = 0;  // 2  D10
  // https://dbuezas.github.io/arduino-web-timers/#mcu=LGT8F328P&timer=3&CompareOutputModeA=set-up%2C+clear-down&OCR3A=85&CompareOutputModeB=set-up%2C+clear-down&OCR3B=128&FCPU_UI=16Mhz&OCnA_OutputPort=AC0P%28D6%29
  TCCR3A = 1 << COM3A1 | 1 << COM3A0 | 1 << COM3B1 | 1 << COM3B0 | 1 << WGM30;
  TCCR3B = 1 << CS30;
  PMX0 = 1 << WCE;
  PMX1 = 1 << C3AC;
  DDRF = 1 << DDF2;
  OCR3A = 0;  // 3 D6
  OCR3B = 0;  // 4 D2
  // https://dbuezas.github.io/arduino-web-timers/#mcu=LGT8F328P&timer=2&CompareOutputModeA=disconnect&CompareOutputModeB=set-up%2C+clear-down&FCPU_UI=16Mhz&clockPrescalerOrSource=1&InterruptOnTimerOverflow=on&OCR2B=128
  TCCR2A = 1 << COM2B1 | 1 << COM2B0 | 1 << WGM21 | 1 << WGM20;
  TCCR2B = 1 << CS20;
  TIMSK2 = 1 << TOIE2;
  DDRD = 1 << DDD3;
  OCR2B = 0;  // : D3

  interrupts();
}
/** @brief Динамічний вивід буфера на екран
  @param[in] uint8_t disp.Bufer[5] "піксельний" буфер екрану 
  @param[in] uint8_t disp.DataBright[5] яскравість кожного сегменту 
  @param[in] unsigned_int disp.DataUDP період оновлення сегменту (значення в Tikах таймера)

  як приклад:

  <br>disp.DataUDP = 200; // період оновлення сегменту (екрану) (значення в Tikах таймера) 200 ~ 80 разів в сек
  <br>disp.Bufer[0] = 0b00110000; // 1 сегмен число 1
  <br>disp.Bufer[1] = 0b01111001; // 2 сегмен число 3
  <br>disp.Bufer[2] = 0b00110011; // 3 сегмен число 4
  <br>disp.Bufer[3] = 0b01111011; // 4 сегмен число 9
  <br>disp.Bufer[4] = 0b00000011; // засвічуємо нижню і верхню крапку в ":"

  <br>disp.DataBright[0] = 255; // яскравість 1 сегменту 
  <br>disp.DataBright[1] = 255; // яскравість 2 сегменту 
  <br>disp.DataBright[2] = 255; // яскравість 3 сегменту 
  <br>disp.DataBright[3] = 255; // яскравість 4 сегменту 
  <br>disp.DataBright[4] = 255; // яскравість ":" сегменту  */
ISR (TIMER2_OVF_vect) {

  const uint8_t catPins[] = { A0, A1, A2, A3, 8, 7, A6 };  ///< список катодів екрану, (аноди жорско прив'язані до виходів таймерів 1,2,3)

  static uint8_t displayPtr = 0;
  static long timerTiks = 0;

  if (timerTiks < disp.DataUDP) {  // зменшуємо частоту індикації, щоб не так сильно падала яскравість екрану
    timerTiks++;
  } else {
    timerTiks = 0;
    if (!displayPtr) dispDataWrite();  // отримуємо данні 
    /* гасим минулий символ */
    switch (displayPtr == 0 ? 3 : displayPtr - 1) {
      case 0: OCR1A = 0; break;
      case 1: OCR1B = 0; break;
      case 2: OCR3A = 0; break;
      case 3: OCR3B = 0; break;
    }
    OCR2B = 0;
    /* виводим теперішній символ */
    for (byte i = 0; i < 7; i++) bitRead(disp.Bufer[displayPtr], i) == 1 ? pinMode(catPins[i], INPUT) : pinMode(catPins[i], INPUT_PULLUP);
    bitRead(disp.Bufer[4], 0) ? pinMode(11, INPUT) : pinMode(11, INPUT_PULLUP);
    bitRead(disp.Bufer[4], 1) ? pinMode(12, INPUT) : pinMode(12, INPUT_PULLUP);
    /* вмикаємо символ */
    switch (displayPtr) {
      #ifdef brightCRT // перепимикаємо яскравість екрану по CRT гаммі, 
        case 0: OCR1A = getBrightCRT(disp.DataBright[0]); break;
        case 1: OCR1B = getBrightCRT(disp.DataBright[1]); break;
        case 2: OCR3A = getBrightCRT(disp.DataBright[2]); break;
        case 3: OCR3B = getBrightCRT(disp.DataBright[3]); break;
      #else // перемикаємо яскравість екрану логарифмічно для людського глазу
        case 0: OCR1A = disp.DataBright[0]; break;
        case 1: OCR1B = disp.DataBright[1]; break;
        case 2: OCR3A = disp.DataBright[2]; break;
        case 3: OCR3B = disp.DataBright[3]; break;
      #endif
    }
    OCR2B = disp.DataBright[4];
    /* двигаємо вказівник по екрану */
    displayPtr < 3 ? displayPtr++ : displayPtr = 0;
  }

}
/** @brief яку інформацію будем виводити на екран 
    <br> фугнкція атоматично визивається кожен раз, після того як відображено інформацію на всіх 4 сегментах */
void dispDataWrite () {

  if(time.error == 0){

   switch(Nmenu){
     case 0:
      dispWriteTimeSmoothly(time.minute, time.hour); // плавно виводи чч:мм
     break;
     case 1:
     case 2:
      dispWriteTimeNoEffect(time.minute, time.hour, Nmenu);

     break;

   }

  

  } else dispWriteError ();

}
/** @brief генеруєм буфер (значення міняються плавно)
  @param[in] uint8_t minute хвилини
  @param[in] uint8_t hour години
  @param[out] uint8_t disp.Bufer[5] "піксельний" буфер екрану 
  @param[out] uint8_t disp.DataBright[5] яскравість кожного сегменту */
void dispWriteTimeSmoothly (byte minute, byte hour) {

  static uint8_t Bright = 0;
  static uint8_t M_Last = 0;

  #ifdef only_smooth_changed_segments
    static uint8_t Last_Bufer[4] = {255,255,255,255}; // буфер стари
  #endif

  if (minute != M_Last) { // тригер появи нового значення
    if (Bright > 1) Bright--; else { // зменшуємо яскравіть екрану, і коли він потух оновлюємо данні
      M_Last = minute; // скидуємо тригер

      disp.Bufer[0] = dispNumbMasks[discharge(hour, 10)];
      disp.Bufer[1] = dispNumbMasks[discharge(hour, 1)];
      disp.Bufer[2] = dispNumbMasks[discharge(minute, 10)];
      disp.Bufer[3] = dispNumbMasks[discharge(minute, 1)];
      disp.Bufer[4] = 0b00000011;
    }    
  } else {
    if (Bright < maxBrightest){ // вмикаємо екран після виведення нового значення
     Bright++; 
    }else{ // після того як повінстю увімкнули екран 
     #ifdef only_smooth_changed_segments // запам'ятовуємо, які цифри змінилися і будем змінювати тільки їхню яскравість
       Last_Bufer[0] = disp.Bufer[0];
       Last_Bufer[1] = disp.Bufer[1];
       Last_Bufer[2] = disp.Bufer[2];
       Last_Bufer[3] = disp.Bufer[3];       
     #endif
    } 
  }
  
  #ifdef only_smooth_changed_segments // змінюємо яскравіть лише того сигмента, що змінився
    Last_Bufer[0] != disp.Bufer[0] ? disp.DataBright[0] = Bright : disp.DataBright[0] = maxBrightest; 
    Last_Bufer[1] != disp.Bufer[1] ? disp.DataBright[1] = Bright : disp.DataBright[1] = maxBrightest;    
    Last_Bufer[2] != disp.Bufer[2] ? disp.DataBright[2] = Bright : disp.DataBright[2] = maxBrightest;  
    Last_Bufer[3] != disp.Bufer[3] ? disp.DataBright[3] = Bright : disp.DataBright[3] = maxBrightest;     
  #else // змінюємо яскравіть всіх сехментів одночасно
    disp.DataBright[0] = Bright;
    disp.DataBright[1] = Bright;
    disp.DataBright[2] = Bright;
    disp.DataBright[3] = Bright;
  #endif

  disp.DataBright[4] = Bright;
 
}
/** @brief генеруєм буфер (значення різька зміна)
  @param[in] uint8_t minute хвилини
  @param[in] uint8_t hour години
  @param[out] uint8_t disp.Bufer[5] "піксельний" буфер екрану 
  @param[out] uint8_t disp.DataBright[5] яскравість кожного сегменту */
void dispWriteTimeNoEffect (byte minute, byte hour, byte select) {

  static uint8_t M_Last = 0;
  static unsigned long blinkTimer;
  static bool state = 0;

  if (minute != M_Last) { // тригер появи нового значення
    M_Last = minute; // скидуємо триге
    
    disp.Bufer[0] = dispNumbMasks[discharge(hour, 10)];
    disp.Bufer[1] = dispNumbMasks[discharge(hour, 1)];
    disp.Bufer[2] = dispNumbMasks[discharge(minute, 10)];
    disp.Bufer[3] = dispNumbMasks[discharge(minute, 1)];
    disp.Bufer[4] = 0b00000011;
  } 
  
  if(blinkTimer < millis()){
    blinkTimer = millis() + 500;
    state = !state;

    if( select == 1 ){
      disp.DataBright[0] = state == 1 ? 255 : 0;
      disp.DataBright[1] = state == 1 ? 255 : 0;
      disp.DataBright[2] = 255;
      disp.DataBright[3] = 255;
    }
    if( select == 2 ){
      disp.DataBright[0] = 255;
      disp.DataBright[1] = 255;
      disp.DataBright[2] = state ? 255 : 0;
      disp.DataBright[3] = state ? 255 : 0;

    }

  }
  disp.DataBright[4] = maxBrightest;
 
}

/** @brief повертає значення заданого розряду числа
  @param[in] Value число 
  @param[in] Discharge розряд числа в форматі 1,10,100,1000,10000
  @param[out] uint8_t число, значення в діапазоні 0-9 */
uint8_t discharge (unsigned long Value, unsigned long Discharge) {
  return ((Value % (Discharge * 10)) / Discharge);
}

void dispWriteError(){

  disp.Bufer[0] = 0b01001111; // 'E'
  disp.Bufer[1] = 0b00000101; // 'r'
  disp.Bufer[2] = 0b00000101; // 'r'
  disp.Bufer[3] = 0b00000000; 
  disp.Bufer[4] = 0b00000000;

  disp.DataBright[0] = 255;
  disp.DataBright[1] = 255;
  disp.DataBright[2] = 255;
  disp.DataBright[3] = 255;
  disp.DataBright[4] = 255;

}

void readTime (byte *second, byte *minute, byte *hour, byte *day, byte *date, byte *month, byte *year, bool *error){

  Wire.beginTransmission(RTC_Add);
  Wire.write(0);

  if(Wire.endTransmission() == 0){

    Wire.requestFrom(RTC_Add, 7);

    *second = bcd2dec ( Wire.read() & 0x7F );
    *minute = bcd2dec ( Wire.read() );
    *hour = bcd2dec ( Wire.read() & 0x3F );
    *day = bcd2dec ( Wire.read() );
    *date =  bcd2dec ( Wire.read() );
    *month = bcd2dec ( Wire.read() );
    *year = bcd2dec ( Wire.read() );

    *error = 0;

  } else *error = 1;

}

void setTime (byte second, byte minute, byte hour, byte day, byte date, byte month, byte year){

  Wire.beginTransmission(RTC_Add);
  Wire.write(0);

  Wire.write(dec2bcd(second));  
  Wire.write(dec2bcd(minute));  
  Wire.write(dec2bcd(hour));  
  Wire.write(dec2bcd(day));  
  Wire.write(dec2bcd(month));  
  Wire.write(dec2bcd(year));  
  Wire.endTransmission();

}

byte bcd2dec (byte val) {
  return ((val / 16 * 10) + (val % 16));  
}

byte dec2bcd (byte val){
  return ((val / 10 * 16) + (val % 10));
} 
#ifdef brightCRT
/** @brief CRT гамма (квадратна)
  @param[in] uint8_t val   
  @param[out] uint8_t    
  */
uint8_t getBrightCRT(uint8_t val) {       
  return ((uint16_t)val * val + 255) >> 8; 
}
#endif

