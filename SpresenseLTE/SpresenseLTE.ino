#include <Arduino.h>
//http://makaizou.blogspot.com/2019/12/spresenseline.html
#include <ArduinoHttpClient.h>
#include <SDHCI.h>
#include <LTE.h>
#include <RTC.h>
#include <Camera.h>
#include <LowPower.h>
#include <Watchdog.h>

// APN data
#define LTE_APN       "povo.jp" // replace your APN
#define LTE_USER_NAME ""   // replace with your username
#define LTE_PASSWORD  ""       // replace with your password

// LINE Token
#define LINE_TOKEN "****************"

// 通知メッセージ（撮影時刻）
#define MESSAGE "PICT %d "
// 区切り用のランダム文字列
#define BOUNDARY "123456789000000000000987654321"
// メッセージヘッダ
#define MESSAGE_HEADER "\r\n--" BOUNDARY "\r\nContent-Disposition: form-data; name=\"message\"\r\n\r\n"
// 画像ヘッダ
#define IMAGE_HEADER "\r\n--" BOUNDARY "\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"image.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n"
// 最後の区切り
#define BOUNDARY_LAST "\r\n--" BOUNDARY "--\r\n"
// URL, path & port 
char server[] = "notify-api.line.me";
char postPath[] = "/api/notify";
int port = 443; // port 443 is the default for HTTPS
int take_picture_count = 0;
#define ROOTCA_FILE "/CERTS/GlobalSign.crt"   // https://notify-api.line.meから取得したcrtファイルを配置

unsigned long sleep_time = 10 * 60;//送信間隔　10分 

// initialize the library instance
LTE lteAccess;
LTETLSClient tlsClient;
HttpClient client = HttpClient(tlsClient, server, port);
SDClass theSD;

// Camera
#define JPEG_QUALITY 10 //少ないほど高画質　デフォルトは７
#define FRAMESIZE_H 1280
#define FRAMESIZE_V 960
/*
#define CAM_IMGSIZE_QQVGA_H   (160)   /**< QQVGA    horizontal size 
#define CAM_IMGSIZE_QQVGA_V   (120)   /**< QQVGA    vertical   size 
#define CAM_IMGSIZE_QVGA_H    (320)   /**< QVGA     horizontal size 
#define CAM_IMGSIZE_QVGA_V    (240)   /**< QVGA     vertical   size 
#define CAM_IMGSIZE_VGA_H     (640)   /**< VGA      horizontal size 
#define CAM_IMGSIZE_VGA_V     (480)   /**< VGA      vertical   size 
#define CAM_IMGSIZE_HD_H      (1280)  /**< HD       horizontal size 
#define CAM_IMGSIZE_HD_V      (720)   /**< HD       vertical   size 
#define CAM_IMGSIZE_QUADVGA_H (1280)  /**< QUADVGA  horizontal size 
#define CAM_IMGSIZE_QUADVGA_V (960)   /**< QUADVGA  vertical   size 
#define CAM_IMGSIZE_FULLHD_H  (1920)  /**< FULLHD   horizontal size 
#define CAM_IMGSIZE_FULLHD_V  (1080)  /**< FULLHD   vertical   size 
#define CAM_IMGSIZE_3M_H      (2048)  /**< 3M       horizontal size 
#define CAM_IMGSIZE_3M_V      (1536)  /**< 3M       vertical   size 
#define CAM_IMGSIZE_5M_H      (2560)  /**< 5M       horizontal size 
#define CAM_IMGSIZE_5M_V      (1920)  /**< 5M       vertical   size 
*/

void printError(enum CamErr err)
{
  Serial.print("Error: ");
  switch (err)
    {
      case CAM_ERR_NO_DEVICE:
        Serial.println("No Device");
        break;
      case CAM_ERR_ILLEGAL_DEVERR:
        Serial.println("Illegal device error");
        break;
      case CAM_ERR_ALREADY_INITIALIZED:
        Serial.println("Already initialized");
        break;
      case CAM_ERR_NOT_INITIALIZED:
        Serial.println("Not initialized");
        break;
      case CAM_ERR_NOT_STILL_INITIALIZED:
        Serial.println("Still picture not initialized");
        break;
      case CAM_ERR_CANT_CREATE_THREAD:
        Serial.println("Failed to create thread");
        break;
      case CAM_ERR_INVALID_PARAM:
        Serial.println("Invalid parameter");
        break;
      case CAM_ERR_NO_MEMORY:
        Serial.println("No memory");
        break;
      case CAM_ERR_USR_INUSED:
        Serial.println("Buffer already in use");
        break;
      case CAM_ERR_NOT_PERMITTED:
        Serial.println("Operation not permitted");
        break;
      default:
        break;
    }
}

void camera_init()
{
  CamErr err;

  /* Initialize SD */
  while (!theSD.begin()) 
    {
      /* wait until SD card is mounted. */
      Serial.println("Insert SD card.");
    }

  /* begin() without parameters means that
   * number of buffers = 1, 30FPS, QVGA, YUV 4:2:2 format */
  Serial.println("Prepare camera");
  err = theCamera.begin();
  if (err != CAM_ERR_SUCCESS)
    {
      printError(err);
    }

  /* Auto white balance configuration */

  Serial.println("Set Auto white balance parameter");
  err = theCamera.setAutoWhiteBalanceMode( CAM_WHITE_BALANCE_AUTO);
  if (err != CAM_ERR_SUCCESS)
    {
      printError(err);
    }
 
  /* Set parameters about still picture.
   * In the following case, QUADVGA and JPEG.
   */

  Serial.println("Set still picture format");
  err = theCamera.setStillPictureImageFormat(
     FRAMESIZE_H ,FRAMESIZE_V ,
     CAM_IMAGE_PIX_FMT_JPG ,JPEG_QUALITY);
  if (err != CAM_ERR_SUCCESS)
    {
      printError(err);
    }
}

void printClock(RtcTime &rtc)
{
  printf("%04d/%02d/%02d %02d:%02d:%02d\n",
        rtc.year(), rtc.month(), rtc.day(),
        rtc.hour(), rtc.minute(), rtc.second());
}

void enterSleep()
{
  lteAccess.detach();
  // Watchdog.stop();
  // Watchdog.end();
  Serial.println("Sleep");
  LowPower.deepSleep(sleep_time);
}
void send_line_notify(CamImage img ,RtcTime &rtc , int picture_count){
  Serial.println("sending Notify");


for (auto t = millis(); millis() - t < 15000 && !client.connect("notify-api.line.me", 443);)
;
if (!client.connected())
  {
    Serial.println("https connect failed.");

  }
  char filename[16];
  sprintf(filename, "%02d%02d%02d-%02d.JPG", rtc.day(), rtc.hour(), rtc.minute() ,picture_count);
  auto messageLength = sizeof(filename);
  auto contentLength = strlen(MESSAGE_HEADER) + messageLength + strlen(IMAGE_HEADER) + img.getImgSize() + strlen(BOUNDARY_LAST);
  client.println("POST /api/notify HTTP/1.0");
  client.println("Authorization: Bearer " LINE_TOKEN);
  client.println("Content-Type: multipart/form-data;boundary=" BOUNDARY);
  client.println("Content-Length: " + String(contentLength));
  client.println();
  Serial.printf("content length: %u\n", contentLength);
  Serial.printf("write message header: %u\n", client.write((uint8_t *)MESSAGE_HEADER, strlen(MESSAGE_HEADER)));
  Serial.printf("write message data: %u\n", client.write((uint8_t *)filename, messageLength));
  Serial.printf("write image header: %u\n", client.write((uint8_t *)IMAGE_HEADER, strlen(IMAGE_HEADER)));
  auto p = img.getImgBuff();
  auto rest = img.getImgSize();
  while (rest > 0 && client.connected())
  {
    auto n = client.write(p, rest > 1460 ? 1460 : rest); //  packet size = 1460に分割して送信
    p += n;
    rest -= n;
  }
  Serial.printf("write image data: %u\n", img.getImgSize() - rest);
  Serial.printf("write last boundary: %u\n", client.write((uint8_t *)BOUNDARY_LAST, strlen(BOUNDARY_LAST)));

  String response;
  while (client.connected() && client.available())
  {
    response =client.readStringUntil('\n') ;
    Serial.print(response + '\n');
  }
  client.stop();
  if (response.indexOf("200") > -1){
    Serial.println("send OK");
    enterSleep();
  }
  Serial.println();
  Serial.println("disconnecting.");
  return;
}

void setup()
{
  // 初期化
  Serial.begin(115200);
  LowPower.begin();
  // Watchdog.begin();
  // Watchdog.start(40000);
  while (!Serial) {
     ; // wait for serial port to connect. Needed for native USB port only
  }

  Serial.println("Starting App");
  pinMode(LED0, OUTPUT);
  camera_init();
  /* Initialize SD */
  while (!theSD.begin()) {
    ; /* wait until SD card is mounted. */
  }
  // SDカードから証明書ファイルを読み込む
  File rootCertsFile = theSD.open(ROOTCA_FILE, FILE_READ);
  tlsClient.setCACert(rootCertsFile, rootCertsFile.available());
  rootCertsFile.close();
    // LTE接続開始
  while (true) {
    if (lteAccess.begin() == LTE_SEARCHING) {
      if (lteAccess.attach(LTE_APN, LTE_USER_NAME, LTE_PASSWORD) == LTE_READY) {
        Serial.println("attach succeeded.");
        break;
      }
      Serial.println("An error occurred, shutdown and try again.");
      lteAccess.shutdown();
      sleep(1);
    }
  }

}



void loop()
{
    sleep(1);
    // Set local time (not UTC) obtained from the network to RTC.
    RTC.begin();
    unsigned long currentTime;
    while(0 == (currentTime = lteAccess.getTime())) {
      sleep(1);
    }
    RtcTime rtc(currentTime);
    printClock(rtc);
    RTC.setTime(rtc);
    Serial.println("call takePicture()");

    CamImage img = theCamera.takePicture();

    /* Check availability of the img instance. */
    /* If any errors occur, the img is not available. */

    if (img.isAvailable())
    {
        /* Create file name */
        digitalWrite(LED0, HIGH);
        // Watchdog.kick();
        send_line_notify(img ,rtc ,take_picture_count);
        // img = theCamera.takePicture();
        digitalWrite(LED0, LOW);
        /* Remove the old file with the same file name as new created file,
        * and create new file.
        */

        // theSD.remove(filename);
        // File myFile = theSD.open(filename, FILE_WRITE);
        // myFile.write(img.getImgBuff(), img.getImgSize());
        // myFile.close();
    }
    else
    {
        /* The size of a picture may exceed the allocated memory size.
        * Then, allocate the larger memory size and/or decrease the size of a picture.
        * [How to allocate the larger memory]
        * - Decrease jpgbufsize_divisor specified by setStillPictureImageFormat()
        * - Increase the Memory size from Arduino IDE tools Menu
        * [How to decrease the size of a picture]
        * - Decrease the JPEG quality by setJPEGQuality()
        */

        Serial.println("Failed to take picture");
        LowPower.reboot();
    }
  Serial.println("send error try again");
  take_picture_count++;
}
