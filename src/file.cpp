/*
 * Open file and process VGM data
 */
#include "file.hpp"

#include "Display/Display.hpp"
#include "FM/FM.hpp"
#include "PT2257/PT2257.hpp"
#include "SI5351/SI5351.hpp"
#include "keypad/keypad.hpp"

#define BUFFERCAPACITY 2048   // VGMの読み込み単位（バイト）
#define MAXLOOP 2            // 次の曲いくループ数
#define ONE_CYCLE 608u       // 速度決定（少ないほど速い）
                             // 22.67573696145125 * 27 = 612.24  // 1000000 / 44100

boolean mount_is_ok = false;
uint8_t currentDir;                  // 今のディレクトリインデックス
uint8_t currentFile;                 // 今のファイルインデックス
uint8_t numDirs = 0;                 // ルートにあるディレクトリ数
char **dirs;                         // ルートにあるディレクトリの配列
uint8_t *attenuations;               // 各ディレクトリの減衰量 (デシベル)
char ***files;                       // 各ディレクトリ内の vgm ファイル名配列
uint8_t *numFiles;                   // 各ディレクトリ内の vgm ファイル数

boolean fileOpened = false;          // ファイル開いてるか
uint8_t dataBuffer[BUFFERCAPACITY];  // バッファ
uint16_t bufferPos = 0;              // バッファ内の位置
uint32_t filesize = 0;               // ファイルサイズ
UINT bufferSize = 0;                 // 現在のバッファ容量
NDFileType fileType = UNDEFINED;     // プレイ中のファイル種

uint64_t startTime;                  // 基準時間
boolean  fileLoaded = false;         // ファイルが読み込まれた状態か
uint8_t  songLoop = 0;               // 現在のループ回数


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
  boolean  DeviceIsYM2608 = false;
  boolean  DRAMIs8bits = false;
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



//---------------------------------------------------------------
// Init and open SD card
// 初期化とSDオープン
// ファイル構造の読み込み
boolean sd_init() {
  int i, j, n;

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

    // 各フォルダ内のファイル数に応じて確保
    files = (char ***)malloc(sizeof(char **) * numDirs);
    numFiles = new uint8_t[numDirs];
    attenuations = new uint8_t[numDirs];


    //-------------------------------------------------------
    // フォルダ内ファイル情報取得（数える）
    LCD_ShowString(0, 0, (u8 *)("READING FILES..."), GRAY);

    for (i = 0; i < numDirs; i++) {
      LCD_ShowString(0, 16, (u8 *)("             "), CYAN);
      LCD_ShowString(0, 32, (u8 *)("             "), CYAN);
      LCD_ShowString(0, 16, (u8 *)(dirs[i]), CYAN);

      // .VGM か .S98 のファイル数取得
      fr = f_findfirst(&dir, &fno, dirs[i], "*.*");
      n = 0;
      while (fr == FR_OK && fno.fname[0]) {
        String filename = fno.fname;
        filename.toLowerCase();
        String ext = filename.substring(filename.lastIndexOf(".")+1); 
        if (ext == "vgm" || ext == "s98") {
          n++;
        }
        f_findnext(&dir, &fno);
      }

      // ファイル名保持用配列メモリ確保
      if (n > 0) {
        files[i] = (char **)malloc(sizeof(char *) * n);
        for (j = 0; j < n; j++) {
          files[i][j] = (char *)malloc(sizeof(char) * 13);
        }
      }
      // フォルダ内のファイル数保持配列設定
      numFiles[i] = n;

      // フォルダノーマライズ
      // 以下の名前を含むファイルがあれば全部 att* dB 下げる
      attenuations[i] = 0;
      fr = f_findfirst(&dir, &fno, dirs[i], "att2");
      if (fr == FR_OK && fno.fname[0]) {
        attenuations[i] = 2;
      }
      fr = f_findfirst(&dir, &fno, dirs[i], "att4");
      if (fr == FR_OK && fno.fname[0]) {
        attenuations[i] = 4;
      }
      fr = f_findfirst(&dir, &fno, dirs[i], "att6");
      if (fr == FR_OK && fno.fname[0]) {
        attenuations[i] = 6;
      }
      fr = f_findfirst(&dir, &fno, dirs[i], "att8");
      if (fr == FR_OK && fno.fname[0]) {
        attenuations[i] = 8;
      }
      fr = f_findfirst(&dir, &fno, dirs[i], "att10");
      if (fr == FR_OK && fno.fname[0]) {
        attenuations[i] = 10;
      }
      fr = f_findfirst(&dir, &fno, dirs[i], "att12");
      if (fr == FR_OK && fno.fname[0]) {
        attenuations[i] = 12;
      }
      fr = f_findfirst(&dir, &fno, dirs[i], "att14");
      if (fr == FR_OK && fno.fname[0]) {
        attenuations[i] = 14;
      }
    }

    // .VGMと.S98のファイル名取得
    if (n > 0) {
      for (i = 0; i < numDirs; i++) {
        fr = f_findfirst(&dir, &fno, dirs[i], "*.*");
        n = 0;
        while (fr == FR_OK && fno.fname[0]) {
          String filename = fno.fname;
          filename.toLowerCase();
          String ext = filename.substring(filename.lastIndexOf(".")+1);
          if (ext == "vgm" || ext == "s98") {
            strcpy(files[i][n], fno.fname);
            n++;
            f_findnext(&dir, &fno);
          }
        }
      }
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
  return get_vgm_ui8() | (get_vgm_ui8() << 8) | (get_vgm_ui8() << 16) | (get_vgm_ui8() << 24);
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

  //LCD_ShowString(0, 0, (u8 *)("vgmReady start."), WHITE);

  UINT i, p;
  UINT br;
  String gd3[10];

  fileLoaded = false;
  songLoop = 0;
  VGMinfo.Delay = 0;
  VGMinfo.DeviceIsYM2608 = false;

  // VGM Version
  VGMinfo.Version = get_vgm_ui32_at(8);
  //LCD_ShowString(0, 0, (u8 *)("vgmReady Version."), WHITE);

  // VGM Loop offset
  VGMinfo.LoopOffset = get_vgm_ui32_at(0x1c);
  //LCD_ShowString(0, 0, (u8 *)("vgmReady LoopOffset."), WHITE);

  // VGM gd3 offset
  uint32_t vgm_gd3_offset = get_vgm_ui32_at(0x14);

  //LCD_ShowString(0, 0, (u8 *)("before GD3. "), WHITE);

  // GD3
  if (vgm_gd3_offset != 0) {
    vgm_gd3_offset += 0x14;

    char c[2];
    uint8_t *gd3buffer;
    gd3buffer = (uint8_t*)malloc(sizeof(uint8_t) * (filesize - vgm_gd3_offset - 12));

    f_lseek(&fil, vgm_gd3_offset + 12);
    f_read(&fil, gd3buffer, filesize - vgm_gd3_offset - 8, &br);

    i = 0;
    c[1] = '\0';
    //LCD_ShowString(0, 0, (u8 *)("after f_read. "), YELLOW);

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

   // LCD_ShowString(0, 0, (u8 *)("after free. "), YELLOW);

    /* GD3 の情報配列
      0 "Track name (in English characters)\0"
      1 "Track name (in Japanese characters)\0"
      2 "Game name (in English characters)\0"
      3 "Game name (in Japanese characters)\0"
      4 "System name (in English characters)\0"
      5 "System name (in Japanese characters)\0"
      6 "Name of Original Track Author (in English characters)\0"
      7 "Name of Original Track Author (in Japanese characters)\0"
      8 "Date of game's release written in the form yyyy/mm/dd, or just yyyy/mm
      or yyyy if month and day is not known\0" 9 "Name of person who converted
      it to a VGM file.\0" 10 "Notes\0"
    */

    gd3[0].concat(" / ");
    gd3[0].concat(gd3[2]);
    Display.set2(gd3[0]);  // 曲名表示＋ゲーム名

    gd3[4].concat(" / ");
    gd3[4].concat(gd3[8]);
    Display.set3(gd3[4]);  // システム名＋リリース日
  }
  //LCD_ShowString(0, 0, (u8 *)("after GD3"), WHITE);

  // Data offset
  // v1.50未満は 0x40、v1.50以降は 0x34 からの相対位置
  VGMinfo.DataOffset = (VGMinfo.Version >= 0x150) ? get_vgm_ui32_at(0x34) + 0x34 : 0x40;

  // Clock
  uint32_t vgm_ay8910_clock = (VGMinfo.Version >= 0x151 && VGMinfo.DataOffset >= 0x78)
                                  ? get_vgm_ui32_at(0x74)
                                  : 0;
  //LCD_ShowString(0, 0, (u8 *)("before Clock"), WHITE);

  if (vgm_ay8910_clock) {
    switch (vgm_ay8910_clock) {
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
  }

  uint32_t vgm_ym2203_clock = get_vgm_ui32_at(0x44);
  if (vgm_ym2203_clock) {
    switch (vgm_ym2203_clock) {
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
      case 4000000:
        SI5351.setFreq(SI5351_8000);
        break;
      case 4500000:     // 4.5MHz
        SI5351.setFreq(SI5351_9000);
        break;
      default:
        SI5351.setFreq(SI5351_8000);
        break;
    }
  }

  uint32_t vgm_ym2608_clock = get_vgm_ui32_at(0x48);
  if (vgm_ym2608_clock) {
    VGMinfo.DeviceIsYM2608 = true;
    switch (vgm_ym2608_clock) {
      case 7987000:  // 7.987MHz
        SI5351.setFreq(SI5351_7987);
        break;
      case 8000000:  // 8MHz
        SI5351.setFreq(SI5351_8000);
        break;
      default:
        SI5351.setFreq(SI5351_8000);
        break;
    }
  }
  /*
    uint32_t vgm_ym2151_clock = get_vgm_ui32_at(0x30);
    if (vgm_ym2151_clock) {
      switch (vgm_ym2151_clock) {
        case 3579580:
        case 3579545:
          SI5351.setFreq(SI5351_3579);
          break;
        case 4000000:
          SI5351.setFreq(SI5351_4000);
          break;
        case 3375000:
          SI5351.setFreq(SI5351_3375);
          break;
        default:
          SI5351.setFreq(SI5351_3579);
          break;
      }
    }
  */
  //LCD_ShowString(0, 0, (u8 *)("after Clock"), WHITE);
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
  VGMinfo.DataOffset = (VGMinfo.Version >= 0x150) ? get_vgm_ui32_at(0x34) + 0x34 : 0x40;

  // 初期バッファ補充
  f_lseek(&fil, VGMinfo.DataOffset);
  fr = f_read(&fil, dataBuffer, BUFFERCAPACITY, &bufferSize);
  bufferPos = 0;

  uint8_t reg;
  uint8_t dat;
  while(1) {
    uint8_t command = get_vgm_ui8();
    switch (command) {
      case 0x56:
        get_vgm_ui8();
        get_vgm_ui8();
        break;
      case 0x57: // YM2608 port 1
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
      case 0x67:{
        get_vgm_ui8();
        get_vgm_ui8();
        uint32_t size = get_vgm_ui32(); // size of data, in bytes
        for (uint32_t i=0; i < size ; i++) {
          get_vgm_ui8();
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
    }
  }
}

void vgmProcess() {

  // 初期バッファ補充
  f_lseek(&fil, VGMinfo.DataOffset);
  fr = f_read(&fil, dataBuffer, BUFFERCAPACITY, &bufferSize);
  bufferPos = 0;
  
  while (1) {
    if (PT2257.process_fadeout()) {  // フェードアウト完了なら次の曲
      if (numFiles[currentDir] - 1 == currentFile)
        openDirectory(1);
      else
        filePlay(1);
    }

    uint8_t reg;
    uint8_t dat;
    startTime = get_timer_value();
    byte command = get_vgm_ui8();
    

    switch (command) {
      case 0xA0:  // AY8910, YM2203 PSG, YM2149, YMZ294D
        dat = get_vgm_ui8();
        reg = get_vgm_ui8();
        FM.set_register(dat, reg, 0);
        break;
      case 0x30:  // SN76489 CHIP 2
        //FM.write(get_vgm_ui8(), CS2);
        break;
      case 0x50:  // SN76489 CHIP 1
        //FM.write(get_vgm_ui8(), CS0);
        break;
      case 0x54:  // YM2151
      case 0xa4:
        //reg = get_vgm_ui8();
        //dat = get_vgm_ui8();
        //FM.set_register(reg, dat, CS0);
        break;
      case 0x55:  // YM2203_0
        reg = get_vgm_ui8();
        dat = get_vgm_ui8();
        FM.set_register(reg, dat, 0);
        break;
      case 0x56:  // YM2608 port 0
        reg = get_vgm_ui8();
        dat = get_vgm_ui8();
        FM.set_register(reg, dat, 0);
        break;
      case 0x57:  // YM2608 port 1
        reg = get_vgm_ui8();
        dat = get_vgm_ui8();
        FM.set_register(reg, dat, 1);
        break;
      case 0x5A:  // YM3812
        //reg = get_vgm_ui8();
        //dat = get_vgm_ui8();
        //FM.set_register(reg, dat, CS0);
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
          if (numFiles[currentDir] - 1 == currentFile)
            openDirectory(1);
          else
            filePlay(1);
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

      case 0x67: { // DATA BLOCK
          get_vgm_ui8(); // dummy
          uint8_t type = get_vgm_ui8(); // data type
          uint32_t dataSize = get_vgm_ui32(); // size of data, in bytes

          switch (type) {
            case 0x00: 
            case 0x81: // YM2608 DELTA-T ROM data

              get_vgm_ui32(); // size of the entire ROM 
              uint32_t startAddr = get_vgm_ui32(); // start address of data

              FM.set_register(0x00, 0x01, 1); // ADPCM リセット
              Tick.delay_us(24);

              FM.set_register(0x10, 0x17, 1); // FLAG 制御
              FM.set_register(0x10, 0x80, 1); // IRQ Reset
              FM.set_register(0x00, 0x60, 1); // 外部メモリー設定、書き込み開始

              if (VGMinfo.DRAMIs8bits) {
                FM.set_register(0x01, 0x02, 1); // RAM TYPE x 8 bits
              } else {
                FM.set_register(0x01, 0x00, 1); // RAM TYPE x 1 bit 
              }

              // Limit Address L/H 
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

              // Stop Address L/H
              FM.set_register(0x04, 0xff, 1);
              FM.set_register(0x05, 0xff, 1);

              Tick.delay_us(24);

              uint8_t wait = 0;
              if (VGMinfo.DRAMIs8bits) {
                wait = 1; // 8 bit はウェイトほぼ不要
              } else {
                wait = 20; // 1 bit アクセスは時間がかかる 最低18usくらい
              }

              for (uint32_t i=0; i < (dataSize - 0x8); i++) {
                FM.set_register(0x08, get_vgm_ui8(), 1); // データ書き込み
                Tick.delay_us(wait);
              }

              FM.set_register(0x00, 0x00, 1); // 終了プロセス
              Tick.delay_us(24);

              break;
          }
        }
        break;

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
      case 0x00:
        break;
      default:
        break;
    }

    if (VGMinfo.Delay > 0) {
      bool flag = false;

      while ((get_timer_value() - startTime) <= VGMinfo.Delay * ONE_CYCLE) {

        if (flag == false && VGMinfo.Delay > 3) {
          flag = true;
          // handle key input
          switch (Keypad.checkButton()) {
            case Keypad.btnSELECT:  // ◯－－－－
              openDirectory(1);
              break;
            case Keypad.btnLEFT:  // －◯－－－
              openDirectory(-1);
              break;
            case Keypad.btnDOWN:  // －－◯－－
              filePlay(+1);
              break;
            case Keypad.btnUP:  // －－－◯－
              filePlay(-1);
              break;
            case Keypad.btnRIGHT:  // －－－－◯
              PT2257.start_fadeout();
              break;
          }
          // LCD の長い曲名をスクロールする
          // タイミングずれるので無効
          //if (Display.update()) { // LCDの文字表示更新
          //  
          //}
        }
      }
      VGMinfo.Delay = 0;

    }

  }
}



//----------------------------------------------------------------------
// ディレクトリ番号＋ファイル番号でファイルを開く
void fileOpen(int d, int f) {
  char st[64];

  PT2257.mute();
  fileLoaded = false;
  closeFile();

  sprintf(st, "%s/%s", dirs[d], files[d][f]);
  if (openFile(st)) {

    // 拡張子チェック
    String filename = files[d][f];
    filename.toLowerCase();
    String ext = filename.substring(filename.lastIndexOf(".")+1);

    if (ext == "vgm") {
      fileType = VGM;
    } else if (ext =="s98") {
      fileType = S98;
    }
    //LCD_ShowString(0, 0, (u8 *)("After suf check."), WHITE);

    switch (fileType) {
      case UNDEFINED:
        break;
      case VGM:
        vgmReady();
        //LCD_ShowString(0, 0, (u8 *)("before DRAM type"), WHITE);
        checkYM2608DRAMType();
        //LCD_ShowString(0, 0, (u8 *)("before fm.reset"), WHITE);
        FM.reset();
        Tick.delay_ms(16);
        //LCD_ShowString(0, 0, (u8 *)("before PT2257.reset"), WHITE);
        PT2257.reset(attenuations[d]);
        //LCD_ShowString(0, 0, (u8 *)("before vgmprocess"), WHITE);
        vgmProcess();
        break;
    }

  }
}

//----------------------------------------------------------------------
// ディレクトリ内の count 個あとの曲再生。マイナスは前の曲
void filePlay(int count) {
  currentFile = mod(currentFile + count, numFiles[currentDir]);
  fileOpen(currentDir, currentFile);
}

//----------------------------------------------------------------------
// count
// 個あとのディレクトリを開いて最初のファイルを再生。マイナスは前のディレクトリ
void openDirectory(int count) {
  currentFile = 0;
  currentDir = mod(currentDir + count, numDirs);
  fileOpen(currentDir, currentFile);
}

int mod(int i, int j) {
  return (i % j) < 0 ? (i % j) + 0 + (j < 0 ? -j : j) : (i % j + 0);
}

