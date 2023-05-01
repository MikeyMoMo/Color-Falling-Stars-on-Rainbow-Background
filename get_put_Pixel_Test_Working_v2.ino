#include <SPI.h>
#include <TFT_eSPI.h>
// Setup file for ESP32 and TTGO TM ST7789 SPI bus TFT
// Define TFT_eSPI object with the size of the screen:
//  240 pixels width and 400 pixels height.
//  We will rotate it later to portrait to get 400 width and 240 high.
//Use this: #include <User_Setups/Setup22_TTGO_T4_v1.3.h>
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spriteHW = TFT_eSprite(&tft);

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
#define fallIterations 1000
#define numFlakes 30
int32_t icons[numFlakes][4];  // 0:XPOS, 1:YPOS, 2:Movement speed, 3:Star color

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

  int *a;  // Sprite61 (the small one)
  a = (int*)spriteHW.createSprite(TFT_WIDTH, TFT_HEIGHT);
  if (a == 0) {
    Serial.println("spriteHW creation failed.  Cannot continue.");
    while (1);
  }
  Serial.printf("createSprite61 dispWidth x dispHeight returned: %p\r\n", a);

  Serial.print("FreeHeapSize after allocating: ");
  Serial.println(xPortGetFreeHeapSize());
  Serial.printf("Array located at %p.\n", a);

  Serial.println("Creating Rainbow via call to rainbow_fill_array.");
  rainbow_fill_array();
  Serial.println("Background colors created.");
  //  spritetest();  // Just to see how it is going so far.
  //  delay(10000);  // Wait for it!
}
/***************************************************************************/
void loop()
/***************************************************************************/
{
  for (currRow = 0; currRow < TFT_HEIGHT; currRow++)  // Put the candle back!
    spriteHW.drawFastHLine(0, currRow, TFT_WIDTH, bgSave[currRow]);

  // With 25 flakes, getting 24 screen updates per second.
  // With 50 flakes, getting 22 screen updates per second.
  // With 200 flakes, down to 14 screen updates per second.

  Serial.printf("Calling demoFallingStars with %i flakes & %i iterations.\r\n",
                numFlakes, fallIterations);
  demoFallingStars(StarBitmap, fallIterations, numFlakes, StarBitmapW, StarBitmapH);
  Serial.println("All done.");

  delay(5000);
}
/***************************************************************************/
void demoFallingStars(const uint8_t *bitmap, int myIterations,
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

  int flake, currRow;
  //    int spriteReps = 0;  // Put this back in if you are tracking screen refresh rate.

  // Initialize all stars, assigning colors to each and draw all of them.
  for (flake = 0; flake < myFlakes; flake++) {
    icons[flake][XPOS]   = random(TFT_WIDTH);        // Random landscape positioning (X value)
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
      spriteHW.drawBitmap(icons[flake][XPOS], icons[flake][YPOS], StarBitmap, StarBitmapW, StarBitmapH,
                          icons[flake][SCOLOR]);
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
        icons[flake][XPOS]   = random(tft.width());
        icons[flake][YPOS]   = -StarBitmapH;
        icons[flake][DELTAY] = random(5) + 1;
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
  R = random(105) + 100;  // 2nd num is minimum brightness.  First is additive to that.
  G = random(105) + 100;
  B = random(105) + 100;
  return (RGB888(R, G, B));
}
// #########################################################################
// Fill screen with a rainbow pattern and save each line color in an array.
// #########################################################################
/***************************************************************************/
void rainbow_fill_array()
/***************************************************************************/
{

  // This routine came out of the tft_eSPI library from one of the sprite examples.

  // The colours and state are not reinitialised so the start colour
  //  changes each time the funtion is called.
  for (int row = 0; row < TFT_HEIGHT; row++) {
    for (int col = 0; col < TFT_WIDTH; col++) {
      if (!col % 2) {
        colorbuild = colour;
        colorbuild = colorbuild << 16;
      } else {
        colorbuild = colorbuild | colour;
      }
    }
    // Draw a horizontal line 1 pixel high in the selected colour
    // Now propogate the color across the screen line with drawFastHLine.
    //    tft.drawFastHLine(0, row, tft.width(), colour);
    // I am not doing this because it is no longer necessary.  It is being handled in loop().

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
    bgSave[row] = colour;  // This is where the background fixup color comes from.
  }
}
/***************************************************************************/
void spritetest()  // Testing only.
//                    Just used to be sure the sprite code was working.
/***************************************************************************/
{
  spriteHW.fillSprite(TFT_YELLOW);
  spriteHW.setCursor (12, 5);
  spriteHW.print("Original ADAfruit font!");

  spriteHW.setTextColor(TFT_RED, TFT_BLACK); // Do not plot the background colour

  // Overlay the black text on top of the rainbow plot
  //  (the advantage of not drawing the background colour!)
  // Draw text centre at position 80, 12 using font 2
  spriteHW.drawCentreString("Font size 2", 80, 14, 2);
  // Draw text centre at position 80, 24 using font 4
  spriteHW.drawCentreString("Font size 4", 80, 30, 4);
  // Draw text centre at position 80, 24 using font 6
  spriteHW.drawCentreString("12.34", 80, 54, 6);
  // Draw text centre at position 80, 90 using font 2
  spriteHW.drawCentreString("12.34 is in font size 6", 80, 92, 2);

  // Note the x position is the top left of the font!

  // draw a floating point number
  float pi = 3.14159; // Value to print
  int precision = 3;  // Number of digits after decimal point
  int xpos = 50;      // x position
  int ypos = 110;     // y position
  // Font 6 only contains characters [space] 0 1 2 3 4 5 6 7 8 9 0 : a p m
  int font = 2;       // font number only 2,4,6,7 valid.
  // Draw rounded number and return new xpos delta for next print position
  xpos += spriteHW.drawFloat(pi, precision, xpos, ypos, font);
  spriteHW.drawString(" is pi", xpos, ypos, font); // Continue printing from new x position

  spriteHW.pushSprite(0, 0);
}
