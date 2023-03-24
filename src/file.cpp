/*
 * Open file and process VGM data
 */
#include "file.hpp"

#include "Display/Display.hpp"
#include "FM/FM.hpp"
#include "PT2257/PT2257.hpp"
#include "SI5351/SI5351.hpp"
#include "keypad/keypad.hpp"

#define BUFFERCAPACITY 5120  // VGMの読み込み単位（バイト）
#define MAXLOOP 2            // 次の曲いくループ数
#define ONE_CYCLE 612u       // 速度
                             // 22.67573696145125 * 27 = 612.24
                             // 1000000 / 44100 * 27
#define PC98  PB9
#define PC98_HIGH (GPIO_BOP(GPIOB) = GPIO_PIN_9)
#define PC98_LOW (GPIO_BC(GPIOB) = GPIO_PIN_9)


boolean mount_is_ok = false;
uint8_t currentDir;                 // 今のディレクトリインデックス
uint8_t currentFile;                // 今のファイルインデックス
uint16_t numDirs = 0;               // ルートにあるディレクトリ数
char **dirs;                        // ルートにあるディレクトリの配列

char **dirFiles;                    // フォルダ内のファイルリスト
uint8_t numFilesInDir = 0;          // ファイル数
boolean pc98 = false;               // フォルダ ssg 減衰設定
uint8_t atte = 0;                   // フォルダ減衰率

boolean fileOpened = false;          // ファイル開いてるか
uint8_t dataBuffer[BUFFERCAPACITY];  // バッファ
uint16_t bufferPos = 0;              // バッファ内の位置
uint32_t filesize = 0;               // ファイルサイズ
UINT bufferSize = 0;                 // 現在のバッファ容量
NDFileType fileType = UNDEFINED;     // プレイ中のファイル種

uint64_t startTime;                  // 基準時間
boolean fileLoaded = false;          // ファイルが読み込まれた状態か
uint8_t songLoop = 0;                // 現在のループ回数
uint32_t compensation = 0;

// File handlers
FATFS fs;
FIL fil;
FILINFO fno;
FRESULT fr;

// VGM Status
typedef struct {
  uint32_t Version;
  uint32_t DataOffset;
  uint32_t LoopOffset;
  uint32_t Delay;
  uint32_t Gd3offset;
  boolean DeviceIsYM2608 = false;
  boolean DRAMIs8bits = false;
} VGMInfoStruct;
VGMInfoStruct VGMinfo;

typedef enum {
  NONE = 0,
  YM2149,
  YM2203,
  YM2612,
  YM2608,
  YM2151,
  YM2413,
  YM3526,
  YMF262,
  AY38910,
  SN76489
} DeviceType;

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
S98TagStruct s98tag;

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
S98InfoStruct s98info;



//---------------------------------------------------------------
// Init and open SD card
// 初期化とSDオープン
// フォルダ構造の読み込み
boolean sd_init() {
  
  pinMode(PC98, OUTPUT);

  int i, n;
  DIR dir;
  FILINFO fno;

  currentDir = 0;
  currentFile = 0;

  // Try until SD card is mounted
  // SD マウントするまで試行
  LCD_ShowString(0, 0, (u8 *)("Checking SD card."), WHITE);
  for (i = 0; i < 18; i++) {
    Tick.delay_ms(200);
    fr = f_mount(&fs, "", 1);
    // LCD_ShowNum(0, 64, i, 2, GREEN);
    LCD_ShowString(i * 8, 16, (u8 *)("."), GREEN);
    if (fr == 0) break;
  }

  if (fr == 0) {
    mount_is_ok = true;
    LCD_ShowString(0, 0, (u8 *)("Reading directories..."), GRAY);

    //-------------------------------------------------------
    // ルートのフォルダ一覧を取得する
    // 数える
    fr = f_findfirst(&dir, &fno, "", "*");
    LCD_ShowString(0, 16, (u8 *)("ROOT"), CYAN);

    while (fr == FR_OK && fno.fname[0]) {  // 数える
      if (!(fno.fattrib & AM_SYS) && !(fno.fattrib & AM_HID) &&
          (fno.fattrib & AM_DIR)) {
        // システムじゃない && 隠しじゃない && ディレクトリ
        numDirs++;
      }
      f_findnext(&dir, &fno);
    }

    // ディレクトリが無い
    if (numDirs == 0) {
      LCD_ShowString(0, 0, (u8 *)("No directory."), WHITE);
      return false;
    }

    // 配列初期化
    dirs = (char **)malloc(sizeof(char *) * numDirs);
    for (i = 0; i < numDirs; i++) {
      dirs[i] = (char *)malloc(sizeof(char) * 13);  // メモリ確保
    }

    n = 0;
    fr = f_findfirst(&dir, &fno, "", "*");
    while (fr == FR_OK && fno.fname[0]) {
      if (!(fno.fattrib & AM_SYS) && !(fno.fattrib & AM_HID) &&
          (fno.fattrib & AM_DIR)) {
        // システムじゃない && 隠しじゃない && ディレクトリ
        strcpy(dirs[n++], fno.fname);
        LCD_ShowString(0, 32, (u8 *)(dirs[n - 1]), CYAN);
      }
      f_findnext(&dir, &fno);
    }

    return true;

  } else {
    mount_is_ok = false;
    LCD_ShowString(0, 0, (u8 *)("SD card mount Error"), WHITE);
    return false;
  }
}

boolean closeFile() {
  FRESULT fr;

  if (!mount_is_ok) return false;

  if (fileOpened) {
    fr = f_close(&fil);
    if (fr == FR_OK) {
      fileOpened = false;
      return true;
    }
  }
  return false;
}

//----------------------------------------------------------------------
// 8 bit 返す
uint8_t get_vgm_ui8() {
  if (bufferPos == bufferSize) {  // バッファの終端を超えた
    if (bufferSize == BUFFERCAPACITY) {
      bufferPos = 0;
      f_read(&fil, dataBuffer, BUFFERCAPACITY, &bufferSize);
    } else {
      LCD_ShowString(0, 64, (u8 *)("EOF"), WHITE);
      return 0;
    }
  }
  return dataBuffer[bufferPos++];
}

//----------------------------------------------------------------------
// 16 bit 返す
uint16_t get_vgm_ui16() { return get_vgm_ui8() + (get_vgm_ui8() << 8); }

//----------------------------------------------------------------------
// 32 bit 返す
uint32_t get_vgm_ui32() {
  return get_vgm_ui8() | (get_vgm_ui8() << 8) | (get_vgm_ui8() << 16) |
         (get_vgm_ui8() << 24);
}

//----------------------------------------------------------------------
// 指定場所の 8 bit 返す
uint8_t get_vgm_ui8_at(uint32_t pos) {
  uint32_t currentPos = fil.fptr;  // 一時退避
  uint8_t buffer[1];
  UINT br;

  f_lseek(&fil, pos);
  f_read(&fil, buffer, 1, &br);
  f_lseek(&fil, currentPos);

  return buffer[0];
}

//----------------------------------------------------------------------
// 指定場所の 16 bit 返す
uint16_t get_vgm_ui16_at(uint32_t pos) {
  uint32_t currentPos = fil.fptr;  // 一時退避
  uint8_t buffer[2];
  UINT br;

  f_lseek(&fil, pos);
  f_read(&fil, buffer, 2, &br);
  f_lseek(&fil, currentPos);

  return (uint32_t(buffer[0])) + (uint32_t(buffer[1]) << 8);
}

//----------------------------------------------------------------------
// 指定場所の 32 bit 返す
uint32_t get_vgm_ui32_at(uint32_t pos) {
  uint32_t currentPos = fil.fptr;  // 一時退避
  uint8_t buffer[4];
  UINT br;

  f_lseek(&fil, pos);
  f_read(&fil, buffer, 4, &br);
  f_lseek(&fil, currentPos);

  return (uint32_t(buffer[0])) + (uint32_t(buffer[1]) << 8) +
         (uint32_t(buffer[2]) << 16) + (uint32_t(buffer[3]) << 24);
}

//----------------------------------------------------------------------
// ファイルを開く
boolean openFile(char *path) {
  String sPath;
  Tick.delay_us(16);

  if (!mount_is_ok) return false;

  fileOpened = false;
  filesize = 0;

  // LCD_Clear(BLACK);
  sPath = path;
  sPath.concat("              ");
  sPath = sPath.substring(0, 19);

  Display.set1(sPath);

  Tick.delay_us(16);

  fr = f_open(&fil, path, FA_READ);
  Tick.delay_us(16);
  if (fr == FR_OK) {
    fr = f_stat(
        path,
        &fno);  // ファイルサイズ取得 ※外すと f_seek が正しく動かないので注意
    filesize = fno.fsize;
    fileOpened = true;
    return true;
  } else {
    return false;
  }
}

//----------------------------------------------------------------------
// オープンした VGM ファイルを解析して再生準備する
void vgmReady() {
  String gd3[10];

  fileLoaded = false;
  songLoop = 0;
  VGMinfo.Delay = 0;
  VGMinfo.DeviceIsYM2608 = false;
  compensation = 0;

  // VGM Version
  VGMinfo.Version = get_vgm_ui32_at(8);

  // VGM Loop offset
  VGMinfo.LoopOffset = get_vgm_ui32_at(0x1c);

  // VGM gd3 offset
  VGMinfo.Gd3offset = get_vgm_ui32_at(0x14);

  // GD3
  if (VGMinfo.Gd3offset != 0) {
    UINT i, p;
    UINT br;

    VGMinfo.Gd3offset += 0x14;

    char c[2];
    uint8_t *gd3buffer;
    gd3buffer = (uint8_t *)malloc(sizeof(uint8_t) *
                                  (filesize - VGMinfo.Gd3offset - 12));

    f_lseek(&fil, VGMinfo.Gd3offset + 12);
    f_read(&fil, gd3buffer, filesize - VGMinfo.Gd3offset - 12, &br);

    i = 0;
    c[1] = '\0';
    for (p = 0; p < br; p += 2) {
      if (gd3buffer[p] == 0 && gd3buffer[p + 1] == 0) {
        i++;
        if (i == 10) break;
      } else {
        c[0] = gd3buffer[p];
        gd3[i].concat(c);
      }
    }

    free(gd3buffer);

    // GD3 の情報配列
    //  0 "Track name (in English characters)\0"
    //  1 "Track name (in Japanese characters)\0"
    //  2 "Game name (in English characters)\0"
    //  3 "Game name (in Japanese characters)\0"
    //  4 "System name (in English characters)\0"
    //  5 "System name (in Japanese characters)\0"
    //  6 "Name of Original Track Author (in English characters)\0"
    //  7 "Name of Original Track Author (in Japanese characters)\0"
    //  8 "Date of game's release written in the form yyyy/mm/dd, or just
    //     yyyy/mm or yyyy if month and day is not known\0"
    //  9 "Name of person who converted it to a VGM file.\0"
    //  10 "Notes\0"

    gd3[0].concat(" / ");
    gd3[0].concat(gd3[2]);
    Display.set2(gd3[0]);  // 曲名表示＋ゲーム名

    Display.set3(gd3[6]);  // システム名
    Display.set4(gd3[4]);
  }

  // Data offset
  // v1.50未満は 0x40、v1.50以降は 0x34 からの相対位置
  VGMinfo.DataOffset =
      (VGMinfo.Version >= 0x150) ? get_vgm_ui32_at(0x34) + 0x34 : 0x40;

  // Clock
  uint32_t vgm_ay8910_clock =
      (VGMinfo.Version >= 0x151 && VGMinfo.DataOffset >= 0x78)
          ? get_vgm_ui32_at(0x74)
          : 0;

  if (vgm_ay8910_clock) {
    switch (vgm_ay8910_clock) {
      case 1250000:
        SI5351.setFreq(SI5351_5000, 0);
        break;
      case 1500000:
        SI5351.setFreq(SI5351_6000, 0);
        break;
      case 1536000:
        SI5351.setFreq(SI5351_6144, 0);
        break;
      case 1789750:
      case 1789772:
      case 1789773:
      case 1789775:
        SI5351.setFreq(SI5351_7159, 0);
        break;
      case 2000000:
        SI5351.setFreq(SI5351_8000, 0);
        break;
      default:
        SI5351.setFreq(SI5351_8000, 0);
        break;
    }
  }

  uint32_t vgm_ym2203_clock = get_vgm_ui32_at(0x44);
  if (vgm_ym2203_clock) {
    switch (vgm_ym2203_clock) {
      case 3000000:  // 3MHz
        SI5351.setFreq(SI5351_6000, 0);
        break;
      case 3072000:  // 3.072MHz
        SI5351.setFreq(SI5351_6144, 0);
        break;
      case 3579580:  // 3.579MHz
      case 3579545:
        SI5351.setFreq(SI5351_7159, 0);
        break;
      case 3993600:
        SI5351.setFreq(SI5351_7987, 0);
        break;
      case 4000000:
        SI5351.setFreq(SI5351_8000, 0);
        break;
      case 4500000:  // 4.5MHz
        SI5351.setFreq(SI5351_9000, 0);
        break;
      default:
        SI5351.setFreq(SI5351_8000, 0);
        break;
    }
  }

  uint32_t vgm_ym2608_clock = get_vgm_ui32_at(0x48);
  if (vgm_ym2608_clock) {
    VGMinfo.DeviceIsYM2608 = true;
    switch (vgm_ym2608_clock) {
      case 7987000:  // 7.987MHz
        SI5351.setFreq(SI5351_7987, 0);
        break;
      case 8000000:  // 8MHz
        SI5351.setFreq(SI5351_8000, 0);
        break;
      default:
        SI5351.setFreq(SI5351_8000, 0);
        break;
    }
  }

  uint32_t vgm_ym2151_clock = get_vgm_ui32_at(0x30);
  if (vgm_ym2151_clock) {
    switch (vgm_ym2151_clock) {
      case 3375000:
        SI5351.setFreq(SI5351_3375, 1);
        break;
      case 3500000:
        SI5351.setFreq(SI5351_3500, 1);
        break;
      case 3579580:
      case 3579545:
      case 3580000:
        SI5351.setFreq(SI5351_3579, 1);
        break;
      case 4000000:
        SI5351.setFreq(SI5351_4000, 1);
        break;
      default:
        SI5351.setFreq(SI5351_3579, 1);
        break;
    }
  }

  fileLoaded = true;
}

// VGM をパースして YM2608 の DRAM タイプを調べる
void checkYM2608DRAMType() {
  VGMinfo.DRAMIs8bits = false;

  // VGM ファイルが YM2608 用か
  if (get_vgm_ui32_at(0x48) == 0) {
    return;
  }

  // VGM Version
  VGMinfo.Version = get_vgm_ui32_at(8);

  // Data offset
  VGMinfo.DataOffset =
      (VGMinfo.Version >= 0x150) ? get_vgm_ui32_at(0x34) + 0x34 : 0x40;

  // 初期バッファ補充
  f_lseek(&fil, VGMinfo.DataOffset);
  fr = f_read(&fil, dataBuffer, BUFFERCAPACITY, &bufferSize);
  bufferPos = 0;

  uint8_t reg;
  uint8_t dat;
  while (1) {
    uint8_t command = get_vgm_ui8();
    switch (command) {
      case 0x56:
        get_vgm_ui8();
        get_vgm_ui8();
        break;
      case 0x57:  // YM2608 port 1
        reg = get_vgm_ui8();
        dat = get_vgm_ui8();
        if (reg == 0x01) {
          VGMinfo.DRAMIs8bits = ((dat & 0b10) == 0b10);
          return;
        }
        break;
      case 0x61:
        get_vgm_ui8();
        get_vgm_ui8();
        break;
      case 0x62:
      case 0x63:
        break;
      case 0x66:
        return;
        break;
      case 0x67: {
        get_vgm_ui8();                       // dummy 0x66
        uint8_t type = get_vgm_ui8();        // data type
        uint32_t dataSize = get_vgm_ui32();  // size of data, in bytes
        switch (type) {
          case 0:
            for (uint32_t i = 0; i < (dataSize - 0x8); i++) {
              get_vgm_ui8();
            }
            break;
          case 0x81:  // YM2608 DELTA-T ROM data
            get_vgm_ui32();  // size of the entire ROM
            get_vgm_ui32();  // start address of data
            for (uint32_t i = 0; i < (dataSize - 0x8); i++) {
              get_vgm_ui8();
            }
        }
        break;
      }
      case 0x70:
      case 0x71:
      case 0x72:
      case 0x73:
      case 0x74:
      case 0x75:
      case 0x76:
      case 0x77:
      case 0x78:
      case 0x79:
      case 0x7A:
      case 0x7B:
      case 0x7C:
      case 0x7D:
      case 0x7E:
      case 0x7F:
        break;
      case 0x90: // DAC Stream
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        break;
      case 0x91:
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        break;
      case 0x92:
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        break;
      case 0x93:
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        break;
      case 0x94:
        get_vgm_ui8();
        break;
      case 0x95:
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        break;
    }
  }
}

void vgmProcess() {

  // 初期バッファ補充
  f_lseek(&fil, VGMinfo.DataOffset);
  fr = f_read(&fil, dataBuffer, BUFFERCAPACITY, &bufferSize);
  bufferPos = 0;

  boolean unmutenow = false; 

  while (1) {
    if (PT2257.process_fadeout()) {  // フェードアウト完了なら次の曲
      if (numFilesInDir - 1 == currentFile) {
        Keypad.LastButton = btnSELECT;
      } else {
        Keypad.LastButton = btnDOWN;
      }
      return;
    }

    uint8_t reg;
    uint8_t dat;
    startTime = get_timer_value();
    byte command = get_vgm_ui8();

    switch (command) {
      case 0xA0:  // AY8910, YM2203 PSG, YM2149, YMZ294D
        dat = get_vgm_ui8();
        reg = get_vgm_ui8();
        compensation += FM.set_register(dat, reg, 0);
        unmutenow = true;
        break;
      case 0x30:  // SN76489 CHIP 2
        // FM.write(get_vgm_ui8(), CS2);
        break;
      case 0x50:  // SN76489 CHIP 1
        // FM.write(get_vgm_ui8(), CS0);
        break;
      case 0x54:  // YM2151
        reg = get_vgm_ui8();
        dat = get_vgm_ui8();
        compensation += FM.set_register_opm(reg, dat);
        unmutenow = true;
        break;
      case 0x55:  // YM2203_0
        reg = get_vgm_ui8();
        dat = get_vgm_ui8();
        compensation += FM.set_register(reg, dat, 0);
        unmutenow = true;
        break;
      case 0x56:  // YM2608 port 0
        reg = get_vgm_ui8();
        dat = get_vgm_ui8();
        compensation += FM.set_register(reg, dat, 0);
        unmutenow = true;
        break;
      case 0x57:  // YM2608 port 1
        reg = get_vgm_ui8();
        dat = get_vgm_ui8();
        compensation += FM.set_register(reg, dat, 1);
        unmutenow = true;
        break;
      case 0x5A:  // YM3812
        // reg = get_vgm_ui8();
        // dat = get_vgm_ui8();
        // FM.set_register(reg, dat, CS0);
        break;

      // Wait n samples, n can range from 0 to 65535 (approx 1.49 seconds)
      case 0x61: {
        uint16_t delay = get_vgm_ui16();
        VGMinfo.Delay += delay;
        break;
      }
      // wait 735 samples (60th of a second)
      case 0x62:
        VGMinfo.Delay += 735;
        break;

      // wait 882 samples (50th of a second)
      case 0x63:
        VGMinfo.Delay += 882;
        break;

      case 0x66:
        if (!VGMinfo.LoopOffset) {  // ループしない曲
          if (numFilesInDir - 1 == currentFile) {
            Keypad.LastButton = btnSELECT;
          } else {
            Keypad.LastButton = btnDOWN;
          }
          return;
        } else {
          songLoop++;
          if (songLoop == MAXLOOP) {  // 既定ループ数ならフェードアウトON
            PT2257.start_fadeout();
          }
          f_lseek(&fil, VGMinfo.LoopOffset + 0x1C);  // ループする曲
          bufferPos = 0;  // ループ開始位置からバッファを読む
          f_read(&fil, dataBuffer, BUFFERCAPACITY, &bufferSize);
        }
        break;

      case 0x67: {                           // DATA BLOCK
        get_vgm_ui8();                       // dummy
        uint8_t type = get_vgm_ui8();        // data type
        uint32_t dataSize = get_vgm_ui32();  // size of data, in bytes

        // YM2608 用 VGM でないのに ADPCM がある場合は無視
        // 一部の YM2203 VGM に 空の ADPCM が存在しているので対策
        if (VGMinfo.DeviceIsYM2608 == false) {
          type = 0;
        }
        
        switch (type) {
          case 0x00: { // Ignore RAW PCM は飛ばす
            for (uint32_t i = 0; i < (dataSize - 0x8); i++) {
              get_vgm_ui8();
            }
            break;
          }  
          case 0x81:  // YM2608 DELTA-T ROM data
            get_vgm_ui32();                       // size of the entire ROM
            uint32_t startAddr = get_vgm_ui32();  // start address of data

            FM.set_register(0x00, 0x01, 1);  // ADPCM リセット
            FM.set_register(0x10, 0x17, 1);  // FLAG 制御
            FM.set_register(0x10, 0x80, 1);  // IRQ Reset
            FM.set_register(0x00, 0x60, 1);  // 外部メモリー設定、書き込み開始

            if (VGMinfo.DRAMIs8bits) {
              FM.set_register(0x01, 0x02, 1);  // RAM TYPE x 8 bits
            } else {
              FM.set_register(0x01, 0x00, 1);  // RAM TYPE x 1 bit
            }

            // Limit Address L/H (Always 0xffff)
            FM.set_register(0x0c, 0xff, 1);
            FM.set_register(0x0d, 0xff, 1);

            // Start Address L/H
            uint32_t start;
            if (VGMinfo.DRAMIs8bits) {
              start = startAddr >> 5;
            } else {
              start = startAddr >> 2;
            }
            FM.set_register(0x02, start & 0xff, 1);
            FM.set_register(0x03, (start >> 8) & 0xff, 1);

            // Stop Address L/H (Always 0xffff)
            FM.set_register(0x04, 0xff, 1);
            FM.set_register(0x05, 0xff, 1);

            uint8_t wait = 0;
            if (VGMinfo.DRAMIs8bits) {
              wait = 1;  // 8 bit はウェイトほぼ不要
            } else {
              wait = 28;  // 1 bit アクセスは遅い
            }

            for (uint32_t i = 0; i < (dataSize - 0x8); i++) {
              FM.set_register(0x08, get_vgm_ui8(), 1);  // データ書き込み
              Tick.delay_us(wait);
            }

            FM.set_register(0x00, 0x00, 1);  // 終了プロセス
            break;
          }
        } break;

      case 0x70:
      case 0x71:
      case 0x72:
      case 0x73:
      case 0x74:
      case 0x75:
      case 0x76:
      case 0x77:
      case 0x78:
      case 0x79:
      case 0x7A:
      case 0x7B:
      case 0x7C:
      case 0x7D:
      case 0x7E:
      case 0x7F:
        VGMinfo.Delay += (command & 15) + 1;
        break;
      case 0x90: // DAC Stream
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        break;
      case 0x91:
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        break;
      case 0x92:
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        break;
      case 0x93:
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        break;
      case 0x94:
        get_vgm_ui8();
        break;
      case 0x95:
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        get_vgm_ui8();
        break;
      case 0x00:
        break;
      default:
        break;
    }

    if (PT2257.muted && unmutenow) {
      PT2257.reset(atte); 
    }

    if (VGMinfo.Delay > 0) {
      bool flag = false;

      while ((get_timer_value() - startTime) <= VGMinfo.Delay * ONE_CYCLE) {
        if (VGMinfo.Delay>=compensation) {
          VGMinfo.Delay -= compensation;
          compensation = 0;
        } else {
          compensation -= VGMinfo.Delay;
          VGMinfo.Delay = 0;
          break;
        }

        if (flag == false && VGMinfo.Delay > 3) {
          flag = true;
          switch (Keypad.checkButton()) {
            case btnNONE:
            break;
            case btnRIGHT:
              Keypad.LastButton = btnNONE;
              PT2257.start_fadeout();
            break;
            case btnLEFT:
            case btnUP:
            case btnDOWN:
            case btnSELECT:
              return;
          }
          // LCD の長い曲名をスクロールする
          // タイミングずれるので無効
          // if (Display.update()) { // LCDの文字表示更新
          //
          //}
        }
      }
      VGMinfo.Delay = 0;
    }
  }
}

//----------------------------------------------------------------------
// オープンした S98 ファイルを解析して再生準備する
void s98Ready() {
  UINT i, p;

  fileLoaded = false;
  songLoop = 0;
  s98info.Sync = 0;

  // magic
  if (((get_vgm_ui8_at(0x00) != 0x53) || (get_vgm_ui8_at(0x01) != 0x39) ||
       (get_vgm_ui8_at(0x02) != 0x38))) {
    return;
  }

  s98info.FormatVersion = (char)get_vgm_ui8_at(0x03) - '0';

  switch (s98info.FormatVersion) {
    case 0:
    case 1:;
      s98info.TimerInfo = get_vgm_ui32_at(0x04);
      if (s98info.TimerInfo == 0) s98info.TimerInfo = 10;
      s98info.TimerInfo2 = 1000;
      s98info.TAGAddress = get_vgm_ui32_at(0x10);
      s98info.DumpAddress = get_vgm_ui32_at(0x14);
      s98info.LoopAddress = get_vgm_ui32_at(0x18);
      s98info.DeviceCount = 0;
      break;
    case 2:
      s98info.TimerInfo = get_vgm_ui32_at(0x04);
      if (s98info.TimerInfo == 0) s98info.TimerInfo = 10;
      s98info.TimerInfo2 = get_vgm_ui32_at(0x08);
      if (s98info.TimerInfo2 == 0) s98info.TimerInfo2 = 1000;
      s98info.TAGAddress = get_vgm_ui32_at(0x10);
      s98info.DumpAddress = get_vgm_ui32_at(0x14);
      s98info.LoopAddress = get_vgm_ui32_at(0x18);
      if (get_vgm_ui32_at(0x20) == 0) s98info.DeviceCount = 0;
      break;
    case 3:
      s98info.TimerInfo = get_vgm_ui32_at(0x04);
      if (s98info.TimerInfo == 0) s98info.TimerInfo = 10;
      s98info.TimerInfo2 = get_vgm_ui32_at(0x08);
      if (s98info.TimerInfo2 == 0) s98info.TimerInfo2 = 1000;
      s98info.TAGAddress = get_vgm_ui32_at(0x10);
      s98info.DumpAddress = get_vgm_ui32_at(0x14);
      s98info.LoopAddress = get_vgm_ui32_at(0x18);
      s98info.DeviceCount = get_vgm_ui32_at(0x1c);
      break;
  }

  // 1 sync の tick
  // 108 MHz / 4 = 27 ns
  s98info.OneCycle = 108000000 / 4 * s98info.TimerInfo / s98info.TimerInfo2;
  
  free(s98info.DeviceInfo);
  
  if (s98info.DeviceCount == 0) {
    s98info.DeviceInfo =
        (S98DeviceInfoStruct *)malloc(sizeof(S98DeviceInfoStruct) * 1);
    s98info.DeviceInfo[0].DeviceType = YM2612;
    s98info.DeviceInfo[0].Clock = 7987200;
    s98info.DeviceInfo[0].Pan = 3;
  } else {
    s98info.DeviceInfo = (S98DeviceInfoStruct *)malloc(
        sizeof(S98DeviceInfoStruct) * s98info.DeviceCount);
    for (i = 0; i < s98info.DeviceCount; i++) {
      s98info.DeviceInfo[i].DeviceType = get_vgm_ui32_at(0x20 + i * 0x10);
      s98info.DeviceInfo[i].Clock = get_vgm_ui32_at(0x24 + i * 0x10);
      s98info.DeviceInfo[i].Pan = get_vgm_ui32_at(0x28 + i * 0x10);
    }
  }

  // Tag info
  if (s98info.FormatVersion == 1 || s98info.FormatVersion == 2) { // old s98
    s98tag.title = "s98 version 1 and 2 file";
    s98tag.artist = "";
    s98tag.game = "";
    s98tag.year = "";
    s98tag.genre = "";
    s98tag.comment = "";
    s98tag.copyright = "";
    s98tag.s98by = "";
    s98tag.system = "";
    Display.set2(s98tag.title);
    Display.set3("");
    Display.set4("");
  } else if (s98info.FormatVersion == 3) { // s98 v3
    UINT br;
    char c[2];
    String st, stlower;
    uint8_t shift = 0;

    if (s98info.TAGAddress != 0) {
      uint8_t gd3buffer[filesize - s98info.TAGAddress - 5];
      f_lseek(&fil, s98info.TAGAddress + 5);
      f_read(&fil, gd3buffer, filesize - s98info.TAGAddress - 5, &br);

      // check BOM
      if (gd3buffer[0] == 0xef && gd3buffer[1] == 0xbb && gd3buffer[2] == 0xbf) {
        s98tag.isUTF8 = true;
      }
      if (s98tag.isUTF8) {
        shift = 3;
      } else {
        shift = 0;
      }
      c[1] = '\0';
      st = "";
      stlower = "";

      for (p = shift; p < br; p++) {
        if (gd3buffer[p] == 0x00) break;
        if (gd3buffer[p] == 0x0a) {
          stlower = st;
          stlower.toLowerCase();
          if (stlower.indexOf("title=") == 0) {
            s98tag.title = st.substring(6);
          } else if (stlower.indexOf("artist=") == 0) {
            s98tag.artist = st.substring(7);
          } else if (stlower.indexOf("game=") == 0) {
            s98tag.game = st.substring(5);
          } else if (stlower.indexOf("year=") == 0) {
            s98tag.year = st.substring(5);
          } else if (stlower.indexOf("genre=") == 0) {
            s98tag.genre = st.substring(6);
          } else if (stlower.indexOf("comment=") == 0) {
            s98tag.comment = st.substring(8);
          } else if (stlower.indexOf("copyright=") == 0) {
            s98tag.copyright = st.substring(10);
          } else if (stlower.indexOf("s98by=") == 0) {
            s98tag.s98by = st.substring(6);
          } else if (stlower.indexOf("system=") == 0) {
            s98tag.system = st.substring(7);
          }
          st = "";
          stlower = "";
        } else {
          c[0] = gd3buffer[p];
          st.concat(c);
        }
      }

      st = s98tag.title;
      st.concat(" / ");
      st.concat(s98tag.game);
      Display.set2(st);

      st = s98tag.artist;
      st.concat(" / ");
      st.concat(s98tag.system);
      Display.set3(st);
      Display.set4("");
    }
  }
  

  // Set Clocks
  if (s98info.DeviceInfo[0].DeviceType == AY38910 ||
      s98info.DeviceInfo[0].DeviceType == YM2149) {
    switch (s98info.DeviceInfo[0].Clock) {
      case 1250000:  // 1.25MHz
        SI5351.setFreq(SI5351_5000, 0);
        break;
      case 1500000:  // 1.5 MHz
        SI5351.setFreq(SI5351_6000, 0);
        break;
      case 1789772:
      case 1789773:
        SI5351.setFreq(SI5351_7159, 0);
        break;
      case 2000000:
        SI5351.setFreq(SI5351_8000, 0);
        break;
      default:
        SI5351.setFreq(SI5351_8000, 0);
        break;
    }
  } else if (s98info.DeviceInfo[0].DeviceType == YM2203) {
    switch (s98info.DeviceInfo[0].Clock) {
      case 3000000:  // 3MHz
        SI5351.setFreq(SI5351_6000);
        break;
      case 3072000:  // 3.072MHz
        SI5351.setFreq(SI5351_6144);
        break;
      case 3579580:  // 3.579MHz
      case 3579545:
        SI5351.setFreq(SI5351_7159);
        break;
      case 3993600:
        SI5351.setFreq(SI5351_7987);
        break;
      case 4000000:
        SI5351.setFreq(SI5351_8000);
        break;
      case 4500000:
        SI5351.setFreq(SI5351_9000);
        break;
      default:
        SI5351.setFreq(SI5351_7987);
        break;
    }
  } else if (s98info.DeviceInfo[0].DeviceType == YM2608) {
    switch (s98info.DeviceInfo[0].Clock) {
      case 7987000:  // 7.987000 MHz
      case 7987200:  // 7.987200 MHz
        SI5351.setFreq(SI5351_7987);
        break;
      case 8000000:  // 8MHz
        SI5351.setFreq(SI5351_8000);
        break;
      default:
        SI5351.setFreq(SI5351_7987);
        break;
    }
  } else if (s98info.DeviceInfo[0].DeviceType == YM2151) {
    switch (s98info.DeviceInfo[0].Clock) {
      case 3375000:
        SI5351.setFreq(SI5351_3375, 1);
        break;
      case 3500000:
        SI5351.setFreq(SI5351_3500, 1);
        break;
      case 3579580:
      case 3579545:
      case 3580000:
        SI5351.setFreq(SI5351_3579, 1);
        break;
      case 4000000:
        SI5351.setFreq(SI5351_4000, 1);
        break;
      default:
        SI5351.setFreq(SI5351_3579, 1);
        break;
    }
  }

  // 初期バッファ補充
  f_lseek(&fil, s98info.DumpAddress);
  fr = f_read(&fil, dataBuffer, BUFFERCAPACITY, &bufferSize);
  bufferPos = 0;
  fileLoaded = true;
  return;
}

void s98Process() {
  boolean timeUpdateFlag = false;  // 連続命令はスタート時刻を更新しないフラグ
  startTime = get_timer_value();

  while (fileLoaded) {
    if (PT2257.process_fadeout()) {  // フェードアウト完了なら次の曲
      if (numFilesInDir - 1 == currentFile) {
        Keypad.LastButton = btnSELECT;
      } else {
        Keypad.LastButton = btnDOWN;
      }
      return;
    }

    uint8_t addr;
    uint8_t data;

    if (timeUpdateFlag) {
      startTime = get_timer_value();
      timeUpdateFlag = false;
    }

    byte command = get_vgm_ui8();
    switch (command) {
      case 0x00:
        addr = get_vgm_ui8();
        data = get_vgm_ui8();
        if (s98info.DeviceInfo[0].DeviceType == YM2151) {
          FM.set_register_opm(addr, data);
        } else {
          FM.set_register(addr, data, 0);
        }
        break;
      case 0x01:
        addr = get_vgm_ui8();
        data = get_vgm_ui8();
        if (s98info.DeviceInfo[1].DeviceType == YM2151) {
          FM.set_register_opm(addr, data);
        } else {
          FM.set_register(addr, data, 1);
        }
        break;
      case 0xFF:  // 1 sync wait
        s98info.Sync += 1;
        break;
      case 0xFE: {  // n sync wait
        data = get_vgm_ui8();
        int s = 0, n = 0, i = 0;
        do {
          ++i;
          n |= (data & 0x7f) << s;
          s += 7;
        } while (data & 0x80);
        n += 2;
        s98info.Sync += n;
        break;
      }
      case 0xFD:
        timeUpdateFlag = true;
        if (s98info.LoopAddress == 0) {  // ループしない曲
          if (numFilesInDir - 1 == currentFile) {
            Keypad.LastButton = btnSELECT;
          } else {
            Keypad.LastButton = btnDOWN;
          }
          return;
        } else {
          songLoop++;
          if (songLoop == MAXLOOP) {  // 既定ループ数ならフェードアウトON
            PT2257.start_fadeout();
          }
          f_lseek(&fil, s98info.LoopAddress);  // ループする曲
          bufferPos = 0;  // ループ開始位置からバッファを読む
          f_read(&fil, dataBuffer, BUFFERCAPACITY, &bufferSize);
        }
        break;
    }

    if (s98info.Sync > 1) {
      while ((get_timer_value() - startTime) <= s98info.Sync * s98info.OneCycle) {
        if (s98info.Sync > 0) {
          switch (Keypad.checkButton()) {
            case btnNONE:
            break;
            case btnRIGHT:
              Keypad.LastButton = btnNONE;
              PT2257.start_fadeout();
            break;
            case btnLEFT:
            case btnUP:
            case btnDOWN:
            case btnSELECT:
              return;
          }
        }
      }
      timeUpdateFlag = true;
      s98info.Sync = 0;
    }
  }
}
//----------------------------------------------------------------------
// ディレクトリ番号＋ファイル番号でファイルを開く
void fileOpen(int d, int f) {
  char st[21];

  PT2257.mute();
  fileLoaded = false;
  closeFile();

  if (pc98) {
    PC98_HIGH;
  } else {
    PC98_LOW;
  }
  
  strcpy(st, dirs[d]);
  strcat(st, "/");
  strcat(st, dirFiles[f]);

  if (openFile(st)) {
    // 拡張子チェック
    const char *ext = strrchr(dirFiles[f], '.');
    if (strcmp(".VGM", ext) == 0) {
      fileType = VGM;
    } else if (strcmp(".S98", ext) == 0) {
      fileType = S98;
    } else {
      fileType = UNDEFINED;
    }
    switch (fileType) {
      case VGM:
        vgmReady();
        checkYM2608DRAMType();
        FM.reset();
        Tick.delay_ms(16);
        //PT2257.reset(atte);
        vgmProcess();
        break;
      case S98:
        s98Ready();
        FM.reset();
        Tick.delay_ms(16);
        PT2257.reset(atte);
        s98Process();
        break;
      default:
        break;
    }
  }
}

//----------------------------------------------------------------------
// ディレクトリ内の count 個あとの曲再生。マイナスは前の曲
void filePlay(int count) {
  currentFile = mod(currentFile + count, numFilesInDir);
  fileOpen(currentDir, currentFile);
}


//----------------------------------------------------------------------
// count 個あとのフォルダを開いてファイルリスト取得
// 最初のファイルを再生。マイナスは前のディレクトリ
// 再生するファイルがない場合は false を返す

boolean openFolder(int count) {

  PT2257.mute();

  currentFile = 0;
  currentDir = mod(currentDir + count, numDirs);
  LCD_Clear(BLACK);
  LCD_ShowString(0, 0, (u8 *)("OPENING FOLDER..."), GRAY);
  LCD_ShowString(0, 16, (u8 *)(dirs[currentDir]), GRAY);

  // 初期化
  atte = 0;
  pc98 = false;

  // 配列のメモリ解放
  for ( unsigned int i = 0; i < numFilesInDir; i++) {
    free(dirFiles[i]);
  }
  free(dirFiles);

  // フォルダ内ファイル数える
  DIR dir;
  FILINFO fno;
  byte n;

  // .VGM か .S98 のファイル数取得
  fr = f_findfirst(&dir, &fno, dirs[currentDir], "*.*");
  n = 0;
  while (fr == FR_OK && fno.fname[0]) {
    // 拡張子チェック
    const char *ext = strrchr(fno.fname, '.');
    if (strcmp(".VGM", ext) == 0 || strcmp(".S98", ext) == 0) {
      n++;
    }
    f_findnext(&dir, &fno);
  }
  numFilesInDir = n;

  if (n == 0) {
    return false;
  }

  // フォルダ別設定
  // 以下の名前を含むファイルがあれば全部 att* dB 下げる
  fr = f_findfirst(&dir, &fno, dirs[currentDir], "att2");
  if (fr == FR_OK && fno.fname[0]) {
    atte = 2;
  }
  fr = f_findfirst(&dir, &fno, dirs[currentDir], "att4");
  if (fr == FR_OK && fno.fname[0]) {
    atte = 4;
  }
  fr = f_findfirst(&dir, &fno, dirs[currentDir], "att6");
  if (fr == FR_OK && fno.fname[0]) {
    atte = 6;
  }
  fr = f_findfirst(&dir, &fno, dirs[currentDir], "att8");
  if (fr == FR_OK && fno.fname[0]) {
    atte = 8;
  }
  fr = f_findfirst(&dir, &fno, dirs[currentDir], "att10");
  if (fr == FR_OK && fno.fname[0]) {
    atte = 10;
  }
  fr = f_findfirst(&dir, &fno, dirs[currentDir], "att12");
  if (fr == FR_OK && fno.fname[0]) {
    atte = 12;
  }
  fr = f_findfirst(&dir, &fno, dirs[currentDir], "att14");
  if (fr == FR_OK && fno.fname[0]) {
    atte = 14;
  }

  // フォルダ別 SSG 減衰設定
  fr = f_findfirst(&dir, &fno, dirs[currentDir], "pc98");
  if (fr == FR_OK && fno.fname[0]) {
    pc98 = true;
  }

  // ファイル名保持用配列メモリ確保
  dirFiles = (char **)malloc(sizeof(char *) * n);
  for (int i = 0; i < n; i++) {
    dirFiles[i] = (char *)malloc(sizeof(char) * 13);
  }

  // .VGMと.S98のファイル名取得
  fr = f_findfirst(&dir, &fno, dirs[currentDir], "*.*");
  n = 0;
  while (fr == FR_OK && fno.fname[0]) {
    // 拡張子チェック
    const char *ext = strrchr(fno.fname, '.');
    if (strcmp(".VGM", ext) == 0 || strcmp(".S98", ext) == 0) {
      strcpy(dirFiles[n], fno.fname);
      n++;
    }
    f_findnext(&dir, &fno);
  }
  return true;
}

int mod(int i, int j) {
  return (i % j) < 0 ? (i % j) + 0 + (j < 0 ? -j : j) : (i % j + 0);
}
