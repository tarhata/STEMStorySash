/* 
 This is Arduino code for the STEM Story Sash for a Lilypad MP3. 
 Modified from Mike Grusin & Bill Porter's Lilypad MP3 Player example sketch
 Team: Joanna Bailet, Tarhata Guiamelon, Letty Limbach, Lisa Schaefbauer
 University of Washington FizzLab
 May 2014
*/

// Include SPI library
#include <SPI.h>

// Include MP3 Player libraries
#include <SdFat.h>
#include <SdFatUtil.h>
#include <SFEMP3Shield.h>
#include <PinChangeInt.h>

// Include RFID library
#include <RFID.h>

// Include Adafruit NeoPixel library
#include <Adafruit_NeoPixel.h>

// Set debugging to true to get serial messages:
boolean debugging = false;

// Initial volume for the MP3 chip. 0 is the loudest, 255 is the lowest.
unsigned char volume = 0;

// Start up *not* playing:
boolean playing = true;

// Set loop_all to true if you would like to automatically
// start playing the next file after the current one ends:
boolean loop_all = false;


boolean interrupt = true;

// Set interruptself = true if you want the above rule to also
// apply to the same trigger. In other words, if interrupt = true
// and interruptself = false, subsequent triggers on the same
// file will NOT start the file over. However, a different trigger
// WILL stop the original file and start a new one.
boolean interruptself = false;

// We'll store the five filenames as arrays of characters.
// "Short" (8.3) filenames are used, followed by a null character.
char track[13] = "0MRWIG~1.MP3";

// Keep track of whether track is paused
boolean paused = false;

// Keep track of whether RFID badge has been found; Initially not found
boolean badgeFound = false;

// LilyPad MP3 pin definitions:

// Connect to RFID SDA pin
#define TRIG1 A0    

// Connect to Shift Register CLOCK/SH_CP pin
#define ROT_LEDG A1  

// Amplifier pin
#define SHDN_GPIO1 A2

// UNUSED PIN!
#define ROT_B A3

// Connect to DATA line of zipper potentiometer
#define TRIG2_SDA A4

// Connect to 'Play/Pause' switch
#define TRIG3_SCL A5

// Connect to external speakers
#define RIGHT A6
#define LEFT A7

// Connect to 'Next Track' switch
#define TRIG5_RXI 0

// Connect to 'Previous Track' switch
#define TRIG4_TXO 1

// MP3 Decoder
#define MP3_DREQ 2

// Connect to shift register LATCH/ST_CP pin
#define ROT_A 3

// Connect to shift register DATA pin
#define ROT_SW 4

// Connect to Neopixel DATA in pin
#define ROT_LEDB 5

// MP3 decoder pins
#define MP3_CS 6
#define MP3_DCS 7
#define MP3_RST 8

// SD card pin
#define SD_CS 9

// Connect to indicator red led
#define ROT_LEDR 10

// Connect to RFID SPI pins (standard Arduino pin numbers)
#define MOSI 11
#define MISO 12
#define SCK 13

// Shift register pins
#define DATA 3
#define LATCH 4
#define CLOCK A1

// Library objects:
SdFat sd;
SdFile file;
SFEMP3Shield MP3player;
RFID rfid (TRIG1, SD_CS);

// Array of all switchable sewable triggers
int trigger[3] = {
  TRIG3_SCL,TRIG4_TXO,TRIG5_RXI};

// RFID variables
unsigned char reading_card[5]; //for reading card
unsigned char first_nums[4] = {3, 165, 225, 4}; // first numbers for each RFID card
int rfid_B = 3;      // blue keychain
int rfid_W = 165;    // white card
int rfid_S1 = 228;   // sticker1
int rfid_S2 = 4;     // sticker2


/* NeoPixel variable
 ** Parameter 1 = number of pixels in strip
 ** Parameter 2 = Arduino pin number (most are valid)
 ** Parameter 3 = pixel type flags, add together as needed:
 ** NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
 ** NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
 ** NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
 ** NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
 */
Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, ROT_LEDB, NEO_GRB + NEO_KHZ800);

/* IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
 ** pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
 ** and minimize distance between Arduino and first pixel.  Avoid connecting
 ** on a live circuit...if you must, connect GND first.
 */

void setup()
{
  byte result;

  if (debugging)
  {
    Serial.begin(9600);
    Serial.println(F("Lilypad MP3 Player"));

    // ('F' places constant strings in program flash to save RAM)

    Serial.print(F("Free RAM = "));
    Serial.println(FreeRam(), DEC);
  }

  // Set up Shift register pins
  pinMode(DATA, OUTPUT);
  pinMode(CLOCK, OUTPUT);
  pinMode(LATCH, OUTPUT);

  // Set up MP3, amplifier, and SD card functionality pins:
  pinMode(MP3_CS, OUTPUT);
  pinMode(SHDN_GPIO1, OUTPUT);
  pinMode(MP3_DREQ, INPUT);
  pinMode(MP3_DCS, OUTPUT);
  pinMode(MP3_RST, OUTPUT);
  pinMode(SD_CS, OUTPUT); 

  // Set up DATA line of NeoPixel Ring
  pinMode(ROT_LEDB, OUTPUT); //D5

  // Red indicator LED 
  pinMode(ROT_LEDR, OUTPUT); //D10
  digitalWrite(ROT_LEDR, HIGH);

  // Look for data on zipper potentiometer
  //  pinMode(TRIG2_SDA, INPUT);

  // Setup 'Play/Pause' switch
  pinMode(TRIG3_SCL, INPUT);  //A5
  digitalWrite(TRIG3_SCL, HIGH); // turn on weak pullup

  // Setup 'Next Track' switch
  pinMode(TRIG4_TXO, INPUT);  //D1
  digitalWrite(TRIG4_TXO, HIGH); // turn on weak pullup  

  // Setup 'Previous Track' switch
  pinMode(TRIG5_RXI, INPUT);  //D0
  digitalWrite(TRIG5_RXI, HIGH); // turn on weak pullup  

  // Turn off amplifier chip / turn on MP3 mode:
  digitalWrite(SHDN_GPIO1, LOW);

  // Initialize the SD card:

  if (debugging) Serial.println(F("Initializing SD card... "));
  result = sd.begin(SD_SEL, SPI_HALF_SPEED);

  if (result != 1)
  {
    if (debugging) Serial.println(F("error, halting"));
  }
  else 
  {
    if (debugging) Serial.println(F("OK"));

    //Initialize the MP3 chip:

    if (debugging) Serial.println(F("Initializing MP3 chip... "));

    result = MP3player.begin();

    // Check result, 0 and 6 are OK:

    if((result != 0) && (result != 6))
    {
      // Blink red led indefinitely due to error
      while (true)
        indication(ROT_LEDR, 1000);
     
      if (debugging)
      {
        Serial.print(F("error "));
        Serial.println(result);
      }
    }

    // Get initial track:

    sd.chdir("/",true); // Index beginning of root directory
//    getNextTrack();
    if (debugging)
    {
      Serial.print(F("current track: "));
      Serial.println(track);
    }

    // Set initial volume (same for both left and right channels)
    MP3player.setVolume(volume, volume);

    // Uncomment to start MP3 playing at end of setup
//    startPlaying();  

    // Uncomment to get a directory listing of the SD card:
    // sd.ls(LS_R | LS_DATE | LS_SIZE);

    // Turn on amplifier chip:  
    digitalWrite(SHDN_GPIO1, HIGH);

    // Initialize RFID module
    rfid.init();

    // Initialize Neopixel Ring
    strip.begin();
    strip.show();    // set all pixels to "OFF" initially

    // Indicate that setup completed successfully
    for (int i=0; i<5; i++) 
    {
      indication(ROT_LEDR, 50);
    }  
  
    // Turn on all led constellations
    for (int i = 0; i < 256; i++)
    {
    updateLEDsLong(i);  
    }
  }
}


void loop()
{ 
  while (badgeFound == false)
  { 
    if (rfid.isCard())
    {   
      if (rfid.readCardSerial())
      {               
        /* Reading card */
        for (unsigned char i = 0; i < 5; i++)
        {     
          indication(ROT_LEDR, 50); 
          reading_card[i] = rfid.serNum[i];     
        }
        
        //verification
        int num = reading_card[0];
        if (num == rfid_B)
        {
          colorWipe(strip.Color(255, 10, 10), 10); // Wrong badge  
          colorWipe(strip.Color(0, 0, 0), 10); // Wrong badge        
        }   
        else if (num == rfid_W)
        {
          colorWipe(strip.Color(255, 10, 10), 10); // Wrong badge
          colorWipe(strip.Color(0, 0, 0), 10); // Wrong badge            
        }
        else if (num == rfid_S1)
        {
          theaterChase(strip.Color(127, 50, 0), 20); // Aeronautical Engineer badge
         
          // Indicate that badge has been found
          badgeFound = true;
          
          // Skip to first track of Save the Moon Story
          getNextTrack();          
          startPlaying();                
        }
        else if (num == rfid_S2)
        {
          colorWipe(strip.Color(0, 255, 0), 10); // Chemist badge                 
          
          // Start playing MP3
          
          indication(ROT_LEDR, 100);
          startPlaying();  
          delay(6500);
          colorWipe(strip.Color(0, 0, 0), 10); // Wrong badge            
          stopPlaying();           
        }       
      }        
    }
    rfid.halt();  
  } 

  int val = analogRead(TRIG2_SDA);
  changeVolume(val);
  if (debugging)
  {
    Serial.print(F("val = "));
    Serial.println(val);
    delay(5);
  }  
  int t;              // current trigger
  int last_t;
  byte result;

  // Step through the trigger inputs, looking for LOW signals.
  // The internal pullup resistors will keep them HIGH when
  // there is no connection to the input.

  // If serial debugging is on, only check triggers 1-3,
  // otherwise check triggers 1-5.
  
  for(t = 3; t <= (debugging ? 3 : 5); t++)
  {
    // The trigger pins are stored in the trigger[] array.
    // Read the pin and check if it is LOW (triggered).
    if (digitalRead(trigger[t-3]) == LOW)
    {

      // Wait for trigger to return high for a solid 50ms
      // (necessary to avoid switch bounce on T2 and T3
      // since we need those free for I2C control of the
      // amplifier)   
      int x;
      while(x < 50)
      {
        if (digitalRead(trigger[t-3]) == HIGH)
          x++;
        else
          x = 0;
        delay(1);
      } 
      
      digitalWrite(ROT_LEDR, LOW);
      delay(200);
      
      if (debugging)
      {
        Serial.print(F("got trigger "));
        Serial.println(t);
      }

      if (t == 5) // Trigger 5: Play/Pause
      { 
        if (MP3player.isPlaying() && paused == false)
        {     
          paused = true;          
          MP3player.pauseDataStream();
        }
        else
        {   
          MP3player.resumeDataStream();
          paused = false;
        }  
      }
      else if (t == 4) //Trigger 4: Next Track
      {  
        stopPlaying();
        getNextTrack();   
     
        if (track[0] == '0')  //Skip to beginning of Save the Moon story
          getNextTrack();
          
        startPlaying();
      } 
      else if (t == 3) //Trigger 3; Previous Track
      {  
        stopPlaying();
        getPrevTrack();
        
        if (track[0] == '0')  //Skip to end of Save the Moon story
          getPrevTrack();
          
        startPlaying();
      }
    }
  } 
}

void changeVolume(int val)
{
  // Increment or decrement the volume.
  // This is handled internally in the VS1053 MP3 chip.
  // Lower numbers are louder (0 is the loudest).

  // Turn off volume once zipper potentiometer is fully open
  if (val <= 725) 
  {
    volume = 255;
  }
  // Turn up volume to loudest once zipper potentiometer is fully closed
  else if  (val > 760) {
    volume = 0;
  }
  // Scale volume depending on zipper position on potentiometer
  else {
    volume = map(val, 725, 760, 40, 1);
  }

  MP3player.setVolume(volume, volume);
  indication(ROT_LEDR, 75);
}


void getNextTrack()
{
  // Get the next playable track (check extension to be
  // sure it's an audio file)

  do
    getNextFile();
  while(isPlayable() != true);
}


void getPrevTrack()
{
  // Get the previous playable track (check extension to be
  // sure it's an audio file)

  do
    getPrevFile();
  while(isPlayable() != true);
}


void getNextFile()
{
  // Get the next file (which may be playable or not)

  int result = (file.openNext(sd.vwd(), O_READ));

  // If we're at the end of the directory,
  // loop around to the beginning:

  if (!result)
  {
    sd.chdir("/",true);
    getNextTrack();
    return;
  }
  file.getFilename(track);  
  file.close();
}


void getPrevFile()
{
  // Get the previous file (which may be playable or not)

  char test[13], prev[13];

  // Getting the previous file is tricky, since you can
  // only go forward when reading directories.

  // To handle this, we'll save the name of the current
  // file, then keep reading all the files until we loop
  // back around to where we are. While doing this we're
  // saving the last file we looked at, so when we get
  // back to the current file, we'll return the previous
  // one.

  // Yeah, it's a pain.

  strcpy(test,track);

  do
  {
    strcpy(prev,track);
    getNextTrack();
  }
  while(strcasecmp(track,test) != 0);

  strcpy(track,prev);
}


void startPlaying()
{
  int result;

  if (debugging)
  {
    Serial.print(F("playing "));
    Serial.print(track);
    Serial.print(F("..."));
  }  

  result = MP3player.playMP3(track);

  if (debugging)
  {
    Serial.print(F(" result "));
    Serial.println(result);  
  }
}

void pausePlaying()
{

  if (debugging)
  {
    Serial.print(F("playing "));
    Serial.print(track);
    Serial.print(F("..."));
  }  

  MP3player.pauseMusic();

  if (debugging)
  {
    Serial.print(F("playback paused"));
  }
}

void stopPlaying()
{
  if (debugging) Serial.println(F("stopping playback"));
  MP3player.stopTrack();
}


boolean isPlayable()
{
  // Check to see if a filename has a "playable" extension.
  // This is to keep the VS1053 from locking up if it is sent
  // unplayable data.

  char *extension;

  extension = strrchr(track,'.');
  extension++;
  if (
  (strcasecmp(extension,"MP3") == 0) ||
    (strcasecmp(extension,"WAV") == 0) ||
    (strcasecmp(extension,"MID") == 0) ||
    (strcasecmp(extension,"MP4") == 0) ||
    (strcasecmp(extension,"WMA") == 0) ||
    (strcasecmp(extension,"FLA") == 0) ||
    (strcasecmp(extension,"OGG") == 0) ||
    (strcasecmp(extension,"AAC") == 0)
    )
    return true;
  else
    return false;
}


void indication(int LED_PIN, long time)
{
  digitalWrite(LED_PIN, HIGH);
  delay(time);
  digitalWrite(LED_PIN, LOW);
  delay(time);
}

/* 
***********************************************************************
NEOPIXEL FUNCTIONS
***********************************************************************
*/

// Fill the dots one after the other with a colors
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

void colorWipeRev(uint32_t c, uint8_t wait) {
  for(uint16_t i=strip.numPixels(); i>=0; i--) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait) {
  for (int j=0; j<10; j++) {  //do 10 cycles of chasing
    for (int q=0; q < 3; q++) {
      for (int i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, c);    //turn every third pixel on
      }
      strip.show();

      delay(wait);

      for (int i=strip.numPixels(); i >=0; i=i-3) {
        strip.setPixelColor(i+q, 10, 127, 127);        //turn every third pixel off
      }
    }
  }
}

//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {
  for (int j=0; j < 256; j++) {     // cycle all 256 colors in the wheel
    for (int q=0; q < 3; q++) {
      for (int i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
      }
      strip.show();

      delay(wait);

      for (int i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
    return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } 
  else if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } 
  else {
    WheelPos -= 170;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

/* ****************************************************
Shift Register Code!
*******************************************************
*/
void updateLEDsLong(int value){
  digitalWrite(LATCH, LOW);    //Pulls the chips latch low
  for(int i = 0; i < 8; i++){  //Will repeat 8 times (once for each bit)
  int bit = value & B10000000; //We use a "bitmask" to select only the eighth 
                               //bit in our number (the one we are addressing this time through
  value = value << 1;          //we move our number up one bit value so next time bit 7 will be
                               //bit 8 and we will do our math on it
//  if(bit == 128){digitalWrite(data, HIGH);} //if bit 8 is set then set our data pin high
//  else{digitalWrite(data, LOW);} //if bit 8 is unset then set the data pin low
  digitalWrite(DATA, HIGH);
  digitalWrite(CLOCK, HIGH);                //the next three lines pulse the clock pin
  delay(1);
  digitalWrite(CLOCK, LOW);
  }
  digitalWrite(LATCH, HIGH);  //pulls the latch high shifting our data into being displayed
}


void updateLEDs(int value){
  digitalWrite(LATCH, LOW);     //Pulls the chips latch low
  shiftOut(DATA, CLOCK, MSBFIRST, value); //Shifts out the 8 bits to the shift register
  digitalWrite(LATCH, HIGH);   //Pulls the latch high displaying the data
}
