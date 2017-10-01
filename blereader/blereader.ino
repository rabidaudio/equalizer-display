
void setup() {
  // put your setup code here, to run once:

  Serial.begin(9600);
  Serial1.begin(9600);

//  Serial.println("configuring");
  Serial1.print("RESET\r");
  while(1) {
    String line = Serial1.readStringUntil('\r');
    if (line.startsWith("Ready")) break;
  }
  Serial1.print("SET MUSIC_META_DATA=ON\r");
  while(1) {
    String line = Serial1.readStringUntil('\r');
    if (line.startsWith("OK")) break;
  }
//  Serial.println("ready");
}

/*
AVRCP_PAUSE 11
AVRCP_PLAY 11
AVRCP_MEDIA TITLE: Brown Eyes
AVRCP_MEDIA ARTIST: Lady Gaga
AVRCP_MEDIA ALBUM: The Fame
AVRCP_MEDIA NUMBER: 0
AVRCP_MEDIA TOTAL_NUMBER: 0
AVRCP_MEDIA PLAYING_TIME(MS): 243000
AVRCP_PAUSE 11
A2DP_STREAM_SUSPEND 10
*/

String title = "";
String artist = "";
boolean playing = false;
boolean hasShownTitle = false;

void loop() {
//  if (Serial1.available()) {
//    while (Serial1.available()) {
//      String line = Serial1.readStringUntil('\r');
//      if (line.startsWith("AVRCP_PLAY") || line.startsWith("A2DP_STREAM_START")) {
//        playing = true;
//      } else if (line.startsWith("AVRCP_PAUSE") || line.startsWith("A2DP_STREAM_SUSPEND")) {
//        playing = false;
//        title = "";
//        artist = "";
//      } else if (line.startsWith("AVRCP_MEDIA TITLE")) {
//        title = line.substring(19);
//        title.trim();
//      } else if (line.startsWith("AVRCP_MEDIA ARTIST")) {
//        artist = line.substring(20);
//        artist.trim();
//        hasShownTitle = false;
//      }
//      delay(10);
//    }
//  }
//
//  if (playing && !hasShownTitle && title.length() > 0 && artist.length() > 0) {
//    Serial.print(artist);
//    Serial.print(" - ");
//    Serial.println(title);
//    hasShownTitle = true;
//  }
  while (Serial1.available()) {
    byte x = Serial1.read();
    Serial.write(x);
    if (x == '\r') {
      Serial.write('\n');
    }
  }
  if (Serial.available()) {
    Serial1.write(Serial.read());
  }
}
