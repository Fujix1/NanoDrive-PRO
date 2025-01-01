/*
 *  Longan Nano VGM player
 *  YM2608 and YM2151
 *  by Fujix 2023 - 2025
 *  Version 2.0.0
 */

#include <Arduino.h>
extern "C" {
#include "fatfs/tf_card.h"
#include "lcd/lcd.h"
}
#include "Display/Display.hpp"
#include "FM/FM.hpp"
#include "PT2257/PT2257.hpp"
#include "SI5351/SI5351.hpp"
#include "file.hpp"
#include "keypad/keypad.hpp"
#include "tick.hpp"

void setup() {
  Lcd_Init();        // LCD 初期化
  LCD_Clear(BLACK);  //

  gpio_pin_remap_config(GPIO_SWJ_NONJTRST_REMAP,
                        ENABLE);  // PB4 のリマップを有効化

  LCD_ShowString(0, 0, (u8 *)("Initializing I2C.        "), WHITE);
  Wire.begin();  // I2C 初期化

  Tick.delay_ms(64);

  LCD_ShowString(0, 0, (u8 *)("Initializing SI5351.     "), WHITE);
  SI5351.begin();  // SI5351 起動
  SI5351.setFreq(SI5351_8000, 0);
  SI5351.setFreq(SI5351_3579, 1);
  SI5351.enableOutputs(true);

  LCD_ShowString(0, 0, (u8 *)("Initializing PT2257.     "), WHITE);
  PT2257.begin();  // PT2257 初期化

  LCD_ShowString(0, 0, (u8 *)("Starting FM.             "), WHITE);
  FM.begin();
  FM.reset();  // FMリセット

  Tick.delay_ms(200);        // SDカード安定用
  if (sd_init() == false) {  // ファイル初期化
    exit(0);
  }

  openFolder(0);
  filePlay(0);
}

void loop() {
  switch (Keypad.LastButton) {
    case btnSELECT:  // ◯－－－－
      Keypad.LastButton = btnNONE;
      while (!openFolder(+1));
      filePlay(0);
      break;
    case btnLEFT:  // －◯－－－
      Keypad.LastButton = btnNONE;
      while (!openFolder(-1));
      filePlay(0);
      break;
    case btnDOWN:  // －－◯－－
      Keypad.LastButton = btnNONE;
      filePlay(1);
      break;
    case btnUP:  // －－－◯－
      Keypad.LastButton = btnNONE;
      filePlay(-1);
      break;
    case btnNONE:
      break;
  }
}