#include "FM.hpp"
extern "C" {
#include "lcd/lcd.h"
}

// Output Pins
#define D0 PC15
#define D1 PC14
#define D2 PC13
#define D3 PA0
#define D4 PA1
#define D5 PA2
#define D6 PA3
#define D7 PA4

#define A0 PA12
#define A1 PA8
#define WR PB4
#define IC PA11

#define CS0 PB8
#define CS1 PB5

#define A0_HIGH (GPIO_BOP(GPIOA) = GPIO_PIN_12)
#define A0_LOW (GPIO_BC(GPIOA) = GPIO_PIN_12)
#define A1_HIGH (GPIO_BOP(GPIOA) = GPIO_PIN_8)
#define A1_LOW (GPIO_BC(GPIOA) = GPIO_PIN_8)
#define WR_HIGH (GPIO_BOP(GPIOB) = GPIO_PIN_4)
#define WR_LOW (GPIO_BC(GPIOB) = GPIO_PIN_4)
#define IC_HIGH (GPIO_BOP(GPIOA) = GPIO_PIN_11)
#define IC_LOW (GPIO_BC(GPIOA) = GPIO_PIN_11)

#define CS0_HIGH (GPIO_BOP(GPIOB) = GPIO_PIN_8)
#define CS0_LOW (GPIO_BC(GPIOB) = GPIO_PIN_8)
#define CS1_HIGH (GPIO_BOP(GPIOB) = GPIO_PIN_5)
#define CS1_LOW (GPIO_BC(GPIOB) = GPIO_PIN_5)


void FMChip::begin() {

  pinMode(D0, OUTPUT);
  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(D3, OUTPUT);
  pinMode(D4, OUTPUT);
  pinMode(D5, OUTPUT);
  pinMode(D6, OUTPUT);
  pinMode(D7, OUTPUT);

  pinMode(WR, OUTPUT);
  pinMode(A0, OUTPUT);
  pinMode(A1, OUTPUT);
  pinMode(IC, OUTPUT);

  pinMode(CS0, OUTPUT);
  pinMode(CS1, OUTPUT);

}

void FMChip::reset(void) {
  CS0_LOW;
  CS1_LOW;
  WR_HIGH;
  A0_LOW;
  A1_LOW;
  IC_LOW;
  Tick.delay_us(100);  // at least 12 cycles // at 1.5MHz: 0.67us
  IC_HIGH;
  CS0_HIGH;
  CS1_HIGH;
  Tick.delay_us(100);
}

void FMChip::set_output(byte data) {
  // LOW
  GPIO_BC(GPIOC) = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  GPIO_BC(GPIOA) =
      GPIO_PIN_4 | GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0;
  // HIGH
  GPIO_BOP(GPIOC) =
      ((data & 0b1) << 15) | ((data & 0b10) << 13) | ((data & 0b100) << 11);
  GPIO_BOP(GPIOA) = (data & 0b11111000) >> 3;  // D3, D4 ,D5, D6, D7
}

u_int8_t FMChip::set_register(byte addr, byte data, boolean a1=0) {
  uint8_t wait = 0;

  /* リズムオフ
  if (a1==0 && addr==0x10) {
    data = 0x80;
  }
  */

  /*
    書き込みモードの待ち時間

    アドレスライト後
    音源部   アドレス    待ちサイクル
    FM       $21 - $B6  17 = 2.125 us
    SSG      $00 - $0F   0
    リズム   $10 - $1D  17
    ADPCM   $00 - $10   0

    データライト後
    音源部   アドレス    待ちサイクル
    FM       $21 - $9E  83 = 10.375 us
             $A0 - $B6  47 = 5.875 us
    SSG      $00 - $0F   0
    リズム   $10        576 = 72us
             $11 - $1D  83 = 10.375 us
    ADPCM    $00 - $10   0

    1/8MHz = 0.125 us
  */

  set_output(addr);

  A0_LOW;
  CS0_LOW;

  if (a1) {
    A1_HIGH;
  } else {
    A1_LOW;
  }
  WR_LOW;
  Tick.delay_500ns();
  WR_HIGH;

  // 書き込み後の待ち時間
  if (a1 == 0) {
    if (addr >= 0 && addr <= 0x0f) { // SSG

    } else { // リズム + FM 1-3
      Tick.delay_us(10);
      wait+=10;
    } 
  } else {
    if (addr >= 0x30 && addr <= 0xb6) { // FM 4-6
      Tick.delay_us(10);
      wait+=10;
    } else {      // ADPCM
      //Tick.delay_us(5);
    } 
  }

  //---------------------------------------
  // data
  
  A0_HIGH;
  Tick.delay_500ns();
  // WR_LOW -> WR_HIGH:  Tww 200ns
  //   D0-D7: Twds 100ns
  WR_LOW;
  Tick.delay_500ns();
  set_output(data);
  Tick.delay_500ns(); //  Tww 最小 200ns
  WR_HIGH;

  /*
    FM       $21 - $9E  83 = 10.375 us
             $A0 - $B6  47 = 5.875 us
    SSG      $00 - $0F   0
    リズム   $10        576 = 72us
             $11 - $1D  83 = 10.375 us
    ADPCM    $00 - $10   0
  */
 
  // 書き込み後の待ち時間
  if (a1 == 0) {
    if (addr >= 0 && addr <= 0x0f) {  // SSG
      Tick.delay_us(16);
      wait+=16;
    } else if (addr >= 0x21 && addr <= 0x9e) {  // FM 1
      Tick.delay_us(16);
      wait+=16;
    } else if (addr >= 0xa0 && addr <= 0xb6) {  // FM 2
      Tick.delay_us(10);
      wait+=10;
    } else if (addr == 0x10) {  // Rythm 1
      Tick.delay_us(77);
      wait+=77;
    } else { // Rythm 2
      Tick.delay_us(16);
      wait+=16;
    }
  } else {
    if (addr >= 0x21 && addr <= 0x9e) {  // FM
      Tick.delay_us(16); 
      wait+=16;
    } else if (addr >= 0xa0 && addr <= 0xb6) { // FM
      Tick.delay_us(10);
      wait+=10;
    } else {  // ADPCM
      
    }
  }

  CS0_HIGH;
  A0_LOW;
  A1_LOW;
  if (wait>=24) {
    return wait-24;
  } else {
    return 0;
  }
}

u_int8_t FMChip::set_register_opm(byte addr, byte data) {
  uint8_t wait = 0;

  set_output(addr);
  A0_LOW;
  CS1_LOW;
  WR_LOW;
  Tick.delay_500ns();
  WR_HIGH;
  Tick.delay_us(4); // 最低 3us
  wait+=4;

  A0_HIGH;
  Tick.delay_500ns();
  WR_LOW;
  Tick.delay_500ns();
  set_output(data);
  Tick.delay_500ns();
  WR_HIGH;
  Tick.delay_us(20); // 最低 18us
  wait+=20;
  CS1_HIGH;
  A0_LOW;

  if (wait>=22) {
    return wait-22;
  } else {
    return 0;
  }
}

FMChip FM;
