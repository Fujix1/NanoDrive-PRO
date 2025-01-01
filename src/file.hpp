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

typedef enum { UNDEFINED = 0, VGM, S98 } NDFileType;

// VGM Status Info
typedef struct {
  uint32_t Version;
  uint32_t DataOffset;
  uint32_t LoopOffset;

  uint32_t Gd3offset;
  boolean DeviceIsYM2608 = false;
  boolean DRAMIs8bits = false;
} VGMInfoStruct;
extern VGMInfoStruct VGMinfo;

// Device Enum
typedef enum { NONE = 0, YM2149, YM2203, YM2612, YM2608, YM2151, YM2413, YM3526, YMF262, AY38910, SN76489 } DeviceType;

// S98 device info
typedef struct {
  uint32_t DeviceType = 0;
  uint32_t Clock = 0;
  uint32_t Pan = 0;
} S98DeviceInfoStruct;

// S98 tag info
typedef struct {
  String title;
  String artist;
  String game;
  String year;
  String genre;
  String comment;
  String copyright;
  String s98by;
  String system;
  boolean isUTF8 = false;
} S98TagStruct;

// S98 info
typedef struct {
  char FormatVersion;
  uint32_t TimerInfo;
  uint32_t TimerInfo2;
  uint32_t Compressing;
  uint32_t TAGAddress;
  uint32_t DumpAddress;
  uint32_t LoopAddress;
  uint32_t DeviceCount;
  S98DeviceInfoStruct *DeviceInfo;

  uint32_t OneCycle;
  uint32_t Sync;
} S98InfoStruct;

extern boolean fileLoaded;   // データが読み込まれた状態か
extern NDFileType fileType;  // プレイ中のファイルの種類

boolean sd_init();
boolean closeFile();
uint8_t get_vgm_ui8();
uint8_t get_vgm_ui8_at(FSIZE_t pos);
uint16_t get_vgm_ui16();
uint32_t get_vgm_ui32();
uint16_t get_vgm_ui16_at(FSIZE_t pos);
uint32_t get_vgm_ui32_at(FSIZE_t pos);

boolean openFile(char *);
void fileOpen(int, int);
void filePlay(int);
boolean openFolder(int);
int mod(int i, int j);

void vgmReady();
void vgmProcess();
bool vgmProcessMain();

void s98Ready();
void s98Process();

#endif