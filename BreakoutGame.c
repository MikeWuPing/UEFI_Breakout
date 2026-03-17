/** @file
  Breakout Game - Optimized brick breaker game for UEFI Shell

  Copyright (c) 2024. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleTextInEx.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

//
// Random number generator
//
STATIC UINT32 mRandomSeed = 12345;

UINT32 GameRandom (VOID)
{
  mRandomSeed = mRandomSeed * 1103515245 + 12345;
  return (mRandomSeed >> 16) & 0x7FFF;
}

#define Random() GameRandom()

//
// Screen and layout constants
//
STATIC UINT32  mScreenWidth;
STATIC UINT32  mScreenHeight;
STATIC UINT32  mGameAreaWidth;
STATIC UINT32  mGameAreaX;
STATIC UINT32  mInfoPanelX;

#define INFO_PANEL_WIDTH   200

//
// Game constants
//
#define PADDLE_WIDTH_DEFAULT  100
#define PADDLE_WIDTH_WIDE     160
#define PADDLE_HEIGHT         14
#define BALL_SIZE             12
#define BRICK_ROWS            6
#define BRICK_COLS            10
#define BRICK_WIDTH           52
#define BRICK_HEIGHT          20
#define BRICK_PADDING         4
#define BRICK_TOP_OFFSET      70
#define MAX_BALLS             5
#define MAX_PARTICLES         50

#define BALL_SPEED_DEFAULT    5
#define BALL_SPEED_SLOW       2
#define PADDLE_SPEED          20

//
// Colors (BGRA format)
//
#define COLOR_BLACK         0xFF000000
#define COLOR_WHITE         0xFFFFFFFF
#define COLOR_RED           0xFF4040FF
#define COLOR_ORANGE        0xFF00A0FF
#define COLOR_YELLOW        0xFF00FFFF
#define COLOR_GREEN         0xFF40FF40
#define COLOR_CYAN          0xFFFFFF40
#define COLOR_BLUE          0xFFFF6040
#define COLOR_PURPLE        0xFFFF40FF
#define COLOR_PINK          0xFFB080FF
#define COLOR_GRAY          0xFF808080
#define COLOR_DARK_GRAY     0xFF404040
#define COLOR_GOLD          0xFF00D0FF

#define COLOR_BG_DARK       0xFF101020
#define COLOR_PANEL_BG      0xFF182838
#define COLOR_PANEL_BORDER  0xFF3080C0

//
// Brick types
//
typedef enum {
  BrickTypeNormal,
  BrickTypeStrong,
  BrickTypeVeryStrong,
  BrickTypeGold,
  BrickTypeExplosive,
  BrickTypeUnbreakable
} BRICK_TYPE;

//
// Power-up types
//
typedef enum {
  PowerUpNone,
  PowerUpWidePaddle,
  PowerUpMultiBall,
  PowerUpSlowBall,
  PowerUpExtraLife,
  PowerUpFireBall
} POWERUP_TYPE;

//
// Particle
//
typedef struct {
  INT32   X;
  INT32   Y;
  INT32   VX;
  INT32   VY;
  UINT32  Color;
  INT32   Life;
  BOOLEAN Active;
} PARTICLE;

//
// Ball
//
typedef struct {
  INT32   X;
  INT32   Y;
  INT32   OldX;
  INT32   OldY;
  INT32   DX;
  INT32   DY;
  INT32   Size;
  UINT32  Color;
  BOOLEAN Active;
  BOOLEAN FireBall;
} BALL;

//
// Paddle
//
typedef struct {
  INT32   X;
  INT32   Y;
  INT32   OldX;
  INT32   Width;
  INT32   Height;
  UINT32  Color;
} PADDLE;

//
// Brick
//
typedef struct {
  INT32       X;
  INT32       Y;
  INT32       Width;
  INT32       Height;
  UINT32      Color;
  BOOLEAN     Active;
  BRICK_TYPE  Type;
  INT32       Hits;
} BRICK;

//
// Power-up
//
typedef struct {
  INT32        X;
  INT32        Y;
  INT32        OldY;
  INT32        Width;
  INT32        Height;
  UINT32       Color;
  BOOLEAN      Active;
  POWERUP_TYPE Type;
  INT32        Speed;
} POWERUP;

//
// Global variables
//
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL       *mGop;
STATIC EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *mTextInputEx;
STATIC PADDLE    mPaddle;
STATIC BALL      mBalls[MAX_BALLS];
STATIC BRICK     mBricks[BRICK_ROWS][BRICK_COLS];
STATIC POWERUP   mPowerUps[10];
STATIC PARTICLE  mParticles[MAX_PARTICLES];
STATIC UINTN     mScore;
STATIC UINTN     mHighScore;
STATIC UINTN     mLives;
STATIC UINTN     mLevel;
STATIC BOOLEAN   mGameOver;
STATIC BOOLEAN   mGameWon;
STATIC BOOLEAN   mGameStarted;
STATIC BOOLEAN   mWidePaddleActive;
STATIC UINTN     mWidePaddleTimer;
STATIC BOOLEAN   mSlowBallActive;
STATIC UINTN     mSlowBallTimer;
STATIC UINTN     mComboCount;

//
// 5x7 font for digits (0-9)
//
STATIC UINT8 FontDigits[10][7] = {
  {0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70},
  {0x20, 0x60, 0x20, 0x20, 0x20, 0x20, 0x70},
  {0x70, 0x88, 0x08, 0x70, 0x80, 0x80, 0xF8},
  {0x70, 0x88, 0x08, 0x70, 0x08, 0x88, 0x70},
  {0x10, 0x30, 0x50, 0x90, 0xF8, 0x10, 0x10},
  {0xF8, 0x80, 0xF0, 0x08, 0x08, 0x88, 0x70},
  {0x70, 0x88, 0x80, 0xF0, 0x88, 0x88, 0x70},
  {0xF8, 0x08, 0x10, 0x20, 0x20, 0x20, 0x20},
  {0x70, 0x88, 0x88, 0x70, 0x88, 0x88, 0x70},
  {0x70, 0x88, 0x88, 0x78, 0x08, 0x88, 0x70},
};

// Letters A-Z
STATIC UINT8 FontLetters[26][7] = {
  {0x70, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x88},
  {0xF0, 0x88, 0x88, 0xF0, 0x88, 0x88, 0xF0},
  {0x70, 0x88, 0x80, 0x80, 0x80, 0x88, 0x70},
  {0xF0, 0x88, 0x88, 0x88, 0x88, 0x88, 0xF0},
  {0xF8, 0x80, 0x80, 0xF0, 0x80, 0x80, 0xF8},
  {0xF8, 0x80, 0x80, 0xF0, 0x80, 0x80, 0x80},
  {0x70, 0x88, 0x80, 0xB8, 0x88, 0x88, 0x78},
  {0x88, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x88},
  {0x70, 0x20, 0x20, 0x20, 0x20, 0x20, 0x70},
  {0x38, 0x10, 0x10, 0x10, 0x10, 0x90, 0x60},
  {0x88, 0x90, 0xA0, 0xC0, 0xA0, 0x90, 0x88},
  {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xF8},
  {0x88, 0xD8, 0xA8, 0xA8, 0x88, 0x88, 0x88},
  {0x88, 0xC8, 0xA8, 0x98, 0x88, 0x88, 0x88},
  {0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70},
  {0xF0, 0x88, 0x88, 0xF0, 0x80, 0x80, 0x80},
  {0x70, 0x88, 0x88, 0x88, 0xA8, 0x90, 0x68},
  {0xF0, 0x88, 0x88, 0xF0, 0xA0, 0x90, 0x88},
  {0x78, 0x80, 0x80, 0x70, 0x08, 0x08, 0xF0},
  {0xF8, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20},
  {0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70},
  {0x88, 0x88, 0x88, 0x88, 0x88, 0x50, 0x20},
  {0x88, 0x88, 0x88, 0xA8, 0xA8, 0xD8, 0x88},
  {0x88, 0x88, 0x50, 0x20, 0x50, 0x88, 0x88},
  {0x88, 0x88, 0x50, 0x20, 0x20, 0x20, 0x20},
  {0xF8, 0x08, 0x10, 0x20, 0x40, 0x80, 0xF8},
};

//
// Chinese font 16x16 bitmap (generated from system font)
// Order: da zhuan kuai you xi kai shi guan ka fen zui gao sheng ming jie shu sheng2 li kong ge shu
//
STATIC UINT16 FontChinese[26][16] = {
  // 0
  {0x0000,0x0800,0x0800,0x09F8,0x3E20,0x0820,0x0820,0x0E20,0x3820,0x0820,0x0820,0x0820,0x18E0,0x0000,0x0000,0x0000},
  // 1
  {0x0000,0x0000,0x0040,0x3DF8,0x1040,0x1080,0x3DF8,0x3480,0x14F8,0x1410,0x1DA0,0x1440,0x0030,0x0000,0x0000,0x0000},
  // 2
  {0x0000,0x0000,0x1080,0x1080,0x13F0,0x3C90,0x1090,0x1490,0x13E8,0x1CC0,0x3120,0x0310,0x0608,0x0000,0x0000,0x0000},
  // 3
  {0x0000,0x0000,0x3220,0x107C,0x0F40,0x24F8,0x1708,0x0510,0x057C,0x1510,0x2510,0x2910,0x2B70,0x0000,0x0000,0x0000},
  // 4
  {0x0000,0x0000,0x00A0,0x3C90,0x0488,0x04F0,0x3780,0x1890,0x08A0,0x0C60,0x1440,0x20AC,0x2318,0x0000,0x0000,0x0000},
  // 5
  {0x0000,0x0000,0x1FF8,0x0640,0x0640,0x0640,0x0640,0x3FF8,0x0440,0x0440,0x0C40,0x1840,0x3040,0x0000,0x0000,0x0000},
  // 6
  {0x0000,0x1000,0x10C0,0x1080,0x3D10,0x1538,0x15C8,0x2400,0x29F8,0x1908,0x0D08,0x15F8,0x2108,0x0000,0x0000,0x0000},
  // 7
  {0x0000,0x0440,0x0640,0x1FF0,0x0100,0x0100,0x0100,0x3FF8,0x0280,0x02C0,0x0C70,0x3018,0x0000,0x0000,0x0000,0x0000},
  // 8
  {0x0000,0x0100,0x01F0,0x0100,0x0100,0x3FFC,0x0100,0x0180,0x0160,0x0130,0x0100,0x0100,0x0000,0x0000,0x0000,0x0000},
  // 9
  {0x0000,0x0000,0x0480,0x0440,0x0820,0x1830,0x3FE8,0x0220,0x0220,0x0620,0x0420,0x0820,0x30C0,0x0000,0x0000,0x0000},
  // 10
  {0x0000,0x0000,0x0FE0,0x0FE0,0x0820,0x07C0,0x3FF8,0x1200,0x1EF0,0x1EA0,0x1260,0x3E60,0x0298,0x0000,0x0000,0x0000},
  // 11
  {0x0000,0x0000,0x0100,0x3FF8,0x0FE0,0x0820,0x0FE0,0x0000,0x3FF8,0x27C8,0x2448,0x27C8,0x2018,0x0000,0x0000,0x0000},
  // 12
  {0x0000,0x0000,0x0100,0x1900,0x1100,0x1FF0,0x2100,0x2100,0x0100,0x0FE0,0x0100,0x0100,0x0100,0x3FF8,0x0000,0x0000},
  // 13
  {0x0000,0x0100,0x0300,0x0280,0x0C60,0x17D8,0x2000,0x1EF0,0x1290,0x1290,0x1EB0,0x10A0,0x0080,0x0000,0x0000,0x0000},
  // 14
  {0x0000,0x0040,0x1840,0x1244,0x25F8,0x3C40,0x09F8,0x1000,0x3DF8,0x3108,0x0108,0x3DF8,0x0108,0x0000,0x0000,0x0000},
  // 15
  {0x0000,0x0000,0x0100,0x3FF8,0x0100,0x1FF0,0x1110,0x1FF0,0x0380,0x0540,0x0960,0x3118,0x2108,0x0000,0x0000,0x0000},
  // 16
  {0x0000,0x0000,0x1C40,0x2540,0x25F8,0x1D40,0x2640,0x2440,0x1DF0,0x2440,0x2440,0x2440,0x2FF8,0x0000,0x0000,0x0000},
  // 17
  {0x0000,0x0100,0x1F08,0x0408,0x0448,0x3FC8,0x0448,0x0E48,0x1D48,0x1448,0x2408,0x0408,0x0438,0x0000,0x0000,0x0000},
  // 18
  {0x0000,0x0100,0x3FF8,0x0100,0x1FF0,0x1110,0x1FF0,0x1110,0x1FF0,0x0100,0x3FF8,0x0100,0x0100,0x0000,0x0000,0x0000},
  // 19
  {0x0000,0x0040,0x1840,0x1244,0x25F8,0x3C40,0x09F8,0x1000,0x3DF8,0x3108,0x0108,0x3DF8,0x0108,0x0000,0x0000,0x0000},
  // 20
  {0x0000,0x2540,0x1640,0x3FF8,0x1690,0x2690,0x0990,0x3E50,0x1260,0x1E20,0x0770,0x3998,0x0000,0x0000,0x0000,0x0000},
  // 21
  {0x0300,0x0200,0x3FFE,0x0200,0x0600,0x0400,0x0FFC,0x08C0,0x18C0,0x30C0,0x60C0,0x40C0,0x1FFE,0x0000,0x0000,0x0000},
  // 22
  {0x0100,0x0300,0x7FFE,0x0200,0x0600,0x0400,0x0FF8,0x1C08,0x3C08,0x6C08,0x4C08,0x0FF8,0x0FF8,0x0C08,0x0000,0x0000},
  // 23
  {0x08FC,0x0884,0x7F68,0x0810,0x18F0,0x1DB0,0x3A3E,0x28C6,0x48A4,0x0818,0x0830,0x09E0,0x0100,0x0000,0x0000,0x0000},
  // 24
  {0x1930,0x1118,0x1100,0x3FFE,0x0200,0x0200,0x07F8,0x0E18,0x0B10,0x11A0,0x60C0,0x01E0,0x1E1E,0x0800,0x0000,0x0000},
  // 25
  {0x3F08,0x2108,0x3FFE,0x2108,0x3F08,0x2148,0x2168,0x7F08,0x0D08,0x0908,0x3108,0x6738,0x0000,0x0000,0x0000,0x0000}
};

// Font data order: 打砖块游戏开始关卡分最高生命结束胜利空格数
// Index:           0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20
#define CHINESE_DA       0   // da
#define CHINESE_ZHUAN    1   // zhuan
#define CHINESE_KUAI     2   // kuai
#define CHINESE_YOU      3   // you
#define CHINESE_XI       4   // xi
#define CHINESE_KAI      5   // kai
#define CHINESE_SHI      6   // shi
#define CHINESE_GUAN     7   // guan
#define CHINESE_KA       8   // ka
#define CHINESE_FEN      9   // fen
#define CHINESE_ZUI      10  // zui
#define CHINESE_GAO      11  // gao
#define CHINESE_SHENG    12  // sheng
#define CHINESE_MING     13  // ming
#define CHINESE_JIE      14  // jie
#define CHINESE_SHU      15  // shu (end)
#define CHINESE_SHENGWIN 16  // sheng (win)
#define CHINESE_LI       17  // li
#define CHINESE_KONG     18  // kong
#define CHINESE_GE       19  // ge
#define CHINESE_SHU_NUM  20  // shu (number)
#define CHINESE_ZUO      21  // zuo (left)
#define CHINESE_YOU_DIR  22  // you (right)
#define CHINESE_YI       23  // yi (move)
#define CHINESE_FA       24  // fa (launch)
#define CHINESE_SHE      25  // she (shoot)

// Star field data
#define MAX_STARS 150
STATIC INT32  mStarX[MAX_STARS];
STATIC INT32  mStarY[MAX_STARS];
STATIC INT32  mStarBright[MAX_STARS];
STATIC BOOLEAN mStarsInitialized = FALSE;

//
// Function prototypes
//
VOID DrawRect (INT32 X, INT32 Y, INT32 Width, INT32 Height, UINT32 Color);
VOID DrawDigit (INT32 X, INT32 Y, INT32 Digit, UINT32 Color);
VOID DrawLetter (INT32 X, INT32 Y, CHAR8 Letter, UINT32 Color);
VOID DrawText (INT32 X, INT32 Y, CHAR8 *Text, UINT32 Color);
VOID DrawNumber (INT32 X, INT32 Y, UINTN Number, UINT32 Color);
VOID DrawChineseChar (INT32 X, INT32 Y, INT32 Index, UINT32 Color);
VOID DrawChineseText (INT32 X, INT32 Y, INT32 *Indices, INT32 Count, UINT32 Color);
VOID InitStars (VOID);
VOID DrawStarfield (VOID);
VOID DrawBackground (VOID);
VOID DrawSmallBlock (INT32 X, INT32 Y, UINT32 Color);
VOID DrawHeart (INT32 X, INT32 Y, UINT32 Color);
VOID DrawInfoPanel (VOID);
VOID DrawPaddle (VOID);
VOID ClearPaddle (VOID);
VOID DrawBalls (VOID);
VOID ClearBalls (VOID);
VOID DrawBricks (VOID);
VOID ClearBrick (UINT32 Row, UINT32 Col);
VOID DrawPowerUps (VOID);
VOID ClearPowerUps (VOID);
VOID DrawParticles (VOID);
VOID ClearParticles (VOID);
VOID DrawStartScreen (VOID);
VOID DrawGameOverScreen (VOID);
VOID DrawWinScreen (VOID);
VOID InitializeGame (VOID);
VOID InitializeLevel (UINTN Level);
VOID UpdateBalls (VOID);
VOID UpdatePowerUps (VOID);
VOID UpdateParticles (VOID);
VOID SpawnParticle (INT32 X, INT32 Y, UINT32 Color);
VOID SpawnPowerUp (INT32 X, INT32 Y);
BOOLEAN CheckPaddleCollision (BALL *Ball);
VOID CheckBrickCollision (BALL *Ball);
EFI_STATUS HandleInput (OUT BOOLEAN *ExitGame);

VOID DrawRect (INT32 X, INT32 Y, INT32 Width, INT32 Height, UINT32 Color)
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL_UNION  Pixel;

  if ((X < 0) || (Y < 0) || (Width <= 0) || (Height <= 0)) return;
  if ((UINT32)(X + Width) > mScreenWidth) Width = mScreenWidth - X;
  if ((UINT32)(Y + Height) > mScreenHeight) Height = mScreenHeight - Y;
  if (Width <= 0 || Height <= 0) return;

  Pixel.Raw = Color;
  mGop->Blt (mGop, &Pixel.Pixel, EfiBltVideoFill, 0, 0, X, Y, Width, Height, 0);
}

VOID DrawDigit (INT32 X, INT32 Y, INT32 Digit, UINT32 Color)
{
  INT32 Row, Col;
  UINT8 RowData;

  if (Digit < 0 || Digit > 9) return;

  for (Row = 0; Row < 7; Row++) {
    RowData = FontDigits[Digit][Row];
    for (Col = 0; Col < 5; Col++) {
      if (RowData & (0x80 >> Col)) {
        DrawRect (X + Col * 3, Y + Row * 3, 3, 3, Color);
      }
    }
  }
}

VOID DrawLetter (INT32 X, INT32 Y, CHAR8 Letter, UINT32 Color)
{
  INT32 Row, Col;
  INT32 Index;
  UINT8 RowData;

  if (Letter >= 'A' && Letter <= 'Z') {
    Index = Letter - 'A';
  } else if (Letter >= 'a' && Letter <= 'z') {
    Index = Letter - 'a';
  } else {
    return;
  }

  for (Row = 0; Row < 7; Row++) {
    RowData = FontLetters[Index][Row];
    for (Col = 0; Col < 5; Col++) {
      if (RowData & (0x80 >> Col)) {
        DrawRect (X + Col * 3, Y + Row * 3, 3, 3, Color);
      }
    }
  }
}

VOID DrawText (INT32 X, INT32 Y, CHAR8 *Text, UINT32 Color)
{
  INT32 Offset = 0;
  INT32 i = 0;

  while (Text[i] != '\0') {
    if (Text[i] >= 'A' && Text[i] <= 'Z') {
      DrawLetter (X + Offset, Y, Text[i], Color);
      Offset += 18;
    } else if (Text[i] >= 'a' && Text[i] <= 'z') {
      DrawLetter (X + Offset, Y, Text[i], Color);
      Offset += 18;
    } else if (Text[i] == ' ') {
      Offset += 12;
    }
    i++;
  }
}

VOID DrawNumber (INT32 X, INT32 Y, UINTN Number, UINT32 Color)
{
  INT32 Digits[10];
  INT32 Count = 0;
  INT32 i;

  if (Number == 0) {
    DrawDigit (X, Y, 0, Color);
    return;
  }

  while (Number > 0 && Count < 10) {
    Digits[Count++] = Number % 10;
    Number /= 10;
  }

  for (i = Count - 1; i >= 0; i--) {
    DrawDigit (X + (Count - 1 - i) * 18, Y, Digits[i], Color);
  }
}

VOID DrawChineseChar (INT32 X, INT32 Y, INT32 Index, UINT32 Color)
{
  INT32 Row, Col;
  UINT16 RowData;

  if (Index < 0 || Index >= 26) return;

  for (Row = 0; Row < 16; Row++) {
    RowData = FontChinese[Index][Row];
    for (Col = 0; Col < 16; Col++) {
      // Check bit from MSB (bit 15) to LSB
      if (RowData & (0x8000 >> Col)) {
        DrawRect (X + Col * 2, Y + Row * 2, 2, 2, Color);
      }
    }
  }
}

VOID DrawChineseText (INT32 X, INT32 Y, INT32 *Indices, INT32 Count, UINT32 Color)
{
  INT32 i;
  for (i = 0; i < Count; i++) {
    DrawChineseChar (X + i * 36, Y, Indices[i], Color);
  }
}

VOID InitStars (VOID)
{
  INT32 i;
  if (mStarsInitialized) return;

  for (i = 0; i < MAX_STARS; i++) {
    mStarX[i] = Random() % (INT32)mScreenWidth;
    mStarY[i] = Random() % (INT32)mScreenHeight;
    mStarBright[i] = 80 + (Random() % 120);
  }
  mStarsInitialized = TRUE;
}

VOID DrawStarfield (VOID)
{
  INT32 i;
  UINT32 Color;

  for (i = 0; i < MAX_STARS; i++) {
    Color = 0xFF000000 | (mStarBright[i] << 16) | (mStarBright[i] << 8) | mStarBright[i];
    DrawRect (mStarX[i], mStarY[i], 2, 2, Color);
  }
}

VOID DrawBackground (VOID)
{
  // Dark background
  DrawRect (0, 0, mScreenWidth, mScreenHeight, COLOR_BG_DARK);
  // Starfield
  DrawStarfield ();
}

VOID DrawSmallBlock (INT32 X, INT32 Y, UINT32 Color)
{
  DrawRect (X, Y, 15, 15, Color);
  DrawRect (X + 2, Y + 2, 11, 11, COLOR_WHITE);
}

VOID DrawHeart (INT32 X, INT32 Y, UINT32 Color)
{
  DrawRect (X, Y + 3, 3, 6, Color);
  DrawRect (X + 3, Y + 3, 3, 6, Color);
  DrawRect (X + 6, Y + 4, 3, 5, Color);
  DrawRect (X + 9, Y + 5, 3, 4, Color);
  DrawRect (X + 1, Y + 1, 3, 3, Color);
  DrawRect (X + 5, Y + 1, 3, 3, Color);
  DrawRect (X + 3, Y, 3, 2, Color);
}

VOID DrawInfoPanel (VOID)
{
  UINT32 PanelX = mInfoPanelX;
  UINT32 Y;
  UINTN i;
  // 打砖块游戏 (5 chars)
  INT32 TitleChars[5] = {CHINESE_DA, CHINESE_ZHUAN, CHINESE_KUAI, CHINESE_YOU, CHINESE_XI};
  // 关卡 (2 chars)
  INT32 LevelChars[2] = {CHINESE_GUAN, CHINESE_KA};
  // 分数 (2 chars)
  INT32 ScoreChars[2] = {CHINESE_FEN, CHINESE_SHU_NUM};
  // 最高 (2 chars)
  INT32 BestChars[2] = {CHINESE_ZUI, CHINESE_GAO};

  DrawRect (PanelX, 0, INFO_PANEL_WIDTH, mScreenHeight, COLOR_PANEL_BG);
  DrawRect (PanelX, 0, 4, mScreenHeight, COLOR_PANEL_BORDER);

  // Title
  Y = 5;
  DrawRect (PanelX + 5, Y, INFO_PANEL_WIDTH - 10, 75, 0xFF205060);
  DrawChineseText (PanelX + 2, Y + 8, TitleChars, 5, COLOR_CYAN);

  // Level
  Y = 95;
  DrawRect (PanelX + 5, Y, INFO_PANEL_WIDTH - 10, 3, COLOR_CYAN);
  DrawChineseText (PanelX + 8, Y + 8, LevelChars, 2, COLOR_CYAN);
  DrawNumber (PanelX + 100, Y + 12, mLevel, COLOR_WHITE);

  // Score
  Y = 180;
  DrawRect (PanelX + 5, Y, INFO_PANEL_WIDTH - 10, 3, COLOR_YELLOW);
  DrawChineseText (PanelX + 8, Y + 8, ScoreChars, 2, COLOR_YELLOW);
  DrawNumber (PanelX + 100, Y + 12, mScore, COLOR_WHITE);

  // High Score
  Y = 265;
  DrawRect (PanelX + 5, Y, INFO_PANEL_WIDTH - 10, 3, COLOR_GOLD);
  DrawChineseText (PanelX + 8, Y + 8, BestChars, 2, COLOR_GOLD);
  DrawNumber (PanelX + 100, Y + 12, mHighScore, COLOR_GOLD);

  // Lives
  Y = 350;
  DrawRect (PanelX + 5, Y, INFO_PANEL_WIDTH - 10, 3, COLOR_PINK);
  for (i = 0; i < mLives && i < 5; i++) {
    DrawHeart (PanelX + 20 + i * 35, Y + 12, COLOR_RED);
  }

  // Controls
  Y = 430;
  DrawRect (PanelX + 5, Y, INFO_PANEL_WIDTH - 10, 3, COLOR_GRAY);
  DrawText (PanelX + 10, Y + 10, "A D", COLOR_DARK_GRAY);
  DrawText (PanelX + 10, Y + 40, "SPACE", COLOR_DARK_GRAY);
}

VOID DrawPaddle (VOID)
{
  DrawRect (mPaddle.X - 2, mPaddle.Y - 2, mPaddle.Width + 4, mPaddle.Height + 4, 0x202080C0);
  DrawRect (mPaddle.X, mPaddle.Y, mPaddle.Width, mPaddle.Height, COLOR_CYAN);
  DrawRect (mPaddle.X + 4, mPaddle.Y + 2, mPaddle.Width - 8, 4, COLOR_WHITE);
}

VOID ClearPaddle (VOID)
{
  DrawRect (mPaddle.OldX - 2, mPaddle.Y - 2, mPaddle.Width + 8, mPaddle.Height + 4, COLOR_BG_DARK);
}

VOID DrawBalls (VOID)
{
  UINTN i;

  for (i = 0; i < MAX_BALLS; i++) {
    if (mBalls[i].Active) {
      if (mBalls[i].FireBall) {
        DrawRect (mBalls[i].X - 1, mBalls[i].Y - 1, mBalls[i].Size + 2, mBalls[i].Size + 2, 0x30FF4020);
        DrawRect (mBalls[i].X, mBalls[i].Y, mBalls[i].Size, mBalls[i].Size, COLOR_ORANGE);
        DrawRect (mBalls[i].X + 3, mBalls[i].Y + 3, 4, 4, COLOR_YELLOW);
      } else {
        DrawRect (mBalls[i].X - 1, mBalls[i].Y - 1, mBalls[i].Size + 2, mBalls[i].Size + 2, 0x20FFFFFF);
        DrawRect (mBalls[i].X, mBalls[i].Y, mBalls[i].Size, mBalls[i].Size, COLOR_WHITE);
        DrawRect (mBalls[i].X + 3, mBalls[i].Y + 3, 3, 3, 0xFFD0D0D0);
      }
    }
  }
}

VOID ClearBalls (VOID)
{
  UINTN i;

  for (i = 0; i < MAX_BALLS; i++) {
    if (mBalls[i].Active) {
      DrawRect (mBalls[i].OldX - 2, mBalls[i].OldY - 2, mBalls[i].Size + 4, mBalls[i].Size + 4, COLOR_BG_DARK);
    }
  }
}

VOID DrawBricks (VOID)
{
  UINT32  Row, Col;
  UINT32  BorderColor, InnerColor;

  for (Row = 0; Row < BRICK_ROWS; Row++) {
    for (Col = 0; Col < BRICK_COLS; Col++) {
      if (mBricks[Row][Col].Active) {
        BorderColor = COLOR_DARK_GRAY;
        InnerColor = mBricks[Row][Col].Color;

        switch (mBricks[Row][Col].Type) {
          case BrickTypeStrong: BorderColor = 0xFF303050; break;
          case BrickTypeVeryStrong: BorderColor = 0xFF404060; break;
          case BrickTypeGold: BorderColor = COLOR_ORANGE; break;
          case BrickTypeExplosive: BorderColor = COLOR_RED; break;
          case BrickTypeUnbreakable: BorderColor = 0xFF505050; InnerColor = COLOR_GRAY; break;
          default: break;
        }

        DrawRect (mBricks[Row][Col].X + 2, mBricks[Row][Col].Y + 2,
                  mBricks[Row][Col].Width, mBricks[Row][Col].Height, 0x40000000);

        DrawRect (mBricks[Row][Col].X, mBricks[Row][Col].Y,
                  mBricks[Row][Col].Width, mBricks[Row][Col].Height, BorderColor);

        DrawRect (mBricks[Row][Col].X + 2, mBricks[Row][Col].Y + 2,
                  mBricks[Row][Col].Width - 4, mBricks[Row][Col].Height - 4, InnerColor);

        DrawRect (mBricks[Row][Col].X + 4, mBricks[Row][Col].Y + 3,
                  mBricks[Row][Col].Width - 8, 3, COLOR_WHITE);

        if ((mBricks[Row][Col].Type == BrickTypeStrong && mBricks[Row][Col].Hits >= 1) ||
            (mBricks[Row][Col].Type == BrickTypeVeryStrong && mBricks[Row][Col].Hits >= 1)) {
          DrawRect (mBricks[Row][Col].X + mBricks[Row][Col].Width / 2 - 5,
                    mBricks[Row][Col].Y + mBricks[Row][Col].Height / 2 - 1, 10, 2, COLOR_BLACK);
        }

        if (mBricks[Row][Col].Type == BrickTypeExplosive) {
          DrawRect (mBricks[Row][Col].X + mBricks[Row][Col].Width / 2 - 4,
                    mBricks[Row][Col].Y + mBricks[Row][Col].Height / 2 - 4, 8, 8, COLOR_YELLOW);
        }
      }
    }
  }
}

VOID ClearBrick (UINT32 Row, UINT32 Col)
{
  DrawRect (mBricks[Row][Col].X, mBricks[Row][Col].Y,
            mBricks[Row][Col].Width + 4, mBricks[Row][Col].Height + 4, COLOR_BG_DARK);
}

VOID DrawPowerUps (VOID)
{
  UINTN i;

  for (i = 0; i < 10; i++) {
    if (mPowerUps[i].Active) {
      DrawRect (mPowerUps[i].X - 1, mPowerUps[i].Y - 1,
                mPowerUps[i].Width + 2, mPowerUps[i].Height + 2, 0x30FFFFFF);
      DrawRect (mPowerUps[i].X, mPowerUps[i].Y,
                mPowerUps[i].Width, mPowerUps[i].Height, COLOR_WHITE);
      DrawRect (mPowerUps[i].X + 3, mPowerUps[i].Y + 3,
                mPowerUps[i].Width - 6, mPowerUps[i].Height - 6, mPowerUps[i].Color);
    }
  }
}

VOID ClearPowerUps (VOID)
{
  UINTN i;

  for (i = 0; i < 10; i++) {
    if (mPowerUps[i].Active) {
      DrawRect (mPowerUps[i].X - 2, mPowerUps[i].OldY - 2,
                mPowerUps[i].Width + 4, mPowerUps[i].Height + 4, COLOR_BG_DARK);
    }
  }
}

VOID DrawParticles (VOID)
{
  UINTN i;

  for (i = 0; i < MAX_PARTICLES; i++) {
    if (mParticles[i].Active) {
      DrawRect (mParticles[i].X, mParticles[i].Y, 3, 3, mParticles[i].Color);
    }
  }
}

VOID ClearParticles (VOID)
{
  UINTN i;

  for (i = 0; i < MAX_PARTICLES; i++) {
    if (mParticles[i].Active) {
      DrawRect (mParticles[i].X, mParticles[i].Y, 4, 4, COLOR_BG_DARK);
    }
  }
}

VOID DrawStartScreen (VOID)
{
  UINT32 BoxWidth = 550;
  UINT32 BoxHeight = 380;
  UINT32 BoxX = mGameAreaX + (mGameAreaWidth - BoxWidth) / 2;
  UINT32 BoxY = (mScreenHeight - BoxHeight) / 2;
  UINT32 TitleBoxWidth = BoxWidth - 60;
  UINT32 ButtonWidth = 200;
  // 打砖块游戏 (5 chars, each char renders as 32px + 4px spacing = 36px per char in DrawChineseText)
  INT32 TitleChars[5] = {CHINESE_DA, CHINESE_ZHUAN, CHINESE_KUAI, CHINESE_YOU, CHINESE_XI};
  // 开始 (2 chars)
  INT32 StartChars[2] = {CHINESE_KAI, CHINESE_SHI};
  // Calculate text widths for centering
  UINT32 TitleTextWidth = 5 * 36;  // 5 chars * 36px spacing
  UINT32 StartTextWidth = 2 * 36;  // 2 chars * 36px spacing
  UINT32 TitleTextX = BoxX + 30 + (TitleBoxWidth - TitleTextWidth) / 2;
  UINT32 StartTextX = BoxX + 175 + (ButtonWidth - StartTextWidth) / 2;

  DrawRect (BoxX + 6, BoxY + 6, BoxWidth, BoxHeight, 0x50000000);
  DrawRect (BoxX - 3, BoxY - 3, BoxWidth + 6, BoxHeight + 6, COLOR_PANEL_BORDER);
  DrawRect (BoxX, BoxY, BoxWidth, BoxHeight, 0xFF182838);

  // Title - centered
  DrawRect (BoxX + 30, BoxY + 20, TitleBoxWidth, 75, COLOR_CYAN);
  DrawChineseText (TitleTextX, BoxY + 32, TitleChars, 5, COLOR_WHITE);

  // Start button - centered
  DrawRect (BoxX + 175, BoxY + 130, ButtonWidth, 75, COLOR_GREEN);
  DrawRect (BoxX + 178, BoxY + 133, ButtonWidth - 6, 69, 0xFF40A060);
  DrawChineseText (StartTextX, BoxY + 148, StartChars, 2, COLOR_WHITE);

  // Controls
  DrawRect (BoxX + 30, BoxY + 240, BoxWidth - 60, 100, 0xFF203040);
  // Left: "左移 A"
  INT32 LeftMoveChars[2] = {CHINESE_ZUO, CHINESE_YI};
  DrawChineseText (BoxX + 60, BoxY + 258, LeftMoveChars, 2, COLOR_CYAN);
  DrawText (BoxX + 140, BoxY + 260, "A", COLOR_WHITE);
  // Right: "右移 D"
  INT32 RightMoveChars[2] = {CHINESE_YOU_DIR, CHINESE_YI};
  DrawChineseText (BoxX + 280, BoxY + 258, RightMoveChars, 2, COLOR_CYAN);
  DrawText (BoxX + 360, BoxY + 260, "D", COLOR_WHITE);
  // Launch: "发射 SPACE"
  INT32 LaunchChars[2] = {CHINESE_FA, CHINESE_SHE};
  DrawChineseText (BoxX + 160, BoxY + 298, LaunchChars, 2, COLOR_YELLOW);
  DrawText (BoxX + 240, BoxY + 300, "SPACE", COLOR_WHITE);
}

VOID DrawGameOverScreen (VOID)
{
  UINT32 BoxWidth = 480;
  UINT32 BoxHeight = 350;
  UINT32 BoxX = mGameAreaX + (mGameAreaWidth - BoxWidth) / 2;
  UINT32 BoxY = (mScreenHeight - BoxHeight) / 2;
  INT32 OverChars[2] = {CHINESE_JIE, CHINESE_SHU};
  INT32 ScoreChars[2] = {CHINESE_FEN, CHINESE_SHU_NUM};

  DrawRect (BoxX + 6, BoxY + 6, BoxWidth, BoxHeight, 0x50000000);
  DrawRect (BoxX - 3, BoxY - 3, BoxWidth + 6, BoxHeight + 6, COLOR_RED);
  DrawRect (BoxX, BoxY, BoxWidth, BoxHeight, 0xFF302020);

  // Title
  DrawRect (BoxX + 30, BoxY + 20, BoxWidth - 60, 75, COLOR_RED);
  DrawChineseText (BoxX + 190, BoxY + 32, OverChars, 2, COLOR_WHITE);

  // Score
  DrawChineseText (BoxX + 50, BoxY + 130, ScoreChars, 2, COLOR_YELLOW);
  DrawNumber (BoxX + 140, BoxY + 135, mScore, COLOR_WHITE);

  // Restart hint
  DrawRect (BoxX + 100, BoxY + 220, BoxWidth - 200, 70, COLOR_DARK_GRAY);
  DrawText (BoxX + 140, BoxY + 245, "SPACE", COLOR_WHITE);
}

VOID DrawWinScreen (VOID)
{
  UINT32 BoxWidth = 480;
  UINT32 BoxHeight = 350;
  UINT32 BoxX = mGameAreaX + (mGameAreaWidth - BoxWidth) / 2;
  UINT32 BoxY = (mScreenHeight - BoxHeight) / 2;
  INT32 WinChars[2] = {CHINESE_SHENGWIN, CHINESE_LI};
  INT32 ScoreChars[2] = {CHINESE_FEN, CHINESE_SHU_NUM};

  DrawRect (BoxX + 6, BoxY + 6, BoxWidth, BoxHeight, 0x50000000);
  DrawRect (BoxX - 3, BoxY - 3, BoxWidth + 6, BoxHeight + 6, COLOR_GOLD);
  DrawRect (BoxX, BoxY, BoxWidth, BoxHeight, 0xFF203020);

  // Title
  DrawRect (BoxX + 30, BoxY + 20, BoxWidth - 60, 75, COLOR_GOLD);
  DrawChineseText (BoxX + 190, BoxY + 32, WinChars, 2, COLOR_WHITE);

  // Score
  DrawChineseText (BoxX + 50, BoxY + 130, ScoreChars, 2, COLOR_YELLOW);
  DrawNumber (BoxX + 140, BoxY + 135, mScore, COLOR_WHITE);
}

EFI_STATUS InitializeGraphics (VOID)
{
  EFI_STATUS  Status;

  Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&mGop);
  if (EFI_ERROR (Status)) return Status;

  mScreenWidth  = mGop->Mode->Info->HorizontalResolution;
  mScreenHeight = mGop->Mode->Info->VerticalResolution;

  mGameAreaWidth = mScreenWidth - INFO_PANEL_WIDTH;
  mGameAreaX = 0;
  mInfoPanelX = mGameAreaWidth;

  return EFI_SUCCESS;
}

EFI_STATUS InitializeInput (VOID)
{
  EFI_STATUS  Status;

  Status = gBS->LocateProtocol (&gEfiSimpleTextInputExProtocolGuid, NULL, (VOID **)&mTextInputEx);
  if (EFI_ERROR (Status)) mTextInputEx = NULL;

  return EFI_SUCCESS;
}

VOID InitializeGame (VOID)
{
  mScore = 0;
  mLives = 3;
  mLevel = 1;
  mGameOver = FALSE;
  mGameWon = FALSE;
  mGameStarted = FALSE;
  mWidePaddleActive = FALSE;
  mSlowBallActive = FALSE;
  mComboCount = 0;

  InitializeLevel (mLevel);
}

VOID InitializeLevel (UINTN Level)
{
  UINT32  Row, Col;
  UINT32  StartX;
  UINTN   i;

  UINT32  RowColors[BRICK_ROWS] = {
    COLOR_RED, COLOR_ORANGE, COLOR_YELLOW, COLOR_GREEN, COLOR_CYAN, COLOR_BLUE
  };

  mPaddle.Width = mWidePaddleActive ? PADDLE_WIDTH_WIDE : PADDLE_WIDTH_DEFAULT;
  mPaddle.X = mGameAreaX + (mGameAreaWidth - mPaddle.Width) / 2;
  mPaddle.Y = mScreenHeight - PADDLE_HEIGHT - 60;
  mPaddle.Height = PADDLE_HEIGHT;
  mPaddle.Color = COLOR_CYAN;
  mPaddle.OldX = mPaddle.X;

  for (i = 0; i < MAX_BALLS; i++) {
    mBalls[i].Active = FALSE;
    mBalls[i].Size = BALL_SIZE;
    mBalls[i].Color = COLOR_WHITE;
    mBalls[i].FireBall = FALSE;
  }

  mBalls[0].Active = TRUE;
  mBalls[0].X = mPaddle.X + mPaddle.Width / 2 - BALL_SIZE / 2;
  mBalls[0].Y = mPaddle.Y - BALL_SIZE - 20;
  mBalls[0].OldX = mBalls[0].X;
  mBalls[0].OldY = mBalls[0].Y;
  mBalls[0].DX = BALL_SPEED_DEFAULT;
  mBalls[0].DY = -BALL_SPEED_DEFAULT;

  StartX = mGameAreaX + 25;

  for (Row = 0; Row < BRICK_ROWS; Row++) {
    for (Col = 0; Col < BRICK_COLS; Col++) {
      mBricks[Row][Col].X = StartX + Col * (BRICK_WIDTH + BRICK_PADDING);
      mBricks[Row][Col].Y = BRICK_TOP_OFFSET + Row * (BRICK_HEIGHT + BRICK_PADDING);
      mBricks[Row][Col].Width = BRICK_WIDTH;
      mBricks[Row][Col].Height = BRICK_HEIGHT;
      mBricks[Row][Col].Color = RowColors[Row];
      mBricks[Row][Col].Active = TRUE;
      mBricks[Row][Col].Hits = 0;
      mBricks[Row][Col].Type = BrickTypeNormal;

      if (Level >= 2) {
        if (Row == 0) mBricks[Row][Col].Type = BrickTypeStrong;
        if ((Col == 0 || Col == BRICK_COLS - 1) && Row < 2) {
          mBricks[Row][Col].Type = BrickTypeUnbreakable;
          mBricks[Row][Col].Color = COLOR_GRAY;
        }
      }

      if (Level >= 3) {
        if (Row == 0) mBricks[Row][Col].Type = BrickTypeVeryStrong;
        if ((Row + Col) % 7 == 0) {
          mBricks[Row][Col].Type = BrickTypeGold;
          mBricks[Row][Col].Color = COLOR_GOLD;
        }
        if ((Row + Col) % 5 == 0 && Row > 1) {
          mBricks[Row][Col].Type = BrickTypeExplosive;
          mBricks[Row][Col].Color = COLOR_RED;
        }
      }
    }
  }

  for (i = 0; i < 10; i++) {
    mPowerUps[i].Active = FALSE;
    mPowerUps[i].Width = 28;
    mPowerUps[i].Height = 14;
    mPowerUps[i].Speed = 3;
  }

  for (i = 0; i < MAX_PARTICLES; i++) {
    mParticles[i].Active = FALSE;
  }
}

VOID SpawnParticle (INT32 X, INT32 Y, UINT32 Color)
{
  UINTN i;

  for (i = 0; i < MAX_PARTICLES; i++) {
    if (!mParticles[i].Active) {
      mParticles[i].X = X;
      mParticles[i].Y = Y;
      mParticles[i].VX = (Random() % 7) - 3;
      mParticles[i].VY = (Random() % 7) - 3;
      mParticles[i].Color = Color;
      mParticles[i].Life = 12 + (Random() % 10);
      mParticles[i].Active = TRUE;
      break;
    }
  }
}

VOID SpawnExplosionParticles (INT32 X, INT32 Y, UINT32 Color)
{
  UINTN i;
  for (i = 0; i < 8; i++) {
    SpawnParticle (X + (Random() % 20) - 10, Y + (Random() % 20) - 10, Color);
  }
}

VOID UpdateParticles (VOID)
{
  UINTN i;

  for (i = 0; i < MAX_PARTICLES; i++) {
    if (mParticles[i].Active) {
      mParticles[i].X += mParticles[i].VX;
      mParticles[i].Y += mParticles[i].VY;
      mParticles[i].VY += 1;
      mParticles[i].Life--;

      if (mParticles[i].Life <= 0 ||
          mParticles[i].X < 0 || mParticles[i].Y < 0 ||
          (UINT32)mParticles[i].X > mScreenWidth ||
          (UINT32)mParticles[i].Y > mScreenHeight) {
        mParticles[i].Active = FALSE;
      }
    }
  }
}

VOID SpawnPowerUp (INT32 X, INT32 Y)
{
  UINTN i;
  UINTN Type;

  if ((Random() % 100) > 25) return;

  for (i = 0; i < 10; i++) {
    if (!mPowerUps[i].Active) {
      mPowerUps[i].X = X;
      mPowerUps[i].Y = Y;
      mPowerUps[i].OldY = Y;
      mPowerUps[i].Active = TRUE;

      Type = Random() % 5;
      switch (Type) {
        case 0: mPowerUps[i].Type = PowerUpWidePaddle; mPowerUps[i].Color = COLOR_GREEN; break;
        case 1: mPowerUps[i].Type = PowerUpMultiBall; mPowerUps[i].Color = COLOR_ORANGE; break;
        case 2: mPowerUps[i].Type = PowerUpSlowBall; mPowerUps[i].Color = COLOR_BLUE; break;
        case 3: mPowerUps[i].Type = PowerUpExtraLife; mPowerUps[i].Color = COLOR_PINK; break;
        default: mPowerUps[i].Type = PowerUpFireBall; mPowerUps[i].Color = COLOR_RED; break;
      }
      break;
    }
  }
}

VOID UpdatePowerUps (VOID)
{
  UINTN i;

  for (i = 0; i < 10; i++) {
    if (mPowerUps[i].Active) {
      mPowerUps[i].OldY = mPowerUps[i].Y;
      mPowerUps[i].Y += mPowerUps[i].Speed;

      if ((UINT32)mPowerUps[i].Y > mScreenHeight) {
        mPowerUps[i].Active = FALSE;
        continue;
      }

      if (mPowerUps[i].X + mPowerUps[i].Width >= mPaddle.X &&
          mPowerUps[i].X <= mPaddle.X + mPaddle.Width &&
          mPowerUps[i].Y + mPowerUps[i].Height >= mPaddle.Y) {
        mPowerUps[i].Active = FALSE;
        mScore += 50;

        switch (mPowerUps[i].Type) {
          case PowerUpWidePaddle:
            mWidePaddleActive = TRUE;
            mWidePaddleTimer = 600;
            mPaddle.Width = PADDLE_WIDTH_WIDE;
            if (mPaddle.X + mPaddle.Width > (INT32)(mGameAreaX + mGameAreaWidth - 10)) {
              mPaddle.X = mGameAreaX + mGameAreaWidth - mPaddle.Width - 10;
            }
            break;

          case PowerUpMultiBall:
            {
              UINTN j;
              for (j = 0; j < MAX_BALLS; j++) {
                if (!mBalls[j].Active) {
                  mBalls[j].Active = TRUE;
                  mBalls[j].X = mPaddle.X + mPaddle.Width / 2;
                  mBalls[j].Y = mPaddle.Y - BALL_SIZE - 10;
                  mBalls[j].OldX = mBalls[j].X;
                  mBalls[j].OldY = mBalls[j].Y;
                  mBalls[j].DX = (j % 2 == 0) ? BALL_SPEED_DEFAULT : -BALL_SPEED_DEFAULT;
                  mBalls[j].DY = -BALL_SPEED_DEFAULT;
                  mBalls[j].Size = BALL_SIZE;
                  mBalls[j].Color = COLOR_WHITE;
                  mBalls[j].FireBall = FALSE;
                }
              }
            }
            break;

          case PowerUpSlowBall:
            mSlowBallActive = TRUE;
            mSlowBallTimer = 400;
            break;

          case PowerUpExtraLife:
            mLives++;
            break;

          case PowerUpFireBall:
            {
              UINTN j;
              for (j = 0; j < MAX_BALLS; j++) {
                if (mBalls[j].Active) mBalls[j].FireBall = TRUE;
              }
            }
            break;

          default: break;
        }
      }
    }
  }
}

VOID UpdateBalls (VOID)
{
  UINTN   i;
  INT32   Speed;
  BOOLEAN AnyActive;

  Speed = mSlowBallActive ? BALL_SPEED_SLOW : BALL_SPEED_DEFAULT;

  for (i = 0; i < MAX_BALLS; i++) {
    if (!mBalls[i].Active) continue;

    mBalls[i].OldX = mBalls[i].X;
    mBalls[i].OldY = mBalls[i].Y;

    mBalls[i].X += mBalls[i].DX;
    mBalls[i].Y += mBalls[i].DY;

    if (mSlowBallActive) {
      mBalls[i].X += (mBalls[i].DX * (Speed - 1)) / BALL_SPEED_DEFAULT;
      mBalls[i].Y += (mBalls[i].DY * (Speed - 1)) / BALL_SPEED_DEFAULT;
    }

    if (mBalls[i].X <= (INT32)(mGameAreaX + 10)) {
      mBalls[i].DX = ABS(mBalls[i].DX);
      mBalls[i].X = mGameAreaX + 11;
    }
    if (mBalls[i].X + mBalls[i].Size >= (INT32)(mGameAreaX + mGameAreaWidth - 10)) {
      mBalls[i].DX = -ABS(mBalls[i].DX);
      mBalls[i].X = mGameAreaX + mGameAreaWidth - mBalls[i].Size - 11;
    }

    if (mBalls[i].Y <= 10) {
      mBalls[i].DY = ABS(mBalls[i].DY);
      mBalls[i].Y = 11;
    }

    if ((UINT32)(mBalls[i].Y + mBalls[i].Size) >= mScreenHeight - 10) {
      mBalls[i].Active = FALSE;
      mComboCount = 0;
      continue;
    }

    if (CheckPaddleCollision (&mBalls[i])) {
      mBalls[i].DY = -ABS(mBalls[i].DY);

      INT32 HitPos = (mBalls[i].X + mBalls[i].Size / 2) - mPaddle.X;
      INT32 PaddleMid = mPaddle.Width / 2;
      INT32 Offset = HitPos - PaddleMid;

      mBalls[i].DX = Offset / 10;
      if (mBalls[i].DX == 0) mBalls[i].DX = (Offset >= 0) ? 1 : -1;
      if (ABS(mBalls[i].DX) > 7) mBalls[i].DX = (mBalls[i].DX > 0) ? 7 : -7;

      mBalls[i].Y = mPaddle.Y - mBalls[i].Size - 1;
      mBalls[i].FireBall = FALSE;
    }

    CheckBrickCollision (&mBalls[i]);
  }

  AnyActive = FALSE;
  for (i = 0; i < MAX_BALLS; i++) {
    if (mBalls[i].Active) { AnyActive = TRUE; break; }
  }

  if (!AnyActive) {
    mLives--;
    mComboCount = 0;
    if (mLives == 0) {
      mGameOver = TRUE;
    } else {
      mBalls[0].Active = TRUE;
      mBalls[0].X = mPaddle.X + mPaddle.Width / 2 - BALL_SIZE / 2;
      mBalls[0].Y = mPaddle.Y - BALL_SIZE - 20;
      mBalls[0].OldX = mBalls[0].X;
      mBalls[0].OldY = mBalls[0].Y;
      mBalls[0].DX = BALL_SPEED_DEFAULT;
      mBalls[0].DY = -BALL_SPEED_DEFAULT;
      mBalls[0].FireBall = FALSE;
    }
  }
}

BOOLEAN CheckPaddleCollision (BALL *Ball)
{
  INT32 BallRight = Ball->X + Ball->Size;
  INT32 BallBottom = Ball->Y + Ball->Size;
  INT32 PaddleRight = mPaddle.X + mPaddle.Width;
  INT32 PaddleBottom = mPaddle.Y + mPaddle.Height;

  if (Ball->DY < 0) return FALSE;

  if ((BallRight >= mPaddle.X) && (Ball->X <= PaddleRight) &&
      (BallBottom >= mPaddle.Y) && (Ball->Y <= PaddleBottom) &&
      (Ball->Y < mPaddle.Y + 10)) {
    return TRUE;
  }

  return FALSE;
}

VOID DestroyAdjacentBricks (INT32 CenterX, INT32 CenterY)
{
  UINT32 Row, Col;
  INT32 BX, BY;

  for (Row = 0; Row < BRICK_ROWS; Row++) {
    for (Col = 0; Col < BRICK_COLS; Col++) {
      if (!mBricks[Row][Col].Active || mBricks[Row][Col].Type == BrickTypeUnbreakable) continue;

      BX = mBricks[Row][Col].X + mBricks[Row][Col].Width / 2;
      BY = mBricks[Row][Col].Y + mBricks[Row][Col].Height / 2;

      if (ABS(BX - CenterX) < BRICK_WIDTH + BRICK_PADDING &&
          ABS(BY - CenterY) < BRICK_HEIGHT + BRICK_PADDING) {
        mBricks[Row][Col].Active = FALSE;
        mScore += 15;
        SpawnExplosionParticles (BX, BY, mBricks[Row][Col].Color);
      }
    }
  }
}

VOID CheckBrickCollision (BALL *Ball)
{
  UINT32  Row, Col;
  INT32   BallRight, BallBottom;
  INT32   BrickRight, BrickBottom;
  BOOLEAN Hit;
  UINTN   Points;

  BallRight = Ball->X + Ball->Size;
  BallBottom = Ball->Y + Ball->Size;

  for (Row = 0; Row < BRICK_ROWS; Row++) {
    for (Col = 0; Col < BRICK_COLS; Col++) {
      if (!mBricks[Row][Col].Active) continue;

      BrickRight = mBricks[Row][Col].X + mBricks[Row][Col].Width;
      BrickBottom = mBricks[Row][Col].Y + mBricks[Row][Col].Height;

      Hit = FALSE;

      if ((BallRight >= mBricks[Row][Col].X) && (Ball->X <= BrickRight) &&
          (BallBottom >= mBricks[Row][Col].Y) && (Ball->Y <= BrickBottom)) {
        Hit = TRUE;
      }

      if (Hit) {
        if (mBricks[Row][Col].Type == BrickTypeUnbreakable) {
          if (!Ball->FireBall) {
            INT32 OverlapLeft = BallRight - mBricks[Row][Col].X;
            INT32 OverlapRight = BrickRight - Ball->X;
            INT32 OverlapTop = BallBottom - mBricks[Row][Col].Y;
            INT32 OverlapBottom = BrickBottom - Ball->Y;

            INT32 MinOverlapX = (OverlapLeft < OverlapRight) ? OverlapLeft : OverlapRight;
            INT32 MinOverlapY = (OverlapTop < OverlapBottom) ? OverlapTop : OverlapBottom;

            if (MinOverlapX < MinOverlapY) Ball->DX = -Ball->DX;
            else Ball->DY = -Ball->DY;
          }
          return;
        }

        if (!Ball->FireBall) {
          if (mBricks[Row][Col].Type == BrickTypeStrong) {
            mBricks[Row][Col].Hits++;
            if (mBricks[Row][Col].Hits < 1) { Ball->DY = -Ball->DY; return; }
          }

          if (mBricks[Row][Col].Type == BrickTypeVeryStrong) {
            mBricks[Row][Col].Hits++;
            if (mBricks[Row][Col].Hits < 2) { Ball->DY = -Ball->DY; return; }
          }
        }

        Points = 10;
        switch (mBricks[Row][Col].Type) {
          case BrickTypeStrong: Points = 20; break;
          case BrickTypeVeryStrong: Points = 30; break;
          case BrickTypeGold: Points = 100; break;
          case BrickTypeExplosive: Points = 15; break;
          default: break;
        }

        mComboCount++;
        if (mComboCount > 1) Points *= mComboCount;

        mScore += Points;
        if (mScore > mHighScore) mHighScore = mScore;

        SpawnExplosionParticles (
          mBricks[Row][Col].X + mBricks[Row][Col].Width / 2,
          mBricks[Row][Col].Y + mBricks[Row][Col].Height / 2,
          mBricks[Row][Col].Color
          );

        if (mBricks[Row][Col].Type == BrickTypeExplosive) {
          DestroyAdjacentBricks (
            mBricks[Row][Col].X + mBricks[Row][Col].Width / 2,
            mBricks[Row][Col].Y + mBricks[Row][Col].Height / 2
            );
        }

        SpawnPowerUp (
          mBricks[Row][Col].X + mBricks[Row][Col].Width / 2,
          mBricks[Row][Col].Y + mBricks[Row][Col].Height / 2
          );

        ClearBrick (Row, Col);
        mBricks[Row][Col].Active = FALSE;

        if (!Ball->FireBall) {
          INT32 OverlapLeft = BallRight - mBricks[Row][Col].X;
          INT32 OverlapRight = BrickRight - Ball->X;
          INT32 OverlapTop = BallBottom - mBricks[Row][Col].Y;
          INT32 OverlapBottom = BrickBottom - Ball->Y;

          INT32 MinOverlapX = (OverlapLeft < OverlapRight) ? OverlapLeft : OverlapRight;
          INT32 MinOverlapY = (OverlapTop < OverlapBottom) ? OverlapTop : OverlapBottom;

          if (MinOverlapX < MinOverlapY) Ball->DX = -Ball->DX;
          else Ball->DY = -Ball->DY;
        }

        // Check win
        BOOLEAN AllCleared = TRUE;
        for (UINT32 r = 0; r < BRICK_ROWS && AllCleared; r++) {
          for (UINT32 c = 0; c < BRICK_COLS && AllCleared; c++) {
            if (mBricks[r][c].Active && mBricks[r][c].Type != BrickTypeUnbreakable) {
              AllCleared = FALSE;
            }
          }
        }

        if (AllCleared) {
          mLevel++;
          if (mLevel > 5) mGameWon = TRUE;
          else InitializeLevel (mLevel);
        }

        return;
      }
    }
  }
}

EFI_STATUS HandleInput (OUT BOOLEAN *ExitGame)
{
  EFI_KEY_DATA  KeyData;
  EFI_STATUS    Status;

  *ExitGame = FALSE;
  ZeroMem (&KeyData, sizeof (KeyData));

  if (mTextInputEx != NULL) {
    Status = gBS->CheckEvent (mTextInputEx->WaitForKeyEx);

    if (Status == EFI_SUCCESS) {
      Status = mTextInputEx->ReadKeyStrokeEx (mTextInputEx, &KeyData);

      if (!EFI_ERROR (Status)) {
        if ((KeyData.Key.ScanCode == SCAN_RIGHT) ||
            (KeyData.Key.UnicodeChar == 'd') ||
            (KeyData.Key.UnicodeChar == 'D')) {
          mPaddle.OldX = mPaddle.X;
          mPaddle.X += PADDLE_SPEED;
          if (mPaddle.X + mPaddle.Width > (INT32)(mGameAreaX + mGameAreaWidth - 10)) {
            mPaddle.X = mGameAreaX + mGameAreaWidth - mPaddle.Width - 10;
          }
        } else if ((KeyData.Key.ScanCode == SCAN_LEFT) ||
                   (KeyData.Key.UnicodeChar == 'a') ||
                   (KeyData.Key.UnicodeChar == 'A')) {
          mPaddle.OldX = mPaddle.X;
          mPaddle.X -= PADDLE_SPEED;
          if (mPaddle.X < (INT32)(mGameAreaX + 10)) {
            mPaddle.X = mGameAreaX + 10;
          }
        } else if ((KeyData.Key.UnicodeChar == 'q') ||
                   (KeyData.Key.UnicodeChar == 'Q')) {
          *ExitGame = TRUE;
        } else if (KeyData.Key.UnicodeChar == ' ' ||
                   KeyData.Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
          if (!mGameStarted) mGameStarted = TRUE;
          if (mGameOver || mGameWon) {
            InitializeGame ();
          }
        }
      }
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI UefiMain (IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS  Status;
  BOOLEAN     ExitGame;
  UINTN       LastScore;
  BOOLEAN     WasStarted = FALSE;

  Status = InitializeGraphics ();
  if (EFI_ERROR (Status)) {
    Print (L"Failed to initialize graphics.\n");
    return Status;
  }

  InitStars ();
  InitializeInput ();
  InitializeGame ();

  DrawBackground ();
  DrawBricks ();
  DrawInfoPanel ();
  DrawStartScreen ();

  ExitGame = FALSE;
  LastScore = 0;
  WasStarted = FALSE;

  while (!ExitGame) {
    HandleInput (&ExitGame);
    if (ExitGame) break;

    if (!mGameStarted) {
      // Only redraw start screen once, not every frame
      if (!WasStarted) {
        DrawStartScreen ();
        WasStarted = TRUE;
      }
      gBS->Stall (30000);
      continue;
    }

    // Redraw background when game just started (clear welcome screen)
    if (WasStarted) {
      DrawBackground ();
      DrawBricks ();
      DrawInfoPanel ();
      WasStarted = FALSE;
    }

    if (mGameOver) {
      DrawGameOverScreen ();
      DrawInfoPanel ();
      gBS->Stall (30000);
      continue;
    }

    if (mGameWon) {
      DrawWinScreen ();
      DrawInfoPanel ();
      gBS->Stall (30000);
      continue;
    }

    if (mWidePaddleActive) {
      mWidePaddleTimer--;
      if (mWidePaddleTimer == 0) {
        mWidePaddleActive = FALSE;
        ClearPaddle ();
        mPaddle.Width = PADDLE_WIDTH_DEFAULT;
        if (mPaddle.X + mPaddle.Width > (INT32)(mGameAreaX + mGameAreaWidth - 10)) {
          mPaddle.X = mGameAreaX + mGameAreaWidth - mPaddle.Width - 10;
        }
      }
    }

    if (mSlowBallActive) {
      mSlowBallTimer--;
      if (mSlowBallTimer == 0) mSlowBallActive = FALSE;
    }

    ClearBalls ();
    ClearPaddle ();
    ClearPowerUps ();
    ClearParticles ();

    UpdateBalls ();
    UpdatePowerUps ();
    UpdateParticles ();

    DrawBricks ();
    DrawPowerUps ();
    DrawParticles ();
    DrawPaddle ();
    DrawBalls ();

    if (mScore != LastScore) {
      DrawInfoPanel ();
      LastScore = mScore;
    }

    gBS->Stall (22000);
  }

  DrawRect (0, 0, mScreenWidth, mScreenHeight, COLOR_BLACK);

  return EFI_SUCCESS;
}