

void setup() {
  // put your setup code here, to run once:
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:

 for (int i = 0; i < 7; i++) {
  for (int p = 0; p < 7; p++) {
    analogWrite(((p + i) % 7) + 2, (32 * (p + 1)) - 1);
  }
  delay(100);
 }
}
