/*
 * ファイル関係
 */

#ifndef __FILER_H
#define __FILER_H

#include <Arduino.h>
extern "C" {
  #include "fatfs/tf_card.h"
  #include "lcd/lcd.h"
}
#include "tick.hpp"

typedef enum {
  UNDEFINED = 0,
  VGM,
  S98
}  NDFileType;


extern boolean fileLoaded;    // データが読み込まれた状態か
extern NDFileType fileType;   // プレイ中のファイルの種類

boolean     sd_init();
boolean     closeFile();
uint8_t     get_vgm_ui8();
uint8_t     get_vgm_ui8_at(FSIZE_t pos);
uint16_t    get_vgm_ui16();
uint16_t    get_vgm_ui32();
uint16_t    get_vgm_ui16_at(FSIZE_t pos);
uint32_t    get_vgm_ui32_at(FSIZE_t pos);
void        pause(uint32_t samples);

boolean     openFile(char *);
void        fileOpen(int, int);
void        filePlay(int);
void        openDirectory(int);
int         mod(int i, int j);

void        vgmReady();
void        vgmProcess();

void        s98Ready();
void        s98Process();

#endif