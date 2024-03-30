/*
04\23\2023

decode 433mhz ask, max 32bit.
*/


#include <RCSwitch.h>
#include <SPI.h>
#include <SD.h>
#include "U8glib.h"

U8GLIB_SSD1306_128X32 u8g(U8G_I2C_OPT_NONE);
RCSwitch mySwitch = RCSwitch();
File Main1;

volatile long firstCode = 0, secondCode = 0;
unsigned int Length1 = 0, bitLen = 0, Length2 = 0;
bool TRmode = 0, NoSd = 0, Vlow = 0, antiDuplicatVal = 0, sdLenCountAble = 0, state1 = 0, state2 = 0, writeVal = 0, able = 0;
float Vout = 0.0;
char str1[30], Rinfo[30];
long SDlineCount = 0, SDlen = 0, SDline = 0, Tcode = 0, Time0 = 0, Time1 = 0, Time2 = 0, Time3 = 0, Time4 = 0;
char cr, c;  // cr for read, c for count SD lines


void setup() {
  mySwitch.enableTransmit(5);
  mySwitch.enableReceive(0);

  DDRD = 0xC8;  //pin 3, 6, 7
  DDRB = 0x07;  //pin 8, 9, 10
  PORTD |= 0xC8;// pullup 3,6,7 pins
  PORTB |= 0x07;//pullup 8,9,10 pins
  PCICR |= 0x02;
  PCIFR |= 0x02;
  PCMSK1 |= 0x07;
}

void loop() {
  visualisation();
  Work();
  if (!NoSd && !Vlow) {
    R_T_mode();
    ReadSD(LineNum());
  }
}

void visualisation() {
  u8g.setFont(u8g_font_6x13);
  if (!Vlow) {
    if (NoSd) {
      u8g.firstPage();
      do {
        SDnonePage();
      } while (u8g.nextPage());
    } else {
      u8g.firstPage();
      do {
        MainPage();  //main page
      } while (u8g.nextPage());
    }
  } else {
    u8g.firstPage();
    do {
      batteryLowPage();
    } while (u8g.nextPage());
  }
}

void MainPage() {
  u8g.setPrintPos(0, 10);
  u8g.print(Voltage(), 2);
  u8g.setPrintPos(25, 10);
  u8g.print("V");
  u8g.setPrintPos(40, 10);
  u8g.print("Main Page");
  u8g.setPrintPos(110, 10);
  ReIndicate();
  printSDcode();
  u8g.setPrintPos(0, 31);
  u8g.print("Code:");
  u8g.setPrintPos(40, 31);
  u8g.print(LineNum() + 1);
  u8g.print("/");
  u8g.print(LineCount());
}

void SDnonePage() {
  u8g.setPrintPos(0, 10);
  u8g.print(Voltage(), 2);
  u8g.setPrintPos(25, 10);
  u8g.print("V");
  u8g.setPrintPos(35, 20);
  u8g.print("Put SD!!!");
}

void batteryLowPage() {  //IF voltage < 5
  u8g.setFont(u8g_font_6x13);
  u8g.setPrintPos(35, 20);
  u8g.print("Low Battery");
}

void ReIndicate() {
  if (!choosMode()) {
    u8g.print("TR");
  } else {
    u8g.print("RE");
  }
}

void R_T_mode() {
  if (!choosMode()) {
    if ((LineCount() != 0) && SD.begin(4)) {
      Transmit();
    }
  } else {
    receiveSave();
  }
}

void receiveSave() {  //receive information proccesing and write in sd card chars
  if (mySwitch.available()) {

    firstCode = mySwitch.getReceivedValue();
    if (firstCode != secondCode) {
      secondCode = firstCode;
      sprintf(str1, "%ld", secondCode);// convert long type code to string
      Length1 = strlen(str1);
      antiDuplicat();

      if (!writeVal) {//control from atntiduplicat function notduplicat = true
        Main1 = SD.open("Main.txt", FILE_WRITE);
        writeOnSd();
        sdLenCountAble = 0;
        Main1.close();
      }
      writeVal = 0;
    }
    mySwitch.resetAvailable();
  }
}

void writeOnSd() {
  Main1.println();
  for (int i = 0; i < Length1; i++) {
    Main1.print(str1[i]);
  }
  Main1.print(',');
}

void ReadSD(long num) {  //read characters from sd save in array
  if (LineCount() != 0) {
    Main1 = SD.open("Main.txt", FILE_READ);
    for (long i = -1; i < num;) {
      cr = Main1.read();
      if (cr == '\n') {
        i++;
      }
    }
    RinfoRST();
    int y = 0;
    while (1) {  // loop is while read ','
      cr = Main1.read();
      Rinfo[y] = cr;
      if (cr == ',') {
        break;
      }
      y++;
    }
    Main1.close();
  }
}

void RinfoRST() {
  int len4 = 0;
  len4 = strlen(Rinfo);
  for (uint8_t v = 0; v < len4; v++) {
    Rinfo[v] = 0;
  }
}

long LineNum() {  //chose code line
  if (LineCount() != 0) {
    if (SDline < (LineCount() - 1)) {
      if (!(PINB & 0x01)) {
        if ((millis() - Time2) >= 90) {
          SDline += 1;
          Time2 = millis();
        }
      }
    }

    if (SDline > 0) {
      if (!(PINB & 0x02)) {
        if ((millis() - Time3) >= 90) {
          SDline -= 1;
          Time3 = millis();
        }
      }
    }
  }
  return SDline;
}

long LineCount() {  //count codes quantity
  Main1 = SD.open("Main.txt", FILE_READ);
  SDlen = Main1.size();
  if (!sdLenCountAble) {
    SDlineCount = 0;
    for (long x = 0; x < SDlen; x++) {
      c = Main1.read();
      if (c == ',') {
        SDlineCount += 1;
      } else if (c == '\n') {
        sdLenCountAble = 1;
      }
    }
  }
  Main1.close();
  return SDlineCount;
}

void Transmit() {  //transmitt marked code read chars from sd card convert chars -> long
  StrToInt();
  if (!(PIND & 0X80)) {
    mySwitch.send(Tcode, 32);
  }
}

void StrToInt() {  //need edit
  unsigned int len = 0;
  long ten = 1;
  long val1 = 0;

  Tcode = 0;
  ten = 1;
  len = strlen(Rinfo);
  for (int u = (len - 1); u >= 0; u--) {
    if (Rinfo[u] > 47 && Rinfo[u] < 58) {
      val1 = charToInt(Rinfo[u]);
      Rinfo[u] = 0;
      Tcode = (val1 * ten) + Tcode;
      ten *= 10;
    }
  }
}

int bitLenRead();  //coming soon..

void antiDuplicat() {
  if (!writeVal) {
    int len3 = 0;
    int count = 0;
    for (long j = 0; j < LineCount(); j++) {
      ReadSD(j);
      len3 = 0;
      len3 = strlen(Rinfo);
      if ((Length1 + 1) == len3) {
        for (uint8_t n = 0; n < (len3 - 1); n++) {
          if (Rinfo[n] == str1[n]) {
            count += 1;
          }
          if (count >= (len3 - 1)) {
            writeVal = 1;
            break;
          }
        }
        count = 0;
      }
      for (uint8_t p = 0; p < len3; p++) {
        Rinfo[p] = 0;
      }
    }
  }
}

uint8_t charToInt(char d) {
  uint8_t val;

  switch (d) {
    case '0':
      val = 0;
      break;
    case '1':
      val = 1;
      break;
    case '2':
      val = 2;
      break;
    case '3':
      val = 3;
      break;
    case '4':
      val = 4;
      break;
    case '5':
      val = 5;
      break;
    case '6':
      val = 6;
      break;
    case '7':
      val = 7;
      break;
    case '8':
      val = 8;
      break;
    case '9':
      val = 9;
      break;
  }
  return val;
}

void printSDcode() {
  unsigned int len1;
  int line = 0;

  len1 = strlen(Rinfo);
  for (int k = 0; k < len1; k++) {
    if ((Rinfo[k] > 47) && (Rinfo[k] < 58)) {
      u8g.setPrintPos(line, 21);
      u8g.print(Rinfo[k]);
      line += 9;
    }
  }
}

bool choosMode() {  //change mode 1-reciver or 0-tansmitter
  if ((millis() - Time1) >= 15) {
    if ((!(PIND & 0x40)) && !state2) {
      TRmode = !TRmode;
      state2 = 1;
    } else if ((PIND & 0x40) && state2) {
      state2 = 0;
    }
    Time1 = millis();
  }
  return TRmode;
}

float Voltage() {  //return voltage from ADC
  if ((millis() - Time0) >= 200) {
    ADMUX |= B11000011;
    ADCSRA |= B11000100;
    while (bit_is_set(ADCSRA, ADSC))
      ;
    Vout = ADCL | (ADCH << 8);
    Vout = (Vout / 1000) * 5.7;

    Time0 = millis();
  }
  return Vout;
}

void Work() {
  if (!SD.begin(4)) {
    NoSd = 1;
  } else {
    NoSd = 0;
  }
  if (Voltage() < 3) {
    Vlow = 1;
  } else {
    Vlow = 0;
  }
}
