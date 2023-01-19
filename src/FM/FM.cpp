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

#define A0 PB5
#define A1 PA12
#define WR PB4
#define IC PA11
#define CS0 PB9
//#define CS1_PIN PA12
//#define CS2_PIN PB5

#define A0_HIGH (GPIO_BOP(GPIOB) = GPIO_PIN_5)
#define A0_LOW (GPIO_BC(GPIOB) = GPIO_PIN_5)
#define A1_HIGH (GPIO_BOP(GPIOA) = GPIO_PIN_12)
#define A1_LOW (GPIO_BC(GPIOA) = GPIO_PIN_12)
#define WR_HIGH (GPIO_BOP(GPIOB) = GPIO_PIN_4)
#define WR_LOW (GPIO_BC(GPIOB) = GPIO_PIN_4)
#define IC_HIGH (GPIO_BOP(GPIOA) = GPIO_PIN_11)
#define IC_LOW (GPIO_BC(GPIOA) = GPIO_PIN_11)

#define CS0_HIGH (GPIO_BOP(GPIOB) = GPIO_PIN_9)
#define CS0_LOW (GPIO_BC(GPIOB) = GPIO_PIN_9)

//#define CS1_HIGH (GPIO_BOP(GPIOA) = GPIO_PIN_12)  // HIGH
//#define CS1_LOW (GPIO_BC(GPIOA) = GPIO_PIN_12)    // LOW
//#define CS2_HIGH (GPIO_BOP(GPIOB) = GPIO_PIN_5)  // HIGH
//#define CS2_LOW  (GPIO_BC(GPIOB)  = GPIO_PIN_5)  // LOW

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
  //pinMode(CS1_PIN, OUTPUT);
  // pinMode(CS2_PIN, OUTPUT);
}

void FMChip::reset(void) {
  CS0_LOW;
  WR_HIGH;
  A0_LOW;
  A1_LOW;
  IC_LOW;
  Tick.delay_us(100);  // at least 12 cycles // at 1.5MHz: 0.67us
  IC_HIGH;
  CS0_HIGH;
  Tick.delay_us(100);
  
}

void FMChip::writeRaw(byte data, boolean a0=0, boolean a1=0 ) {
  uint32_t data_bits = 0;

  if (a0) {
    A0_HIGH;
  } else {
    A0_LOW;
  }

  if (a1) {
    A1_HIGH;
  } else {
    A1_LOW;
  }

  WR_HIGH;

  //---------------------------------------
  // data
  if (data & 0b00000001) {
    data_bits += GPIO_PIN_15;
  }
  if (data & 0b00000010) {
    data_bits += GPIO_PIN_14;
  }
  if (data & 0b00000100) {
    data_bits += GPIO_PIN_13;
  }
  // LOW
  GPIO_BC(GPIOC) = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  GPIO_BC(GPIOA) =
      GPIO_PIN_4 | GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0;

  // HIGH
  GPIO_BOP(GPIOC) = data_bits;
  GPIO_BOP(GPIOA) = (data & 0b11111000) >> 3;

  WR_LOW;
  Tick.delay_us(2);
  WR_HIGH;

}

void FMChip::set_register(byte addr, byte data, boolean a1=0) {

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

  // LOW
  GPIO_BC(GPIOC) = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  GPIO_BC(GPIOA) =
      GPIO_PIN_4 | GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0;
  // HIGH
  GPIO_BOP(GPIOC) =
      ((addr & 0b1) << 15) | ((addr & 0b10) << 13) | ((addr & 0b100) << 11);
  GPIO_BOP(GPIOA) = (addr & 0b11111000) >> 3;  // D3, D4 ,D5, D6, D7

  A0_LOW;
  CS0_LOW;

  if (a1) {
    A1_HIGH;
  } else {
    A1_LOW;
  }
  WR_LOW;
  Tick.delay_500ns();
  Tick.delay_500ns();
  WR_HIGH;

  // 書き込み後の待ち時間
  if (a1 == 0) {
    if (addr >= 0 && addr <= 0x0f) {
      //Tick.delay_500ns(); // SSG
    } else {
      Tick.delay_us(8); // リズム + FM 1-3
    } 
  } else {
    if (addr >= 0x30 && addr <= 0xb6) {
      Tick.delay_us(8); // FM 4-6
    } else {
      // ADPCM
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

 // LOW
  GPIO_BC(GPIOC) = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  GPIO_BC(GPIOA) =
      GPIO_PIN_4 | GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0;
  // HIGH
  GPIO_BOP(GPIOC) =
      ((data & 0b1) << 15) | ((data & 0b10) << 13) | ((data & 0b100) << 11);
  GPIO_BOP(GPIOA) = (data & 0b11111000) >> 3;

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
      //Tick.delay_500ns();
    } else if (addr >= 0x21 && addr <= 0x9e) {  // FM 1
      Tick.delay_us(15);
    } else if (addr >= 0xa0 && addr <= 0xb6) {  // FM 2
      Tick.delay_us(9);
    } else if (addr == 0x10) {  // Rythm 1
      Tick.delay_us(75);
    } else { // Rythm 2
      Tick.delay_us(15);
    }
  } else {
    if (addr >= 0x21 && addr <= 0x9e) {  // FM
      Tick.delay_us(15); 
    } else if (addr >= 0xa0 && addr <= 0xb6) { // FM
      Tick.delay_us(9);
    } else {  // ADPCM
      //Tick.delay_us(5);
    } 
  }

  CS0_HIGH;
  A0_LOW;
  A1_LOW;

}



FMChip FM;
