
#include "keypad.hpp"

KeypadClass::KeypadClass() {
  rcu_periph_clock_enable(RCU_GPIOA);
  gpio_init(GPIOA, GPIO_MODE_AIN, GPIO_OSPEED_50MHZ, GPIO_PIN_6);

  RCU_CFG0 |= (0b10 << 14) | (1 << 28); // ADC clock = 108MHz / 8 = 13.5MHz(14MHz max.)
  rcu_periph_clock_enable(RCU_ADC0);
  ADC_CTL1(ADC0) |= ADC_CTL1_ADCON;
}

uint16_t KeypadClass::get_adc(int ch) {
  ADC_RSQ2(ADC0) = 0;
  ADC_RSQ2(ADC0) = ch;
  ADC_CTL1(ADC0) |= ADC_CTL1_ADCON;

  while( !(ADC_STAT(ADC0) & ADC_STAT_EOC) );

  uint16_t ret = ADC_RDATA(ADC0) & 0xFFFF;
  ADC_STAT(ADC0) &= ~ADC_STAT_EOC;
  return ret;
}

//----------------------------------------------------------------------
// ボタンの状態取得
Button KeypadClass::readButton() {
  /*
  電圧降下なし
  ボタン 押して無いとき 4095
  ボタン1 2985 - 2995   低電圧時 3000
  ボタン2 2051 - 2065   低電圧時 2200
  ボタン3 1375 - 1385   低電圧時 1500
  ボタン4 582 - 592     低電圧時 850
  ボタン5 8 - 20        低電圧時 300
  */
 
  //adc_software_trigger_enable(ADC0, ADC_INSERTED_CHANNEL);
  //int adcData = ADC_IDATA0(ADC0);
  uint16_t adcData = get_adc(6);

  //LCD_ShowNum(0, 64, adcData, 4, GREEN); // デバッグ用 実際の入力値表示
  if (adcData > 3900) return btnNONE;                        // 4095, 戻り値 5
  if (adcData < 400 && adcData >=0) return btnSELECT;        // 0, 戻り値 0
  if (adcData < 1000 && adcData >= 530)  return btnLEFT;     // 585 - 595 , 戻り値 1
  if (adcData < 1600 && adcData >= 1100) return btnDOWN;     // 1375 - 1385 , 戻り値 2
  if (adcData < 2350 && adcData >= 1900) return btnUP;       // 2051 - 2065 , 戻り値 3
  if (adcData < 3200 && adcData >= 2500) return btnRIGHT;    // 2985 - 2995 , 戻り値 4
  return btnNONE;
}

Button KeypadClass::checkButton() {
  Button anaInput;
  uint32_t ms = Tick.millis2();
  if (ms > buttonLastTick + BUTTON_INTERVAL) {  // 最後に押したときから
                                                // BUTTON_INTERVAL 経過してるとき
    anaInput = readButton();                   // ボタン取得
    if (lastAnalogInput != anaInput) {         // 前回のボタンと違えば
      lastAnalogInput = anaInput;
      if (anaInput != btnNONE) {
        LastButton = anaInput;
        buttonRepeatStarted = ms;
        return anaInput;
      }
    } else if (anaInput != 0 && lastAnalogInput == anaInput &&
               ms - buttonRepeatStarted > REPEAT_DELAY) {
      // 同じキーの場合はリピートディレイを過ぎているとき
      LastButton = anaInput;
      return anaInput;
    }
    buttonLastTick = ms;
  }
  LastButton = btnNONE;
  return btnNONE;
}

KeypadClass Keypad;
