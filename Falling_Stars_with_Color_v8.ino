#include <SPI.h>
#include <TFT_eSPI.h>
#include "Adafruit_GFX.h"
#define version "8.00"

// Setup file for ESP32 and TTGO T4 v1.3 SPI bus TFT
// Define TFT_eSPI object with the size of the screen:
//  240 pixels width and 320 pixels height.

//Use this: #include <User_Setups/Setup22_TTGO_T4_v1.3.h>

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spriteHW = TFT_eSprite(&tft);

// Setting PWM properties, do not change this!
const int pwmFreq = 5000;
const int pwmResolution = 8;
const int pwmLedChannelTFT = 0;
// Startup TFT backlight intensity on a scale of 0 to 255.
const int ledBacklightFull = 255;
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
#define fallIterations 1000
#define numFlakes 30
int32_t icons[numFlakes][4];  // 0:XPOS, 1:YPOS, 2:Movement speed, 3:Star color

//byte red = 31;
//byte green = 0;
//byte blue = 0;
//byte state = 0;
//unsigned int colour = red << 11;

uint32_t colorbuild;

int currRow;
uint16_t bgSave[TFT_HEIGHT];
int32_t row, column;
bool oldSwapBytes;

long int ap;
unsigned long startMillis;

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
  tft.setRotation(3);

  // This must be allocated in PSRAM since ESP folks screwed up allocation.  There is a 32 bit
  //  and an 8 bit allocation but no 16 bit so you get double what you need and it blows the stack!
  int *a;  // spriteHW (full height and width of the screen)
  a = (int*)spriteHW.createSprite(TFT_WIDTH, TFT_HEIGHT);
  if (a == 0) {
    Serial.println("spriteHW creation failed.  Cannot continue.");
    while (1);
  }
  Serial.printf("createSpriteHW dispWidth x dispHeight returned: %p\r\n", a);

  // No longer coming from heap.  Too large!
  //  Serial.print("FreeHeapSize after allocating: ");
  //  Serial.println(xPortGetFreeHeapSize());
  //  Serial.printf("Array located at %p.\n", a);

  Hello(); delay(5000);  // Show sign on message with version number.
}
/***************************************************************************/
void loop()
/***************************************************************************/
{
  selectRainbowFill();
  for (currRow = 0; currRow < TFT_HEIGHT; currRow++)  // Put the candle back!
    spriteHW.drawFastHLine(0, currRow, TFT_WIDTH, bgSave[currRow]);

  // With 25 flakes, getting 24 screen updates per second.
  // With 50 flakes, getting 22 screen updates per second.
  // With 200 flakes, down to 14 screen updates per second.

  Serial.printf("Calling demoFallingStars with %i flakes & %i iterations.\r\n",
                numFlakes, fallIterations);
  demoFallingStars(StarBitmap, fallIterations, numFlakes, StarBitmapW, StarBitmapH);
  Serial.println("All done.");
  //  delay(10000);
}
/***************************************************************************/
void demoFallingStars(const uint8_t *bitmap, int myIterations,
                      int myFlakes, uint8_t w, uint8_t h)
/***************************************************************************/
{
  // This was taken from an Adafruit demo program.  All praise to them.
  //  I added color to the stars and a background that the stars float over
  //  without destroying the background. Actually, they do destroy it, but I fix it quickly!
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

  int flake, currRow;
  //    int spriteReps = 0;  // Put this back in if you are tracking screen refresh rate.

  // Initialize all stars, assigning colors to each and draw all of them.
  for (flake = 0; flake < myFlakes; flake++) {
    icons[flake][XPOS]   = random(TFT_WIDTH - w); // Random landscape positioning (X value)
    icons[flake][YPOS]   = random(TFT_HEIGHT * .8);  // Start them all on-screen. (Y value)
    icons[flake][DELTAY] = random(6) + 1;            // This is the fall speed. (Ensure != 0)
    icons[flake][SCOLOR] = GetStarColor();
  }
  //  startMillis = millis();  // Put this back in if you are tracking screen refresh rate.
  while (myIterations-- > 0) {

    // Include this routine to see your frame refresh rate.
    //        if ((millis() - startMillis) > 1000) {
    //          Serial.printf("%i screen updates per second.\r\n", spriteReps);
    //          startMillis = millis(); spriteReps = 0;
    //        }

    // First, put the requested number of flakes on the screen.
    for (flake = 0; flake < myFlakes; flake++)
      spriteHW.drawBitmap(icons[flake][XPOS], icons[flake][YPOS], bitmap, w, h, icons[flake][SCOLOR]);
    // Then, update the screen.  The time to do this is just under 42 ms on my device.
    //  No delay() needed.
    spriteHW.pushSprite(0, 0);
    //    delay(100);  // The pushSprite delay is sufficient.

    // Get out on the last rep to leave the displayed stars still showing in their last positions.
    if (myIterations == 1) return;  // Leave the last update showing.

    // Now erase the flakes by rewriting the background colors, in their proper rows.
    //  I only rewrite the back square that was "destroyed" by the star being written,
    //  not the whole line.  That saves time and the full line write is not required.
    //  It takes less than a ms to repair the "damage" but takes 34 ms to rewrite the entire
    //  background with pushImage.  Just a few ms longer for a bunch of fill width drawFastHLine's.

    for (flake = 0; flake < myFlakes; flake++) {
      for (currRow = 0; currRow < 16; currRow++)
        spriteHW.drawFastHLine(icons[flake][XPOS], icons[flake][YPOS] + currRow, 16,
                               bgSave[icons[flake][YPOS] + currRow]);
      // Now adjust the position of each flake down the screen and re-init if off screen.
      if (icons[flake][YPOS] > tft.height()) {
        icons[flake][XPOS]   = random(tft.width() - w);
        icons[flake][YPOS]   = -StarBitmapH;
        icons[flake][DELTAY] = random(6) + 1;
        icons[flake][SCOLOR] = GetStarColor();
      } else {  // Else, move it down the screen a bit.
        icons[flake][YPOS]  += icons[flake][DELTAY];
      }
    }
    //        spriteReps++;  // Put this back in if you are tracking screen refresh rate.
  }
}
/***************************************************************************/
uint32_t GetStarColor()
/***************************************************************************/
{
  static byte R, G, B;
  R = random(200) + 50;
  G = random(200) + 50;
  B = random(200) + 50;
  return (RGB565(R, G, B));
}
// #########################################################################
// Fill screen with a rainbow pattern and save each line color in an array.
// #########################################################################
/***************************************************************************/
void rainbow_fill_4()
/***************************************************************************/
{
  //  static int rVal    = 255, gVal    = 32, bVal    = 32;
  //  static int RedDir =   1, GreenDir =  1, BlueDir =  1;
  //  static int NextState = 1;  // Color inits for rainbow4

  Serial.println("Using background routine 4 (mine)");

  // For this one, they will all continuously cycle either up or down all at
  // once.  Will see if this is better...  Hope so!

  int colorStep  =   4;
  int maxBright  = 255;
  int minBright  =  70;
  const int maxScreenBright = 650;

  static int rVal = 200, gVal = 70, bVal = 70;  // Brightness
  static int rDir =  -1, gDir =  1, bDir =  1;  // Either up or down on brightness
  static int rStep =  4, gStep = 4, bStep = 4;  // How fast to step the brightness (4 to 12)

  //  Serial.printf("0-R %3i %3i %3i, G %3i %3i %3i, B %3i %3i %3i\r\n",
  //                rVal, rDir, rStep, gVal, gDir, gStep, bVal, bDir, bStep);

  for (int row = 0; row < tft.height(); row++) {
    // Adjust Red
    rVal += rDir * rStep;
    if (rVal > maxBright) {
      rVal = maxBright; rDir = -1; rStep = random(4) + 1;
    }
    if (rVal < minBright) {
      rVal = minBright; rDir = 1; rStep = random(4) + 1;
    }
    //    Serial.printf("r-R %3i %3i %3i, G %3i %3i %3i, B %3i %3i %3i\r\n",
    //                  rVal, rDir, rStep, gVal, gDir, gStep, bVal, bDir, bStep);
    if (rVal + gVal + bVal > maxScreenBright) rDir = -1;

    // Adjust Green
    gVal += gDir * gStep;
    if (gVal > maxBright) {
      gVal = maxBright; gDir = -1; gStep = random(4) + 1;
    }
    if (gVal < minBright) {
      gVal = minBright; gDir = 1; gStep = random(4) + 1;
    }
    //    Serial.printf("g-R %3i %3i %3i, G %3i %3i %3i, B %3i %3i %3i\r\n",
    //                  rVal, rDir, rStep, gVal, gDir, gStep, bVal, bDir, bStep);
    if (rVal + gVal + bVal > maxScreenBright) gDir = -1;

    // Adjust Blue
    bVal += bDir * bStep;
    if (bVal > maxBright) {
      bVal = maxBright; bDir = -1; bStep = random(4) + 1;
    }
    if (bVal < minBright) {
      bVal = minBright; bDir = 1; bStep = random(4) + 1;
    }
    if (rVal + gVal + bVal > maxScreenBright) bDir = -1;

    //    Serial.printf("b-R %3i %3i %3i, G %3i %3i %3i, B %3i %3i %3i to %3i\r\n",
    //                  rVal, rDir, rStep, gVal, gDir, gStep, bVal, bDir, bStep, row);
    bgSave[row] = RGB565(rVal, gVal, bVal);
  }
}
/***************************************************************************/
void rainbow_fill_3()
/***************************************************************************/
{
  int colorStep =   4;
  int maxColor  = 250;
  int minColor  =  70;
  static int NextState = 1;  // Color inits for rainbow4
  static int rVal = 255, gVal = 32, bVal = 32;
  static int RedDir = 1, GreenDir = 1, BlueDir = 1;

  Serial.println("Using background routine 3 (my state machine)");
  Serial.printf("Entering rainbow3 with state %i.\r\n", NextState);
  // Start with red = 255, the others are 0.
  // First, cycle green up to 255 and back down by 4 then down to 32
  // Then, cycle blue up to 255 by 4, then back down to 32
  // Then, lower red by 4 and do it again.  This may be too slow.
  //case  1:  // Red is stable, green going up
  //case  2:  // Red is stable, green coming down
  //case  3:  // Red is stable, blue going up
  //case  4:  // Red is stable, blue coming down
  //case  5:  // Green is stable, Red coming up
  //case  6:  // Green is stable, Red coming down
  //case  7:  // Green is stable, Blue coming up
  //case  8:  // Green is stable, Blue coming down
  //case  9:  // Blue is stable, Red coming up
  //case 10:  // Blue is stable, Red coming down
  //case 11:  // Blue is stable, Green coming up
  //case 12:  // Blue is stable, Green coming down

  //r, r+g, r+b, g, g+b, b

  //1 red to max from 70                   red only
  //2 green to max from 70                 yellow (red+green)
  //3 green down to 70 + blue up from 70   purple (red+blue)
  //4 blue down to 70                      green only
  //5 blue up from 70                      cyan (green+blue)
  //6 green down to 70                     blue only

  for (int row = 0; row < tft.height(); row++) {
    switch (NextState) {
      case 1:  // Red is stable, green going up
        //        maxColor = random(250);
        gVal += colorStep;
        if (gVal >= maxColor) {
          gVal = maxColor;
          NextState = 2;
        }
        //        Serial.printf("End of state  1 R %3i G %3i, B %3i, Next State %i\r\n",
        //                      rVal, gVal, bVal, NextState);
        break;
      case 2:  // Red is stable, green coming down
        //        minColor = random(50) + 50;
        gVal -= colorStep;
        if (gVal <= minColor) {
          gVal = minColor;
          NextState = 3;
        }
        //        Serial.printf("End of state  2 R %3i G %3i, B %3i, Next State %i\r\n",
        //                      rVal, gVal, bVal, NextState);
        break;
      case 3:  // Red is stable, blue going up
        //        maxColor = random(250);
        bVal += colorStep;
        if (bVal >= maxColor) {
          bVal = maxColor;
          NextState = 4;
        }
        //        Serial.printf("End of state  3 R %3i G %3i, B %3i, Next State %i\r\n",
        //                      rVal, gVal, bVal, NextState);
        break;
      case 4:  // Red is stable, blue coming down
        //        minColor = random(50) + 50;
        bVal -= colorStep;
        //        Serial.printf("End of state 4 bVal %3i, minColor %i\r\n", bVal, minColor);
        if (bVal <= minColor) {
          bVal = minColor;
          NextState = 5;
        }
        //        Serial.printf("End of state  4 R %3i G %3i, B %3i, Next State %i\r\n",
        //                      rVal, gVal, bVal, NextState);
        break;
      case 5:  // Green is stable, Red coming up
        //        maxColor = random(250);
        rVal += colorStep;
        if (rVal >= maxColor) {
          rVal = maxColor;
          NextState = 6;
        }
        //        Serial.printf("End of state  5 R %3i G %3i, B %3i, Next State %i\r\n",
        //                      rVal, gVal, bVal, NextState);
        break;
      case 6:  // Green is stable, Red coming down
        //        minColor = random(50) + 50;
        rVal -= colorStep;
        if (rVal <= minColor) {
          rVal = minColor;
          NextState = 7;
        }
        //        Serial.printf("End of state  6 R %3i G %3i, B %3i, Next State %i\r\n",
        //                      rVal, gVal, bVal, NextState);
        break;
      case 7:  // Green is stable, Blue coming up
        //        maxColor = random(250);
        gVal += colorStep;
        if (gVal >= maxColor) {
          gVal = maxColor;
          NextState = 8;
        }
        //        Serial.printf("End of state  7 R %3i G %3i, B %3i, Next State %i\r\n",
        //                      rVal, gVal, bVal, NextState);
        break;
      case 8:  // Green is stable, Blue coming down
        //        minColor = random(50) + 50;
        gVal -= colorStep;
        if (gVal <= minColor) {
          gVal = minColor;
          NextState = 9;
        }
        //        Serial.printf("End of state  8 R %3i G %3i, B %3i, Next State %i\r\n",
        //                      rVal, gVal, bVal, NextState);
        break;

      case  9:  // Blue is stable, Red coming up
        //        maxColor = random(250);
        rVal += colorStep;
        if (rVal >= maxColor) {
          rVal = maxColor;
          NextState = 10;
        }
        //        Serial.printf("End of state  9 R %3i G %3i, B %3i, Next State %i\r\n",
        //                      rVal, gVal, bVal, NextState);
        break;

      case 10:  // Blue is stable, Red coming down
        //        minColor = random(50) + 50;
        rVal -= colorStep;
        if (rVal <= minColor) {
          rVal = minColor;
          NextState = 11;
        }
        //        Serial.printf("End of state 10 R %3i G %3i, B %3i, Next State %i\r\n",
        //                      rVal, gVal, bVal, NextState);
        break;

      case 11:  // Blue is stable, Green coming up
        //        maxColor = random(250);
        bVal += colorStep;
        if (bVal >= maxColor) {
          bVal = maxColor;
          NextState = 12;
        }
        //        Serial.printf("End of state 11 R %3i G %3i, B %3i, Next State %i\r\n",
        //                      rVal, gVal, bVal, NextState);
        break;
      case 12:  // Blue is stable, Green coming down
        //        minColor = random(50) + 50;
        bVal -= colorStep;
        if (bVal <= minColor) {
          bVal = minColor;
          NextState = 1;
        }
        //        Serial.printf("End of state 12 R %3i G %3i, B %3i, Next State %i\r\n",
        //                      rVal, gVal, bVal, NextState);
        break;
    }
    bgSave[row] = RGB565(rVal, gVal, bVal);
  }
}
/***************************************************************************/
void rainbow_fill_2()
/***************************************************************************/
{
  static int redRun =  50, greenRun =  50, blueRun = 50;  // How many to do before init
  static int redDir =  -1, greenDir =  +1, blueDir = +1;  // Up or down (changes at init)
  static int redVal = 200, greenVal = 100, blueVal = 50;  // Color value adjusted per row

  Serial.println("Using background routine 2 (mine - not state machine)");
  // See the globals for starting values.
  //  Up there so it won't repeat endlessly.
  for (row = 0; row < tft.height(); row++) {
    // Save what we have now into the BG draw array.
    // Adjust the color values up or down depending on xDir value.
    redVal += redDir; greenVal += greenDir; blueVal += blueDir;
    // Decrease the effective value. When out of limits, reset.
    redRun -= 1; greenRun -= 1; blueRun -= 1;
    // Now check boundaries and re-init stuff.
    if (redRun < 1) {  // Out of moves.
      redRun = random(tft.height() / 4);
      if (redRun < 20) redRun += 20;
      redDir = random(1000);
      if (redDir < 500)
        redDir = -1;
      else
        redDir = 1;
    }
    if (greenRun < 1) {  // Out of moves.
      greenRun = random(tft.height() / 4);
      if (greenRun < 20) greenRun += 20;
      greenDir = random(1000);
      if (greenDir < 500)
        greenDir = -1;
      else
        greenDir = 1;
    }
    if (blueRun < 1) {  // Out of moves.
      blueRun = random(tft.height() / 4);
      if (blueRun < 20) blueRun += 20;
      blueDir = random(1000);
      if (blueDir < 500)
        blueDir = -1;
      else
        blueDir = 1;
    }
    // Finally, a brute force fixup for direction to say in bounds.
    if (redVal < 0) {
      if (redRun < 20) redRun += 20;
      redDir = 1;
      redVal = 0;
    }
    if (redVal > 250) {
      if (redRun < 20) redRun += 20;
      redDir = -1;
      redVal = 250;
    }
    if (greenVal < 0) {
      if (greenRun < 20) greenRun += 20;
      greenDir = 1;
      greenVal = 0;
    }
    if (greenVal > 250) {
      if (greenRun < 20) greenRun += 20;
      greenDir = -1;
      greenVal = 250;
    }
    if (blueVal < 0) {
      if (blueRun < 20) blueRun += 20;
      blueDir = 1;
      blueVal = 0;
    }
    if (blueVal > 250) {
      if (blueRun < 20) blueRun += 20;
      blueDir = -1;
      blueVal = 250;
    }
    bgSave[row] = RGB565(redVal, greenVal, blueVal);
  }
}
/***************************************************************************/
void rainbow_fill_1()
/***************************************************************************/
{
  static int r = 255, b = 0, g = 0;

  Serial.println("Using background routine 1 (unknown source)");
  for (int row = 0; row < TFT_HEIGHT; row++) {
    if (r > 0 && b == 0) {
      r--; g++; r--; g++;
      if (r < 0)   r = 0;
      if (g > 255) g = 255;
    }
    if (g > 0 && r == 0) {
      g--; b++; g--; b++;
      if (g < 0)   g = 10;
      if (b > 255) b = 255;
    }
    if (b > 0 && g == 0) {
      r++; b--; r++; b--;
      if (b < 0)   b = 10;
      if (r > 255) r = 255;
    }
    // This array will get written to the screen, later.
    bgSave[row] = RGB565(r, g, b);
  }
}
// #########################################################################
// Fill screen with a rainbow pattern and save each line color in an array.
// #########################################################################
/***************************************************************************/
void rainbow_fill_0()
/***************************************************************************/
{
  static byte red   = 31;
  static byte green =  0;
  static byte blue  =  0;
  static byte state =  0;
  static unsigned int colour = red << 11;

  Serial.println("Using background routine 0 (Bodmer)");
  // Colour changing state machine
  for (int row = 0; row < tft.height(); row++) {
    switch (state) {
      case 0:
        green += 2;
        if (green == 64) {
          green = 63;
          state = 1;
        }
        break;
      case 1:
        red--;
        if (red > 254) {
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
        green -= 2;
        if (green > 254) {
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
        if (blue > 254) {
          blue = 0;
          state = 0;
        }
        break;
    }
    colour = red << 11 | green << 5 | blue;
    // This array will get written to the screen, later.
    bgSave[row] = colour;  // This is where the background fixup color comes from.
  }
}
/***************************************************************************/
void Hello()  // Testing only.  Signon message.
//               Just used to be sure the sprite code was working.
/***************************************************************************/
{
  spriteHW.fillSprite(TFT_YELLOW);
  //  spriteHW.setCursor (12, 5);
  //  spriteHW.print("Original ADAfruit font!");

  spriteHW.setTextColor(TFT_BLACK, TFT_YELLOW); // Do not plot the background colour

  // Overlay the black text on top of the rainbow plot
  //  (the advantage of not drawing the background colour!)
  // Draw text centre at position 80, 12 using font 2
  //  spriteHW.drawCentreString("Font size 2", 80, 14, 2);
  //  // Draw text centre at position 80, 24 using font 4
  spriteHW.drawCentreString("Falling Stars v" + String(version), TFT_WIDTH / 2, TFT_HEIGHT / 2, 4);
  Serial.print("Hello from version "); Serial.println(version);
  //  // Draw text centre at position 80, 24 using font 6
  //  spriteHW.drawCentreString("12.34", 80, 54, 6);
  //  // Draw text centre at position 80, 90 using font 2
  //  spriteHW.drawCentreString("12.34 is in font size 6", 80, 92, 2);

  // Note the x position is the top left of the font!

  //  // draw a floating point number
  //  float pi = 3.14159; // Value to print
  //  int precision = 3;  // Number of digits after decimal point
  //  int xpos = 50;      // x position
  //  int ypos = 110;     // y position
  //  // Font 6 only contains characters [space] 0 1 2 3 4 5 6 7 8 9 0 : a p m
  //  int font = 2;       // font number only 2,4,6,7 valid.
  //  // Draw rounded number and return new xpos delta for next print position
  //  xpos += spriteHW.drawFloat(pi, precision, xpos, ypos, font);
  //  spriteHW.drawString(" is pi", xpos, ypos, font); // Continue printing from new x position

  spriteHW.pushSprite(0, 0);
}
/***************************************************************************/
void selectRainbowFill()
/***************************************************************************/
{
  static int lastPick;
  int pick = random(5);
  while (pick == lastPick) pick = random(5);
  lastPick = pick;
  switch (pick) {
    case 0: rainbow_fill_0(); break;
    case 1: rainbow_fill_1(); break;
    case 2: rainbow_fill_2(); break;
    case 3: rainbow_fill_3(); break;
    case 4: rainbow_fill_4(); break;
    default: Serial.println("I am sorry, but this is impossible!"); break;
  }
}
