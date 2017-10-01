#define RESET 11
#define STROBE 12

#define BANDS 7
#define LP_STEPS 16
#define BUCKETS 256

#define DEBUG 0

uint8_t measureIndex = 0;
uint8_t passIndex = 0;
uint16_t measurements[BANDS * LP_STEPS];

// not overly readable code, but shooting for efficency.
// resetState counts down from 3 at each strobe until 0.
// on 3 and 1 we can toggle reset, and when it hits 0
// we can start taking measurements.
uint8_t resetState = 3;

void setup() {
  // put your setup code here, to run once:

  pinMode(STROBE, OUTPUT);
  pinMode(RESET, OUTPUT);

  for (uint8_t i = 0; i < BANDS; i++) {
    pinMode(i + 2, OUTPUT);
  }

  if(DEBUG) Serial.begin(9600);

  setupBluetooth();

  setupTimer();
}

ISR(TIMER5_COMPA_vect){
  // on strobe off, take measurement
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
ISR(TIMER5_COMPB_vect) {
  // if resetState is 0 and strobe is low
  if (!resetState && !(PORTB & (1 << 6))) {
    // take measurement
    measurements[(passIndex * BANDS) + measureIndex] = analogRead(A0);
    // increment measureIndex
    measureIndex = (measureIndex + 1) % BANDS;
    if (!measureIndex) {
      passIndex = (passIndex + 1) % LP_STEPS;
    }
  }
}

String title = "";
String artist = "";
boolean playing = false;
boolean connected = false;
boolean hasShownTitle = false;

void loop() {
  // put your main code here, to run repeatedly:
//  Serial.print("Values: ");
//  Serial.print(lowPass(0), DEC);
//  Serial.print("\t");
//  Serial.print(lowPass(1), DEC);
//  Serial.print("\t");
//  Serial.print(lowPass(2), DEC);
//  Serial.print("\t");
//  Serial.print(lowPass(3), DEC);
//  Serial.print("\t");
//  Serial.print(lowPass(4), DEC);
//  Serial.print("\t");
//  Serial.print(lowPass(5), DEC);
//  Serial.print("\t");
//  Serial.println(lowPass(6), DEC);
//  delay(10);

  boolean stoppedPlaying = false;
  boolean disconnected = false;
  if (Serial1.available()) {
    while (Serial1.available()) {
      String line = Serial1.readStringUntil('\r');
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
        if(DEBUG) Serial.println("disconnected");
        reenableDiscoverableIfNeccessary();
      } else if (line.startsWith("AVRCP_PAUSE") || line.startsWith("A2DP_STREAM_SUSPEND")) {
        if (playing) {
          stoppedPlaying = true;
        }
        playing = false;
        title = "";
        artist = "";
        if(DEBUG) Serial.println("stopped");
      } else if (line.startsWith("AVRCP_MEDIA TITLE")) {
        title = line.substring(19);
        title.trim();
      } else if (line.startsWith("AVRCP_MEDIA ARTIST")) {
        artist = line.substring(20);
        artist.trim();
        hasShownTitle = false;
      }
      delay(10);
    }
  }
  printSongNameIfNeccessary();
  if (playing) {
    updateEqualizer();
  } else {
    if (stoppedPlaying){
      if(DEBUG) Serial.println("stop playing");
      clearEqualizer();
    }
    if (disconnected) {
      delay(100);
      reenableDiscoverableIfNeccessary();
    }
    delay(100);
  }
}

uint32_t lowPass(uint8_t bandIndex) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < LP_STEPS; i++) {
    sum += measurements[(i * BANDS) + bandIndex];
  }
  uint32_t result = sum * BUCKETS / LP_STEPS / 1024;
  return (uint8_t) result;
//  return (uint8_t)(sum / LP_STEPS / 1024 * BUCKETS);
//  return sum / LP_STEPS;
}

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
  Serial1.begin(9600);

  Serial1.print("RESET\r");
  while(1) {
    String line = Serial1.readStringUntil('\r');
    if (line.startsWith("Ready")) break;
  }
  if(DEBUG) Serial.println("ready");
}

void reenableDiscoverableIfNeccessary() {
  Serial1.print("STATUS\r");
  String line = Serial1.readStringUntil('\r');
  String nextLine;
  do {
    nextLine = Serial1.readStringUntil('\r');
  }while(!nextLine.startsWith("OK"));
  // if state is connectable but not discoverable
  connected = line.startsWith("STATE CONNECTED");
  if (line.startsWith("STATE CONNECTABLE IDLE")) {
    Serial1.print("DISCOVERABLE ON\r");
    Serial1.readStringUntil('\r'); // read the OK
  }
}

void updateEqualizer() {
  for (uint8_t i = 0; i < BANDS; i++) {
    analogWrite(i + 2, lowPass(i));
  }
}

void clearEqualizer() {
  for (uint8_t i = 0; i < BANDS; i++) {
    analogWrite(i + 2, 0);
  }
}

void printSongNameIfNeccessary() {
    if (playing && !hasShownTitle && title.length() > 0 && artist.length() > 0) {
    if(DEBUG) Serial.print(artist);
    if(DEBUG) Serial.print(" - ");
    if(DEBUG) Serial.println(title);
    hasShownTitle = true;
  }
}

