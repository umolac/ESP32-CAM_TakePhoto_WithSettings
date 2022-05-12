/*
 * Better ESP32-CAM taking photo made with love by AUT.
 * I loved the most basic tutorial about how to take photos with the ESP32-CAM, but I've found it... Insufficient, and without showing off
 * camera settings that camera can make. So, I decided to make my own, which I believe is a tad more complete. It shows off camera settings you're capable of changing
 * and how to use the camera, in setup and in loop functions!
 * 
 */


 //Includes
#include "esp_camera.h"        // Camera driver
#include "FS.h"                // Filesystem (to read and to write to SD CARD)
#include "SD_MMC.h"            // SD Card interface
#include <EEPROM.h>            // read and write from flash memory



#define EEPROM_SIZE 1 //Number of bytes we want to access to EEPROM

// Camera pin definitions
#define PWDN_GPIO_NUM     32 //Camera power control pin
#define RESET_GPIO_NUM    -1 //Camera reset pin
#define XCLK_GPIO_NUM      0 //Camera MCLK (in schematic)
#define SIOD_GPIO_NUM     26 //Camera I2C SDA 
#define SIOC_GPIO_NUM     27 //Camera I2C SCL

#define Y9_GPIO_NUM       35 //Camera D7
#define Y8_GPIO_NUM       34 //Camera D6
#define Y7_GPIO_NUM       39 //Camera D5
#define Y6_GPIO_NUM       36 //Camera D4
#define Y5_GPIO_NUM       21 //Camera D3
#define Y4_GPIO_NUM       19 //Camera D2
#define Y3_GPIO_NUM       18 //Camera D1
#define Y2_GPIO_NUM        5 //Camera D0
#define VSYNC_GPIO_NUM    25 //Camera VSYNC
#define HREF_GPIO_NUM     23 //Camera HSYNC
#define PCLK_GPIO_NUM     22 //Camera PCLK



int pictureNumber = 0;    //Number of images at start should be 0, we're reading this number from EEPROM, and we store it as well

//Setup
void setup() {    //Statting of the setup loop
  Serial.begin(115200); //Starting serial
  Serial.setDebugOutput(true); //I left serial debug option on. It can be quite useful
  Serial.println("Starting config..."); 
  camera_config_t config; //Definition of camera config. Required so that program knows which pins are connected to the camera sensor
    config.ledc_channel = LEDC_CHANNEL_0; //LEDC channel (NOT to be confused with flash LED on ESP32-CAM, that is connected to GPIO4)
    config.ledc_timer = LEDC_TIMER_0; //LEDC timer
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000; //We are using 20 MHz clock. There is an experimental 15MHz clock. 
    config.pixel_format = PIXFORMAT_JPEG; //We want sensor to output JPEG format of every picture. We can also use YUV442, GRAYSCALE and RGB565 formats, if desired.
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; 
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 12; //Here I did put 12 as quality. The higher the number here, the lower the quality of the photo. You shouldn't go lower than 4, by my experience. 
    config.fb_count = 1;

  Serial.println("Pin config completed. Configuring format...");
  if(config.pixel_format == PIXFORMAT_JPEG){
    if(psramFound()){ //Looking for aditional PSRAM for processing of photos. It's highly recommended to use PSRAM for that, because internal RAM of ESP32 is sometimes just too small
      Serial.println("I've found PSRAM!");
      config.jpeg_quality = 5; //If we have PSRAM, we can increase JPEG quality to 5 without worries
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
      config.frame_size = FRAMESIZE_UXGA; //You can set other sizes as well, here I'm using UXGA, which is the largest size supported by the sensor on ESP32-CAM (OV2640)
      //Other sizes you can do are: QVGA, CIF, VGA, SVGA, XGA and SXGA, with OV2640 sensor
      
      Serial.println("Format: JPEG; Quality: 5; Resolution: UXGA");
    } else {
      Serial.println ("I didn't find PSRAM, I'll be limiting the format!");
      // Limit the frame size when PSRAM is not available, and JPEG quality
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
      config.jpeg_quality = 15;
      Serial.println("Format: JPEG; Quality: 15; Resolution: SVGA");
    }
  } else {
    config.frame_size = FRAMESIZE_UXGA; //If desired format isn't JPEG, it will always use UXGA size of frame. You can lower that as well
  }

  //Camera init
  Serial.println("Initializing camera...");
  esp_err_t err = esp_camera_init(&config); //To catch an error, if it happens during camera init
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  Serial.println("Success! Setting up camera parameters..."); //Setting up the parameters for image processing. With those, you can take photos even in darker areas
  
  
  /*
   * If you ever saw the example of ESP32-CAM server, you did saw those parameters. This is now you use it.
   */

  //I think that setting names are pretty self-explanatory
  
  sensor_t * s = esp_camera_sensor_get();
  s->set_brightness(s, 0);                        // -2 to 2
  s->set_contrast(s, 1);                          // -2 to 2
  s->set_saturation(s, -1);                       // -2 to 2
  s->set_special_effect(s, 0);                    // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
  s->set_whitebal(s, 1);                          // 0 = disable , 1 = enable
  s->set_awb_gain(s, 1);                          // 0 = disable , 1 = enable
  s->set_wb_mode(s, 1);                           // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
  s->set_exposure_ctrl(s, 1);                     // 0 = disable , 1 = enable
  s->set_aec2(s, 1);                              // 0 = disable , 1 = enable
  s->set_ae_level(s, -2);                         // -2 to 2
  s->set_aec_value(s, 300);                       // 0 to 1200
  s->set_gain_ctrl(s, 1);                         // 0 = disable , 1 = enable
  s->set_agc_gain(s, 0);                          // 0 to 30
  s->set_gainceiling(s, (gainceiling_t)0);        // 0 to 6
  s->set_bpc(s, 1);                               // 0 = disable , 1 = enable
  s->set_wpc(s, 1);                               // 0 = disable , 1 = enable
  s->set_raw_gma(s, 1);                           // 0 = disable , 1 = enable
  s->set_lenc(s, 1);                              // 0 = disable , 1 = enable
  s->set_hmirror(s, 0);                           // 0 = disable , 1 = enable
  s->set_vflip(s, 0);                             // 0 = disable , 1 = enable
  s->set_dcw(s, 1);                               // 0 = disable , 1 = enable
  s->set_colorbar(s, 0);                          // 0 0 disable , 1 = enable
  
 

  Serial.println("Parameters set up!");


  //Mounting SD card
  Serial.println("Mounting SD card...");
  if(!SD_MMC.begin()){                            //If some error happens while mounting SD card, we want information about it
    Serial.println("SD Card Mount Failed");
    return;
  }  
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){                      //If there isn't any SD card attached to ESP32-CAM, we want information about it as well
    Serial.println("No SD Card attached");
    return;
  }

  Serial.println("Capturing photo...");
  camera_fb_t  * fb = esp_camera_fb_get();        //Calling a function to take a photo
  if(!fb) {
    Serial.println("Camera capture failed");      //If photo cannot be taken, we want info as well. That usually happens if you set quality too high
    return;
  }
  Serial.println("Photo has been taken");
  Serial.println("Reading EEPROM...");
  EEPROM.begin(EEPROM_SIZE);                      //Reading the ESP32's EEPROM
  pictureNumber = EEPROM.read(0) + 1;             //Setting the pictureNumber according to written value in EEPROM, and increasing it by one

  Serial.println("Saving picture...");
  String path = "/picture" + String(pictureNumber) +".jpg";

  fs::FS &fs = SD_MMC; 
  Serial.printf("Picture file name: %s\n", path.c_str());
  File file = fs.open(path.c_str(), FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file in writing mode");
  } 
  else {
    file.write(fb->buf, fb->len);                 // Image data and its length
    Serial.printf("Saved file to path: %s\n", path.c_str());
    EEPROM.write(0, pictureNumber);               //Writing latest picture number into ESP32's EEPROM
    EEPROM.commit();
  }
  file.close();
  esp_camera_fb_return(fb); 
  delay (2000);

///////////////////////
  Serial.printf("Going into the loop now!");
}


//I also made a little thing in the loop where it takes photo each 10 seconds. 
void loop() {
  
  //Loop is relatively simple. It does the same things as we did in setup function. Since in setup we defined everything, here we just take the photo, read EEPROM,
  //open filesystem on SD card, save the picture, save new value in EEPROM, and wait for 10 seconds. Then loop completes and starts over.
  //NOTE: I didn't test what happens when you reach maximum number that you can store in one byte of EEPROM (255)
  
  Serial.println("Capturing photo in a loop...");
  camera_fb_t  * fb = esp_camera_fb_get();
  if(!fb) {
    Serial.println("Loop: Camera capture failed");
    return;
  }
  Serial.println("Loop: Photo has been taken");
  Serial.println("Loop: Reading EEPROM...");
  EEPROM.begin(EEPROM_SIZE);
  pictureNumber = EEPROM.read(0) + 1;

  Serial.println("Loop: Saving picture...");
  String path = "/Loop_picture" + String(pictureNumber) +".jpg";

  fs::FS &fs = SD_MMC; 
  Serial.printf("Loop: Picture file name: %s\n", path.c_str());
  File file = fs.open(path.c_str(), FILE_WRITE);
  if(!file){
    Serial.println("Loop: Failed to open file in writing mode");
  } 
  else {
    file.write(fb->buf, fb->len); // payload (image), payload length
    Serial.printf("Loop: Saved file to path: %s\n", path.c_str());
    EEPROM.write(0, pictureNumber);
    EEPROM.commit();
  }
  file.close();
  esp_camera_fb_return(fb); 
  Serial.printf("10 second wait before restarting the loop.");
  delay (10000);
  
}
