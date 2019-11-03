#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_TFTLCD.h> // Hardware-specific library
#include <TouchScreen.h>     //TOuch screen library
#include <SD.h>
#include <SPI.h>

#define LCD_CS A3 // Chip Select goes to Analog 3
#define LCD_CD A2 // Command/Data goes to Analog 2
#define LCD_WR A1 // LCD Write goes to Analog 1
#define LCD_RD A0 // LCD Read goes to Analog 0

#define YP A3  // must be an analog pin
#define XM A2  // must be an analog pin
#define YM 9   // can be a digital pin
#define XP 8   // can be a digital pin

#define SD_CS 10  // Set the chip select line to whatever you use (10 doesnt conflict with the library)

#define TS_MINX 204
#define TS_MINY 195
#define TS_MAXX 948
#define TS_MAXY 910

//These are color codes
#define  BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

Adafruit_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, A4);
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

char playStatus,currentPage;
char incoming[16];
boolean Touch;
int volume=40;

#define MINPRESSURE 10
#define MAXPRESSURE 1000

void setup()
{
  Serial.begin(9600);
   delay(500);
   
  tft.reset();
  uint16_t identifier = tft.readID();
  identifier=0x9341;
  tft.begin(identifier);
  tft.setRotation(-1);

  Serial.print(F("Initializing SD card..."));
  if (!SD.begin(SD_CS)) {
    Serial.println(F("failed!"));
    return;
  }
  Serial.println(F("OK!"));
  delay(1500);
  tft.fillScreen(BLACK);
  drawHomeScreen();  // Draws the Home Screen
  currentPage = '0'; // Indicates that we are at Home Screen
  playStatus = '0';
}

void loop(void)
{ 
  
  CheckTouchscreen();

   while (Serial.available()>=7) {

       for(int i=0; i<=6; i++)
       {
        incoming[i]= (char)Serial.read();
        
       }
    }
    printRadioStation();

          
}

#define BUFFPIXEL 20
void bmpDraw(char *filename, int x, int y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel in buffer (R+G+B per pixel)
  uint16_t lcdbuffer[BUFFPIXEL];  // pixel out buffer (16-bit per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();
  uint8_t  lcdidx = 0;
  boolean  first = true;

  if((x >= tft.width()) || (y >= tft.height())) return;

 
  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.println(F("File not found"));
    return;
  }

  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.print(F("File size: ")); 
    Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print(F("Image Offset: ")); 
    Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print(F("Header size: ")); 
    Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print(F("Bit Depth: ")); 
      Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if((x+w-1) >= tft.width())  w = tft.width()  - x;
        if((y+h-1) >= tft.height()) h = tft.height() - y;

        // Set TFT address window to clipped image bounds
        tft.setAddrWindow(x, y, x+w-1, y+h-1);

        for (row=0; row<h; row++) {        
          if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if(bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          for (col=0; col<w; col++) { // For each column...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              // Push LCD buffer to the display first
              if(lcdidx > 0) {
                tft.pushColors(lcdbuffer, lcdidx, first);
                lcdidx = 0;
                first  = false;
              }
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            lcdbuffer[lcdidx++] = tft.color565(r,g,b);
          } // end pixel
        } // end scanline
        // Write any remaining data to LCD
        if(lcdidx > 0) {
          tft.pushColors(lcdbuffer, lcdidx, first);
        } 
        
      } // end goodBmp
    }
  }

  bmpFile.close();
  if(!goodBmp) Serial.println(F("BMP format not recognized."));
}

uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

void drawMusicPlayerScreen()
{  
   tft.fillScreen(BLACK);
   printMenu(); 
   drawPlayButton();
   if (playStatus == '2') {
    drawPauseButton();
   } 
   drawPreviousButton();
   drawNextButton();
   drawVolumeUp();
   drawVolumeDown();
   
   printRadioStation();
   
   tft.drawLine(0,26,319,26,RED);

    // Volume Bar
  tft.fillRect (65, 165, 65 + 120,10,WHITE);
  tft.fillRect (65, 165, 65 + 75, 10,YELLOW);

}

void drawPreviousButton()
{
  bmpDraw("orn5.bmp", 45, 75);
}

void drawVolumeDown()
{
   bmpDraw("orn6.bmp", 30, 158);
}
void drawVolumeUp()
{
   bmpDraw("orn7.bmp", 261, 155);
}

void drawNextButton()
{
  bmpDraw("orn4.bmp", 227, 75);
}

void drawVolume(int volumeUp,int VolumeDown) {
  tft.fillRect (65 + VolumeDown/10, 165, 65 + 120, 10,WHITE);
  tft.fillRect (65, 165, 78 + volumeUp/10, 10,YELLOW);
}

void drawPlayButton()
{
  bmpDraw("orn3.bmp", 130, 62);
}
void drawPauseButton()
{
  bmpDraw("orn1.bmp", 130, 62);
}

void printRadioStation()
{  
   //Print The Radio Station
   tft.setTextColor(WHITE);
   tft.setTextSize(2);
   tft.setCursor(130,200);
   tft.print(incoming);
   incoming[0]= '\0'; 
    
}

void printMenu()
{
   tft.drawRect(0,0,319,240,WHITE);

   //Print MENU
   tft.setTextColor(RED);
   tft.setTextSize(2);
   tft.setCursor(5,5);
   tft.print("MENU");

   //Print Media Center
   tft.setTextColor(WHITE);
   tft.setTextSize(1);
   tft.setCursor(130,5);
   tft.print("MediaCenter");
     
   //Print Team 6
   tft.setTextColor(WHITE);
   tft.setTextSize(2);
   tft.setCursor(232,5);
   tft.print("Team 6");
}
void drawMp3TExt()
{
  tft.setCursor(100,36);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.print("Mp3 Running");
}

void drawRadioTExt()
{
  tft.setCursor(30,36);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.print("Internet Radio Running");
}
void drawHomeScreen()
{
  tft.fillScreen(BLACK);
  tft.drawRect(0,0,319,240,WHITE);


  bmpDraw("orn3.bmp", 20, 40);
  tft.setCursor(120,55);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.print("Internet Radio");

  //Print "Listen from MP3"
  bmpDraw("orn3.bmp", 20, 170);
  tft.setCursor(120,185);
  tft.setTextColor(WHITE);
  tft.setTextSize(3);
  tft.print("MP3");
}

void CheckTouchscreen(){
      
      TSPoint p, q;
      int upCount = 0;
   
      // Wait for screen press
      do
      {
      q = ts.getPoint();
      delay(1);
      }
       while( q.z < MINPRESSURE ||  q.z > MAXPRESSURE );
      // Save initial touch point
      p.x = q.x; p.y = q.y; p.z = q.z;
       
         // Wait for finger to come up
      do
        {
          q = ts.getPoint();
          if ( q.z < MINPRESSURE || q.z > MAXPRESSURE  )
        {
         upCount++;
        }
      else
        {
          upCount = 0;
          p.x = q.x; p.y = q.y; p.z = q.z;
          Touch = false;
        }
       //delay(2);             // Try varying this delay
       } while( upCount < 1 );  // and this count for different results.
     
     // Use this to set the direction and size of the touchscreen compared to the display itself
   p.x = map(p.x, TS_MINX, TS_MAXX, 240, 0);//default is (240, 0) [default puts touch cord. 0=x/y upper right.
   p.y = map(p.y, TS_MINY, TS_MAXY, 320, 0);//default is (320, 0) [I change these cause i like 0=xy bottom left.

 
   // Clean up pin modes for LCD
   pinMode(XM, OUTPUT);
   digitalWrite(XM, LOW);
   pinMode(YP, OUTPUT);
   digitalWrite(YP, HIGH);
   pinMode(YM, OUTPUT);
   digitalWrite(YM, LOW);
   pinMode(XP, OUTPUT);
   digitalWrite(XP, HIGH);

   

    //checks touch x/y if pressed and sends them your serial monitor
   if (p.z > ts.pressureThreshhold) {
  Serial.print("Y = "); Serial.print(p.y);     
  Serial.print("\tX = "); Serial.print(p.x);  //  \t= space
  Serial.print("\tPressure = "); Serial.println(p.z);
  delay(80); //just to slow down the readings a little bit
   }
  
     if(p.x>180 && p.x<200 && p.y>305 && p.y<325) //The user has pressed start button
      {                      
         drawMusicPlayerScreen();
         drawRadioTExt();
         currentPage = '1'; 

          pinMode(XM, OUTPUT);
          digitalWrite(XM, LOW);
          pinMode(YP, OUTPUT);
          digitalWrite(YP, HIGH);
          pinMode(YM, OUTPUT);
          digitalWrite(YM, LOW);
          pinMode(XP, OUTPUT);
          digitalWrite(XP, HIGH);
      }

       if(p.x>20 && p.x<50 && p.y>305 && p.y<325) //The user has pressed start button
      {                      
         drawMusicPlayerScreen();
         drawMp3TExt();
         currentPage = '2'; 

          pinMode(XM, OUTPUT);
          digitalWrite(XM, LOW);
          pinMode(YP, OUTPUT);
          digitalWrite(YP, HIGH);
          pinMode(YM, OUTPUT);
          digitalWrite(YM, LOW);
          pinMode(XP, OUTPUT);
          digitalWrite(XP, HIGH);
      }

 //---------------------- FROM RADIO -----------------------------------------
 
   if (currentPage == '1') {
         
   if(p.x>145 && p.x<170 && p.y>160 && p.y<185)// The user has pressed inside the button play bitmap
   {  
      
      if (playStatus == '0') {       
          drawPauseButton();
          Serial.println("play");
          playStatus = '2';
          return;
         } 

         if (playStatus == '1') {
          drawPauseButton();
          Serial.println("play");
          playStatus = '2';
          return;
        }
       
       if (playStatus == '2') {
          drawPlayButton();
          Serial.println("stop");
          delay(100);
          playStatus = '1';
          return;
        }           
   } 

   if(p.x>165 && p.x<185 && p.y>80 && p.y<100)// The user has pressed NextButton
   {  
      Serial.println("nextStation");   
   }

    if(p.x>157 && p.x<170 && p.y>285 && p.y<300)// The user has pressed PreviousButton
   {  

      Serial.println("previousStation"); 
    
   }

   if(p.x>70 && p.x<82 && p.y>40 && p.y<58)// The user has pressed VolumeUp Button
   {  
        if (volume > 0 & volume < 100) {
          volume=volume+10;
          drawVolume(volume,0);
        }
        Serial.print("volumeUp:");
        Serial.println(volume);
        delay(100);   
   }

   if(p.x>60 && p.x<79 && p.y>325 && p.y<340)// The user has pressed VolumeDown Button
   {  

   if (volume > 0 & volume <100) {
          volume=volume-10;
          drawVolume(0,volume);
        }
        Serial.print("volumeDown:");
        Serial.println(volume);
        delay(100);  
   }
     
   }

   //------------------------ FROM MP3 -------------------------------------

   if(currentPage=='2')
   {
      //For ButtonPlay
      if(p.x>145 && p.x<170 && p.y>160 && p.y<185)
      {
          if (playStatus == '0') {  
          drawPauseButton();
          Serial.println("mp3Play");
          playStatus = '2';
          return;
         } 

         if (playStatus == '1') {
          drawPauseButton();
          Serial.println("mp3Play");
          playStatus = '2';
          return;
        }
       
       if (playStatus == '2') {
          drawPlayButton();
          Serial.println("mp3Stop");
          delay(100);
          playStatus = '1';
          return;
        } 
      }

      if(p.x>165 && p.x<185 && p.y>80 && p.y<100)// The user has pressed NextButton
      {  
        Serial.println("nextFile");   
      }

      if(p.x>150 && p.x<175 && p.y>285 && p.y<300)// The user has pressed PreviousButton
      {  

        Serial.println("previousFile"); 
      }

      if(p.x>70 && p.x<82 && p.y>40 && p.y<58)// The user has pressed VolumeUp Button
      {  
        if (volume > 0 & volume < 100) {
          volume=volume+10;
          drawVolume(volume,0);
        }
         Serial.print("mp3VolUp:");
         Serial.println(volume);
         delay(100);   
       }

      if(p.x>60 && p.x<79 && p.y>325 && p.y<340)// The user has pressed VolumeDown Button
      {  

        if (volume > 0 & volume <100) {
          volume=volume-10;
          drawVolume(0,volume);
        }
        Serial.print("mp3VolDown:");
        Serial.println(volume);
        delay(100);  
   }
      
   }
     
}
