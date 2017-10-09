
// 32x16 display setup
#include <Adafruit_GFX.h>   // Core graphics library
#include <RGBmatrixPanel.h> // Hardware-specific library

#define CLK 11
#define LAT A3
#define OE  9
#define A   A0
#define B   A1
#define C   A2

#define WIDTH 32
#define HEIGHT 16

#define HUE_ROLL 1536

// Last parameter = 'true' enables double-buffering, for flicker-free,
// buttery smooth animation.  Note that NOTHING WILL SHOW ON THE DISPLAY
// until the first call to swapBuffers().  This is normal.
RGBmatrixPanel matrix(A, B, C, CLK, LAT, OE, true);

//int measurements[] = {63, 95, 127, 159, 191, 223, 255};

// each band gets 4 pixels, except the two which get 6
// this could be calculated with conditionals, but memory is readily available on the mega so
// we unroll for performance
int bandOffsets[] = {0, 6, 10, 14, 18, 22, 26};
int bandWidths[] =  {6, 4,  4,  4,  4,  4,  6};
//int bandOffsets[] = {0, 4,  8, 14, 18, 24, 28};
//int bandWidths[] =  {4, 4,  6,  4,  6,  4,  4};

void setup() {
  matrix.begin();
  matrix.setTextWrap(false); // Allow text to run off right edge
  matrix.setTextSize(1);

  setTrack("Lady Gaga - Poker Face");
}

uint16_t i, hue;
uint16_t color, colorInverse;

String track;
int textX;
int textMin;
boolean trackShown;

void loop() {
  matrix.fillScreen(0);

  color = matrix.ColorHSV(hue, 255, 127, true);
  colorInverse = matrix.ColorHSV((hue + (HUE_ROLL / 2)) % HUE_ROLL, 255, 127, true);

  for(i=0; i<7; i++) {
    // x, y, w, h, color
    uint8_t m = random(0, 256);
    matrix.fillRect(bandOffsets[i], HEIGHT - (m >> 4), bandWidths[i], m >> 4, color);
    
    if (0x0C & m){
      matrix.drawFastHLine(bandOffsets[i], HEIGHT - (m >> 4) - 1, bandWidths[i], color);
    }
      // the overflow can be put in the brightness of the last line
//    matrix.drawFastHLine(bandOffsets[i], HEIGHT - (m >> 4) - 1, bandWidths[i], matrix.ColorHSV(hue, 255, (0x0F & m) << 3, true));
  }

  showTrack();
  
  matrix.swapBuffers(false);
  
  delay(10);
  hue = (hue + 1) % HUE_ROLL;
}

void setTrack(String trackName) {
  track = trackName;
  textX = WIDTH;
  textMin = track.length() * -12;
  trackShown = false;
}

void showTrack() {
  if (!trackShown) {
    matrix.setTextColor(colorInverse);
    matrix.setCursor(textX, 8);
    matrix.print(track);
    if((--textX) < textMin) trackShown = true;
  }
}

