#define RESET 11
#define STROBE 12

#define BANDS 7
#define LP_STEPS 16
#define BUCKETS 8

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
  Serial.begin(9600);

  setupTimer();
}

ISR(TIMER3_COMPA_vect){
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
ISR(TIMER3_COMPB_vect) {
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

void loop() {
  // put your main code here, to run repeatedly:
//  Serial.print("Values: ");
  Serial.print(lowPass(0), DEC);
  Serial.print("\t");
  Serial.print(lowPass(1), DEC);
  Serial.print("\t");
  Serial.print(lowPass(2), DEC);
  Serial.print("\t");
  Serial.print(lowPass(3), DEC);
  Serial.print("\t");
  Serial.print(lowPass(4), DEC);
  Serial.print("\t");
  Serial.print(lowPass(5), DEC);
  Serial.print("\t");
  Serial.println(lowPass(6), DEC);
//  delay(10);
}

uint8_t lowPass(uint8_t bandIndex) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < LP_STEPS; i++) {
    sum += measurements[(i * BANDS) + bandIndex];
  }
//  return sum;  
  return sum / LP_STEPS / (1024 / BUCKETS);
//  return sum / LP_STEPS;
}

void setupTimer() {
  // http://www.8bit-era.cz/arduino-timer-interrupts-calculator.html
  // TIMER 3 for interrupt frequency 5000 Hz:
  cli(); // stop interrupts
  TCCR3A = 0; // set entire TCCR1A register to 0
  TCCR3B = 0; // same for TCCR1B
  TCNT3  = 0; // initialize counter value to 0
  // set compare match register for 10000 Hz increments
  OCR3A = 3199; // = 16000000 / (1 * 5000) - 1 (must be <65536)
  OCR3B = 799;
  // turn on CTC mode
  TCCR3B |= (1 << WGM32);
  // Set CS32, CS31 and CS30 bits for 1 prescaler
  TCCR3B |= (0 << CS32) | (0 << CS31) | (1 << CS30);
  // enable timer compare interrupt
  TIMSK3 |= (1 << OCIE3A) | (1 << OCIE3B);
  sei(); // allow interrupts
}

