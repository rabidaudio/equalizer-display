#define RESET 11
#define STROBE 12

#define BANDS 7
#define LP_STEPS 16

#define DEBUG 0

// 32x16 display setup
#include <Adafruit_GFX.h>   // Core graphics library
#include <RGBmatrixPanel.h> // Hardware-specific library

// docs put this on 11 but already using 11 for reset.
// just needs to be on PORTB
#define CLK 10
#define LAT A3
#define OE  9
#define A   A0
#define B   A1
#define C   A2

// display size
#define WIDTH 32
#define HEIGHT 16

// when to start over hue back at 0
#define HUE_ROLL 1536

#define TITLE_SHOW_PERIOD 30 * 1000

// Last parameter = 'true' enables double-buffering, for flicker-free,
// buttery smooth animation.  Note that NOTHING WILL SHOW ON THE DISPLAY
// until the first call to swapBuffers().  This is normal.
RGBmatrixPanel matrix(A, B, C, CLK, LAT, OE, true);

// each band gets 4 pixels, except the two which get 6
// this could be calculated with conditionals, but memory is
// readily available on the mega so we unroll for performance
const int bandOffsets[] = {0, 6, 10, 14, 18, 22, 26};
const int bandWidths[] =  {6, 4,  4,  4,  4,  4,  6};

uint16_t hue = 0;
uint16_t color, colorInverse;

// buffer for EQ values
uint8_t measureIndex = 0;
uint8_t passIndex = 0;
uint16_t measurements[BANDS * LP_STEPS];

// not overly readable code, but shooting for efficency.
// resetState counts down from 3 at each strobe until 0.
// on 3 and 1 we can toggle reset, and when it hits 0
// we can start taking measurements.
uint8_t resetState = 3;

// state
boolean playing = false;
boolean connected = false;

// text state
String title, artist, track;
int textX, textMin;
boolean titleShowing;
unsigned long nextTitleShow;

void setup() {
  pinMode(STROBE, OUTPUT);
  pinMode(RESET, OUTPUT);

  if(DEBUG) Serial.begin(9600);

  setupBluetooth();

  setupTimer();

  matrix.begin();
  // Allow text to run off right edge
  matrix.setTextWrap(false);
  matrix.setTextSize(1);

  // clear screen, just in case
  matrix.fillScreen(0);
  matrix.swapBuffers(false);
}

// This timer controlls the strobe and reset pins
ISR(TIMER5_COMPA_vect){
  if (resetState == 3) {
    PORTB |= (1 << 5); // digitalWrite(RESET, HIGH);
  } else if (resetState == 1) {
    PORTB &= ~(1 << 5); // digitalWrite(RESET, LOW);
  }
  // toggle strobe
  PORTB ^= (1 << 6);

  if (resetState) {
    resetState--;
  }
}

// this timer takes the measurement
ISR(TIMER5_COMPB_vect) {
  // if resetState is 0 and strobe is low
  if (!resetState && !(PORTB & (1 << 6))) {
    // take measurement
    measurements[(passIndex * BANDS) + measureIndex] = analogRead(A7);
    // increment measureIndex
    measureIndex = (measureIndex + 1) % BANDS;
    if (!measureIndex) {
      passIndex = (passIndex + 1) % LP_STEPS;
    }
  }
}

void loop() {

  if (playing) {
    matrix.fillScreen(0); //clear screen

    // calculate new colors
    color = matrix.ColorHSV(hue, 255, 127, true);
    colorInverse = matrix.ColorHSV((hue + (HUE_ROLL / 2)) % HUE_ROLL, 255, 127, true);

    updateEqualizer();
    showTrack();

    matrix.swapBuffers(false); // frame update
    hue = (hue + 1) % HUE_ROLL; // increment hue
  }
  
  // check for state changes
  boolean stoppedPlaying = false;
  boolean disconnected = false;
  if (Serial1.available()) {
    while (Serial1.available()) {
      String line = Serial1.readStringUntil('\r');
      if (DEBUG) Serial.println(line);
      if (line.startsWith("AVRCP_PLAY") || line.startsWith("A2DP_STREAM_START")) {
        playing = true;
        if(DEBUG) Serial.println("playing");
      } else if (line.startsWith("ROLE_OK")){
        connected = true;
        if(DEBUG) Serial.println("connected");
      } else if (line.startsWith("CLOSE_OK")) {
        if (connected) {
          disconnected = true;
        }
        connected = false;
        if(DEBUG) Serial.println("not connected");
      } else if (line.startsWith("AVRCP_PAUSE") || line.startsWith("A2DP_STREAM_SUSPEND")) {
        if (playing) {
          stoppedPlaying = true;
        }
        playing = false;
        if(DEBUG) Serial.println("stopped");
      } else if (line.startsWith("AVRCP_MEDIA TITLE")) {
        title = line.substring(19);
        title.trim();
      } else if (line.startsWith("AVRCP_MEDIA ARTIST")) {
        artist = line.substring(20);
        artist.trim();
        setTrack(title, artist);
      }

      // if connection was lost, give it time to fill the buffer
      if (disconnected) delay(10);
    }
  }

  if (stoppedPlaying) {
    if(DEBUG) Serial.println("stop playing");
    // clear screen
    matrix.fillScreen(0);
    matrix.swapBuffers(false);
  }
  if (disconnected) {
    if(DEBUG) Serial.println("disconnected");
    reenableDiscoverable();
  }
  delay(10);
}

// digital low pass filter (running average) of the EQ measurements
// we use powers of 2 for fast division (compiler optimizes to shift)
uint8_t lowPass(uint8_t bandIndex) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < LP_STEPS; i++) {
    sum += measurements[(i * BANDS) + bandIndex];
  }
  uint32_t result = sum * HEIGHT / LP_STEPS / 1024;
  return (uint8_t) result;
}

// EQ chip works using a clocked multiplexor. Every time
// strobe goes high, it outputs an analog voltage for the
// measurement. we need to give this output a few microseconds
// to settle, and then spend a few more microseconds measuring
// it with analogRead. To accomplish this we configure the 
// 16 bit timer in CTC mode. It calls the first interrupt
// to control the strobe using the A register and the measure
// interrupt uses the B register (same frequency, but a different
// trigger so it is out of phase, allowing for settle time of EQ
// chip).
void setupTimer() {
  // http://www.8bit-era.cz/arduino-timer-interrupts-calculator.html
  // TIMER 1 for interrupt frequency 5000 Hz:
  cli(); // stop interrupts
  TCCR5A = 0; // set entire TCCR1A register to 0
  TCCR5B = 0; // same for TCCR1B
  TCNT5  = 0; // initialize counter value to 0
  // set compare match register for 10000 Hz increments
  OCR5A = 3199; // = 16000000 / (1 * 5000) - 1 (must be <65536)
  OCR5B = 799;
  // turn on CTC mode
  TCCR5B |= (1 << WGM52);
  // Set CS12, CS11 and CS10 bits for 1 prescaler
  TCCR5B |= (0 << CS52) | (0 << CS51) | (1 << CS50);
  // enable timer compare interrupt
  TIMSK5 |= (1 << OCIE5A) | (1 << OCIE5B);
  sei(); // allow interrupts
}

void setupBluetooth() {
  // Serial1 is the UART interface of the bluetooth module
  Serial1.begin(9600);

  Serial1.print("RESET\r");
  while(1) {
    String line = Serial1.readStringUntil('\r');
    if (line.startsWith("Ready")) break;
  }
  if(DEBUG) Serial.println("ready");
}

// the device defaults to discoverable mode on power on, but
// after a disconnect it does not automatically return. so
// we have to listen for disconnect events and re-enable
// discoverability
void reenableDiscoverable() {
  Serial1.print("STATUS\r");
  String line = Serial1.readStringUntil('\r');
  if(DEBUG) Serial.println(line);
  String nextLine;
  do {
    nextLine = Serial1.readStringUntil('\r');
    if(DEBUG) Serial.println(nextLine);
  }while(!nextLine.startsWith("OK"));
  // if state is connectable but not discoverable
  connected = line.startsWith("STATE CONNECTED");
  if (line.startsWith("STATE CONNECTABLE IDLE")) {
    Serial1.print("DISCOVERABLE ON\r");
    Serial1.readStringUntil('\r'); // read the OK
    if(DEBUG) Serial.println("enabled discovery");
  }
}

// read from the eq buffer and draw the rectangles
void updateEqualizer() {
  for (uint8_t i = 0; i < BANDS; i++) {
    uint8_t m = lowPass(i);
    matrix.fillRect(bandOffsets[i], HEIGHT - m, bandWidths[i], m, color);
  }
}

// update the title and artist and reset the cursor position
void setTrack(String title, String artist) {
  track = artist + " - " + title;
  textX = WIDTH;
  textMin = track.length() * -12;
  titleShowing = true;
}

void showTrack() {
  if (titleShowing) {
    // update text position
    matrix.setTextColor(colorInverse);
    matrix.setCursor(textX, 8);
    matrix.print(track);
    // if text showing complete
    if((--textX) < textMin) {
      // stop showing, schedule next show
      titleShowing = false;
      nextTitleShow = millis() + TITLE_SHOW_PERIOD;
    }
  } else if (artist.length() > 0 && title.length() > 0 && millis() > nextTitleShow) {
    // if there is a title and it is time to show again
    setTrack(title, artist);
  }
}
