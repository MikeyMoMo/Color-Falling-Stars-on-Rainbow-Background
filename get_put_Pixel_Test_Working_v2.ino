#include <SPI.h>
#include <TFT_eSPI.h>
// Setup file for ESP32 and TTGO TM ST7789 SPI bus TFT
// Define TFT_eSPI object with the size of the screen:
//  240 pixels width and 400 pixels height.
//  We will rotate it later to portrait to get 400 width and 240 high.
//#include "Setup23_TTGO_TM.h"  // Doesn't work when included heret
TFT_eSPI tft = TFT_eSPI();
// Setting PWM properties, do not change this!
const int pwmFreq = 5000;
const int pwmResolution = 8;
const int pwmLedChannelTFT = 0;
// Startup TFT backlight intensity on a scale of 0 to 255.
const int ledBacklightFull = 250;
#include "Adafruit_GFX.h"
//For orientations 1 & 3, we have to swap the width and height
#undef TFT_WIDTH
#define TFT_WIDTH 320
#undef TFT_HEIGHT
#define TFT_HEIGHT 240
#define RGB565(r,g,b) ((((r>>3)<<11) | ((g>>2)<<5) | (b>>3)))
#define RGB888(r,g,b) ((r << 16) | (g << 8) | b)
const int xCenter = TFT_WIDTH  / 2;
const int yCenter = TFT_HEIGHT / 2;

//For the falling snowflakes
#define XPOS 0
#define YPOS 1
#define DELTAY 2
#define SCOLOR 3
#define fallIterations 100
#define numFlakes 25
uint32_t icons[numFlakes][4];  // 0:XPOS, 1:YPOS, 2:Movement speed, 3:Star color

byte red = 31;
byte green = 0;
byte blue = 0;
byte state = 0;
unsigned int colour = red << 11; // Colour order is RGB 5+6+5 bits each
uint32_t colorbuild;
int whichHole, currRow;
uint16_t bgSave[TFT_HEIGHT];
int32_t row, column;
uint8_t  r, g, b;
byte R, G, B;
bool oldSwapBytes;

long int ap;
unsigned long startMillis;

int buffSize = (TFT_HEIGHT * TFT_WIDTH) / 2;
uint16_t *workBuff;

#define StarBitmapH 16
#define StarBitmapW 16
static const unsigned char PROGMEM StarBitmap[] =
{ B00000000, B11000000,
  B00000001, B11000000,
  B00000001, B11000000,
  B00000011, B11100000,
  B11110011, B11100000,
  B11111110, B11111000,
  B01111110, B11111111,
  B00110011, B10011111,
  B00011111, B11111100,
  B00001101, B01110000,
  B00011011, B10100000,
  B00111111, B11100000,
  B00111111, B11110000,
  B01111100, B11110000,
  B01110000, B01110000,
  B00000000, B00110000
};

/***************************************************************************/
void setup()
/***************************************************************************/
{
  Serial.begin(115200); delay(2000);

  Serial.println(F("\n\nThis is the get/put Pixel test w/Falling Stars."));
  String asdf = String(__FILE__);
  asdf = asdf.substring(asdf.lastIndexOf("\\") + 1, asdf.length());
  Serial.print("Running from: "); Serial.println(asdf);

  tft.init(); // Initialize the screen.
  ledcSetup(pwmLedChannelTFT, pwmFreq, pwmResolution);
  ledcWrite(pwmLedChannelTFT, ledBacklightFull);
  tft.invertDisplay(false); // Where it is true or false
  tft.setRotation(1);

  workBuff = (uint16_t *)malloc(buffSize);
  if (workBuff)
    memset(workBuff, 0, buffSize);
  else {
    Serial.println("Buffer was not allocated.  Cannot continue."); while (1);
  }

  Serial.print("FreeHeapSize after allocating: ");
  Serial.println(xPortGetFreeHeapSize());
  Serial.printf("Array located at %p.\n", workBuff);

  Serial.println("Creating Rainbow via call to rainbow_fill_array.");
  rainbow_fill_array(workBuff);
  Serial.println("Screen filled.");

  Serial.println("Saving single color byte per row.");
  for (currRow = 0; currRow < TFT_HEIGHT; currRow++)
    bgSave[currRow] = tft.readPixel(0, currRow);
  Serial.printf("Row colors saved, now rebuilding the Rainbow with drawFastHLine.\n");
}
/***************************************************************************/
void loop()
/***************************************************************************/
{
  Serial.println("Clear the screen for fast draw with pushImage.");
//  tft.fillScreen(0);
  Serial.printf("The array starts at %p\n", workBuff);
  Serial.println("Now, redraw the screen from the buffer with pushImage.");
  oldSwapBytes = tft.getSwapBytes();
  tft.setSwapBytes(!oldSwapBytes);
  startMillis = millis();
  tft.pushImage(0, 0, TFT_WIDTH, TFT_HEIGHT, workBuff);
  Serial.printf("It took %i ms to draw rainbow with pushImage.\n", millis() - startMillis);
  tft.setSwapBytes(oldSwapBytes);

  Serial.println("Calling demoFallingStars routine.");
  demoFallingStars(StarBitmap,  workBuff, fallIterations, numFlakes,
                   StarBitmapW, StarBitmapH);
  Serial.println("All done.");
  delay(5000);
}
/***************************************************************************/
void demoFallingStars(const uint8_t *bitmap, uint16_t *ptr, int myIterations,
                      int myFlakes, uint8_t w, uint8_t h)
/***************************************************************************/
{
  // This was taken from an Adafruit demo program.  All praise to them.
  //  I added color to the stars and a background that the stars float over
  //  without destroying the background.
  // To implement rainbow_fill, I have to fetch the color of each line of the background
  //  before writing the stars, then put it back at the right place to erase the star.
  //  Keep the background line color in a TFT_HEIGHT size byte array since
  //  the entire line is the same color.  Just one byte needed per line for its color.
  //  I only fill in the area occupied by the flake.  It is very fast!
  //  The improvement to this is to use a canvas to further reduce the flickering.

  // red, orange, yellow, green, blue, indigo (violet)
  // Take these 6 colors, 40 lines each and disolve one into the next, continuously down the screen.
  // Red    255,   0,   0
  // Orange 255, 180,   0
  // Yellow 255, 255,   0
  // Green  0,   255,   0
  // Blue   0,     0, 255
  // Indigo 75,    0, 130

  int currRow, b;
  byte myByte;
  int flake, myH, myW;
  int byteWidth = (w + 7) / 8;

  // Initialize all stars, assigning colors to each and draw all of them.
  for (flake = 0; flake < myFlakes; flake++) {
    icons[flake][XPOS] = random(TFT_WIDTH);
    icons[flake][YPOS] = random(TFT_HEIGHT * .8);;
    icons[flake][DELTAY] = random(5) + 1;
    icons[flake][SCOLOR] = GetStarColor();
  }
  while (myIterations-- > 0) {
    for (flake = 0; flake < myFlakes; flake++) {
      tft.drawBitmap(icons[flake][XPOS], icons[flake][YPOS], StarBitmap, StarBitmapW, StarBitmapH,
                     icons[flake][SCOLOR]);
    }
    delay(100);
    if (myIterations == 1) return;  // Leave the last update showing.
    // Now erase the flakes by rewriting the background colors, proper rows,
    for (flake = 0; flake < myFlakes; flake++) {
      for (currRow = 0; currRow < 16; currRow++)
        tft.drawFastHLine(icons[flake][XPOS], icons[flake][YPOS] + currRow, 16,
                          bgSave[icons[flake][YPOS] + currRow]);
      // Now adjust the position of each flake down the screen and re-init if off screen.
      if (icons[flake][YPOS] > tft.height()) {
        icons[flake][XPOS]   = random(tft.width());
        icons[flake][YPOS]   = 0;
        icons[flake][DELTAY] = random(5) + 1;
        icons[flake][SCOLOR] = GetStarColor();
      } else {  // Else, move it down the screen a bit.
        icons[flake][YPOS]  += icons[flake][DELTAY];
      }
    }
  }
}
/***************************************************************************/
uint32_t GetStarColor()
/***************************************************************************/
{
  R = random(105) + 150;  // 2nd num is minimum brightness.  First is additive to that.
  G = random(105) + 150;
  B = random(105) + 150;
  //  Serial.printf("R %i, G %i, B %i  or  %X\n", R, G, B, RGB888(R, G, B));
  return (RGB888(R, G, B));
}
// #########################################################################
// Fill screen with a rainbow pattern and save each line color in an array.
// #########################################################################
/***************************************************************************/
void rainbow_fill_array(uint16_t *ptr)
/***************************************************************************/
{
  // The colours and state are not reinitialised so the start colour
  //  changes each time the funtion is called.
  uint16_t *my_ptr, debugCt;
  my_ptr = workBuff;
  for (int row = 0; row < TFT_HEIGHT; row++) {
    for (int col = 0; col < TFT_WIDTH; col++) {
      if (!my_ptr % 2) {
        colorbuild = colour;
        colorbuild = colorbuild << 16;
        //        if (debugCt++ < 20) Serial.printf("Even. Colour = %x, colorbuild now: %X\n",
        //                                            colour, colorbuild);
      } else {
        colorbuild = colorbuild | colour;
        //        if (debugCt++ < 20) Serial.printf("Odd.  Colour = %x, colorbuild now: %X\n",
        //                                            colour, colorbuild);
        *my_ptr = colour;
      }
      my_ptr++;
      //      Serial.printf("%p\n", my_ptr);
    }
    // Draw a horizontal line 1 pixel high in the selected colour
    // Now propogate the color across the screen line with drawFastHLine.
    tft.drawFastHLine(0, row, tft.width(), colour);
    // This is a "state machine" that ramps up/down the colour brightnesses in sequence
    switch (state) {
      case 0:
        green ++;
        if (green == 64) {
          green = 63;
          state = 1;
        }
        break;
      case 1:
        red--;
        if (red == 255) {
          red = 0;
          state = 2;
        }
        break;
      case 2:
        blue ++;
        if (blue == 32) {
          blue = 31;
          state = 3;
        }
        break;
      case 3:
        green --;
        if (green == 255) {
          green = 0;
          state = 4;
        }
        break;
      case 4:
        red ++;
        if (red == 32) {
          red = 31;
          state = 5;
        }
        break;
      case 5:
        blue --;
        if (blue == 255) {
          blue = 0;
          state = 0;
        }
        break;
    }
    colour = red << 11 | green << 5 | blue;
  }
}
