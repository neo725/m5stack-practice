#include <Arduino.h>
#include <M5Stack.h>
#include <Wire.h>

#include "EEPROM.h"
#include "SimpleBeacon.h"
#include "SerialCommand.h"
#include "esp_system.h"

SimpleBeacon ble;

SerialCommand sCmd;

#define TFT_GREY 0x5AEB

#define TFT_W 320
#define TFT_H 240

#define EEPROM_SIZE 64

unsigned int sys_menucolor;
unsigned int sys_windowcolor;
unsigned int sys_menutextcolor;

byte menuidx = 0;
byte menuidxmin = 0;
byte menuidxmax = 3;
byte menulock = 0;
boolean menuIsMenu = HIGH;

unsigned long tmp_tmr = 0;

void setup()
{
    Serial.begin(115200);

    if (!EEPROM.begin(EEPROM_SIZE))
    {
        Serial.println("failed to initialise EEPROM");
        while (HIGH)
            ;
    }

    M5.begin();
    Wire.begin();

    M5.lcd.setBrightness(byte(EEPROM.read(0)));

    sys_menutextcolor = TFT_WHITE;
    sys_menucolor = setrgb(0, 0, 128);
    sys_windowcolor = TFT_GREY;

    menuUpdate(menuidx, menulock);

    // sCmd.addCommand("wifiscan",    rcmdWiFiScan);
    // sCmd.addCommand("i2cscan",    rcmdIICScan);
    // sCmd.addCommand("eddystoneurl",    rcmdEddystoneURL);
    // sCmd.addCommand("eddystonetlm",    rcmdEddystoneTLM);
    sCmd.addCommand("ibeacon", rcmdIBeacon);
    sCmd.addCommand("sleep", rcmdSleep);
    sCmd.addCommand("bright", rcmdBright);
    sCmd.addCommand("clr", rcmdClr);
    // sCmd.addCommand("qrc",    rcmdQRC);
    sCmd.setDefaultHandler(rcmdDef);
}

void loop()
{
    sCmd.readSerial();
    if (M5.BtnA.wasPressed())
    {
        if (menuidx > menuidxmin)
            menuidx--;
        menuUpdate(menuidx, menulock);
    }
    if (M5.BtnC.wasPressed())
    {
        if (menuidx < menuidxmax)
            menuidx++;
        menuUpdate(menuidx, menulock);
    }
    if (M5.BtnB.wasPressed())
    {
        if (menuIsMenu)
        {
            menulock = menuidx;
            menuidx = 0;
            menuUpdate(menuidx, menulock);
        }
        else
        {
            menuRunApp(menuidx, menulock);
        }
    }
    M5.update();
}

int square(int x)
{ // function declared and implemented
    return x * x;
}

String getSTAMac()
{
    uint8_t baseMac[6];
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    char baseMacChr[18] = {0};
    sprintf(baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
    return String(baseMacChr);
}

void rcmdDef(const char *command)
{
    Serial.println(F("ERROR"));
}

void rcmdClr()
{
    menuidx = 0;
    menulock = 0;
    M5.Lcd.fillScreen(TFT_BLACK);
    menuUpdate(menuidx, menulock);
}

void rcmdSleep()
{
    M5.setWakeupButton(BUTTON_B_PIN);
    M5.powerOFF();
}

void rcmdIBeacon()
{
    int aNumber;
    char *arg;

    unsigned int tmpmajor;
    unsigned int tmpminor;
    byte tmppwr;

    arg = sCmd.next();
    if (arg != NULL)
    {
        tmpmajor = atoi(arg);
        if (tmpmajor >= 0 and tmpmajor <= 65535)
        {
            arg = sCmd.next();
            if (arg != NULL)
            {
                tmpminor = atoi(arg);
                if (tmpminor >= 0 and tmpminor <= 65535)
                {
                    arg = sCmd.next();
                    if (arg != NULL)
                    {
                        tmppwr = atoi(arg);
                        if (tmppwr >= 0 and tmppwr <= 255)
                        {
                            ble.iBeacon(tmpmajor, tmpminor, tmppwr);
                            delay(100);
                            ble.end();
                            Serial.println(F("OK"));
                        }
                    }
                }
            }
        }
    }
}

void rcmdBright()
{
    int aNumber;
    char *arg;
    unsigned int tmpval;

    arg = sCmd.next();
    if (arg != NULL)
    {
        tmpval = atoi(arg);
        if (tmpval >= 0 and tmpval <= 255)
        {
            EEPROM.write(0, tmpval);
            EEPROM.commit();
            M5.lcd.setBrightness(byte(EEPROM.read(0)));
        }
    }
}

unsigned int setrgb(byte inred, byte ingrn, byte inblue)
{
    inred = map(inred, 0, 255, 0, 31);
    ingrn = map(ingrn, 0, 255, 0, 31);
    inblue = map(inblue, 0, 255, 0, 31);
    return inred << 11 | ingrn << 5 | inblue;
}

void menuUpdate(byte inmenuidx, byte inmenulock)
{
    switch (inmenulock)
    {
    case 0:
        menuTopLevel(inmenuidx);
        break;
    case 1:
        menuAplikaceLevel(inmenuidx);
        break;
    case 2:
        menuSystemLevel(inmenuidx);
        break;
    case 3:
        menuNastaveniLevel(inmenuidx);
        break;
    default:
        Serial.println();
    }
}

void appSysInfo()
{
    menuDrawMenu(F("M5 SYSTEM INFO"), F(""), F("ESC"), F(""), sys_menucolor, sys_windowcolor, sys_menutextcolor);
    menuidx = 2;
    menulock = 0;
    while (M5.BtnB.wasPressed())
    {
        M5.update();
    }
    uint8_t chipRev = ESP.getChipRevision();
    uint8_t cpuSpeed = ESP.getCpuFreqMHz();
    uint32_t flashSize = ESP.getFlashChipSize();
    uint32_t flashSpeed = ESP.getFlashChipSpeed();
    const char *sdkVer = ESP.getSdkVersion();
    String wifiSTAMAC = getSTAMac();

    M5.Lcd.drawString(F("CPU_FREQ:"), 10, 40, 2);
    M5.Lcd.drawNumber(cpuSpeed, 120, 40, 2);

    M5.Lcd.drawString(F("CHIP_REV:"), 10, 60, 2);
    M5.Lcd.drawNumber(chipRev, 120, 60, 2);

    M5.Lcd.drawString(F("FLASH_SIZE:"), 10, 80, 2);
    M5.Lcd.drawNumber(flashSize, 120, 80, 2);

    M5.Lcd.drawString(F("FLASH_SPEED:"), 10, 100, 2);
    M5.Lcd.drawNumber(flashSpeed, 120, 100, 2);

    M5.Lcd.drawString(F("SDK_VERSION:"), 10, 120, 2);
    M5.Lcd.drawString(sdkVer, 120, 120, 2);

    M5.Lcd.drawString(F("WIFI_STA_MAC:"), 10, 140, 2);
    M5.Lcd.drawString(wifiSTAMAC, 120, 140, 2);

    while (!M5.BtnB.wasPressed())
    {
        M5.update();
    }
    menuUpdate(menuidx, menulock);
}

void appBLEBaecon()
{
    byte beaconIdx = 0;
    menuDrawMenu(F("BLE BEACON SIMULATOR"), F(""), F("ESC"), F(""), sys_menucolor, sys_windowcolor, sys_menutextcolor);
    M5.Lcd.drawCentreString(F("RUNNING"), TFT_W / 2, TFT_H / 2, 2);
    menuidx = 1;
    menulock = 0;
    while (M5.BtnB.wasPressed())
    {
        M5.update();
    }
    while (!M5.BtnB.wasPressed())
    {
        M5.update();
        if (millis() - tmp_tmr > 300)
        {
            tmp_tmr = millis();
            if (beaconIdx == 4)
            {
                beaconIdx = 0;
            }

            // 強制模式
            beaconIdx = 2;

            if (beaconIdx == 0)
            {
                ble.iBeacon(10, 20, 50);
            }
            if (beaconIdx == 1)
            {
                ble.EddystoneTLM(2048, 512, 100, 1024);
            }
            if (beaconIdx == 2)
            {
                // eddystone url max length : 17 characters
                ble.EddystoneURIPlain(1, "goo.gl/2Fd02", 1);
            }
            if (beaconIdx == 3)
            {
                ble.AltBeacon();
            }
            beaconIdx++;
        }
    }
    ble.end();
    menuUpdate(menuidx, menulock);
}

void appSerialBridge(byte inSpdIdx, boolean inRun)
{
    HardwareSerial Serial2(2);
    byte spdIdx = 0;
    int8_t spdLast = -1;
    unsigned long spdCfg = 9600;
    boolean spRun = LOW;
    boolean spLast = LOW;
    spdIdx = inSpdIdx;
    spRun = inRun;
    menuDrawMenu(F("SERIAL BRIDGE"), F("SPD"), F("ESC"), F("ON/OFF"), sys_menucolor, sys_windowcolor, sys_menutextcolor);
    M5.Lcd.drawCentreString(F("PORT 0"), TFT_W / 2, 40, 2);
    menuidx = 1;
    menulock = 0;
    while (M5.BtnB.wasPressed())
    {
        M5.update();
    }
    while (!M5.BtnB.wasPressed())
    {
        M5.update();
        if (spdIdx > 3)
        {
            spdIdx = 0;
        }
        if (spdLast != spdIdx)
        {
            spdLast = spdIdx;
            if (spdLast == 0)
            {
                menuWindowClr(sys_windowcolor);
                spdCfg = 9600;
                M5.Lcd.drawCentreString(F("UART 2"), TFT_W / 2, 40, 2);
                M5.Lcd.drawString(F("SPEED"), 20, 100, 2);
                M5.Lcd.drawNumber(spdCfg, 100, 80, 6);
                M5.Lcd.drawString(F("STATE"), 20, 160, 2);
                M5.Lcd.drawNumber(spLast, 100, 140, 6);
            }
            if (spdLast == 1)
            {
                menuWindowClr(sys_windowcolor);
                spdCfg = 19200;
                M5.Lcd.drawCentreString(F("UART 2"), TFT_W / 2, 40, 2);
                M5.Lcd.drawString(F("SPEED"), 20, 100, 2);
                M5.Lcd.drawNumber(spdCfg, 100, 80, 6);
                M5.Lcd.drawString(F("STATE"), 20, 160, 2);
                M5.Lcd.drawNumber(spLast, 100, 140, 6);
            }
            if (spdLast == 2)
            {
                menuWindowClr(sys_windowcolor);
                spdCfg = 57600;
                M5.Lcd.drawCentreString(F("UART 2"), TFT_W / 2, 40, 2);
                M5.Lcd.drawString(F("SPEED"), 20, 100, 2);
                M5.Lcd.drawNumber(spdCfg, 100, 80, 6);
                M5.Lcd.drawString(F("STATE"), 20, 160, 2);
                M5.Lcd.drawNumber(spLast, 100, 140, 6);
            }
            if (spdLast == 3)
            {
                menuWindowClr(sys_windowcolor);
                spdCfg = 115200;
                M5.Lcd.drawCentreString(F("UART 2"), TFT_W / 2, 40, 2);
                M5.Lcd.drawString(F("SPEED"), 20, 100, 2);
                M5.Lcd.drawNumber(spdCfg, 100, 80, 6);
                M5.Lcd.drawString(F("STATE"), 20, 160, 2);
                M5.Lcd.drawNumber(spLast, 100, 140, 6);
            }
        }
        if (M5.BtnA.wasPressed())
        {
            spdIdx++;
        }
        if (M5.BtnC.wasPressed())
        {
            if (spRun)
            {
                spRun = LOW;
            }
            else
            {
                spRun = HIGH;
            }
        }
        if (spRun != spLast)
        {
            spLast = spRun;
            menuWindowClr(sys_windowcolor);
            M5.Lcd.drawCentreString(F("UART 2"), TFT_W / 2, 40, 2);
            M5.Lcd.drawString(F("SPEED"), 20, 100, 2);
            M5.Lcd.drawNumber(spdCfg, 100, 80, 6);
            M5.Lcd.drawString(F("STATE"), 20, 160, 2);
            M5.Lcd.drawNumber(spLast, 100, 140, 6);
            if (spLast)
            {
                Serial.begin(spdCfg);
                Serial2.begin(spdCfg);
                delay(100);
            }
            else
            {
                Serial.end();
                Serial2.end();
                delay(100);
            }
        }
        if (spLast)
        {
            if (Serial.available())
            {
                Serial2.write(Serial.read());
            }

            if (Serial2.available())
            {
                Serial.write(Serial2.read());
            }
        }
    }
    Serial.end();
    Serial2.end();
    Serial.begin(115200);
    menuUpdate(menuidx, menulock);
}

void appCfgBrigthness()
{
    menuidx = 3;
    menulock = 0;
    byte tmp_brigth = byte(EEPROM.read(0));
    byte tmp_lbrigth = 0;

    menuDrawMenu(F("DISPLAY BRIGHTNESS"), F("-"), F("OK"), F("+"), sys_menucolor, sys_windowcolor, sys_menutextcolor);
    M5.Lcd.setTextColor(sys_menutextcolor, sys_windowcolor);

    while (M5.BtnB.wasPressed())
    {
        M5.update();
    }

    while (!M5.BtnB.wasPressed())
    {
        if (M5.BtnA.wasPressed() and tmp_brigth >= 16)
        {
            tmp_brigth = tmp_brigth - 16;
        }
        if (M5.BtnC.wasPressed() and tmp_brigth <= 239)
        {
            tmp_brigth = tmp_brigth + 16;
        }
        if (tmp_lbrigth != tmp_brigth)
        {
            tmp_lbrigth = tmp_brigth;
            EEPROM.write(0, tmp_lbrigth);
            EEPROM.commit();
            M5.lcd.setBrightness(byte(EEPROM.read(0)));
            menuDrawMenu(F("DISPLAY BRIGHTNESS"), F("-"), F("OK"), F("+"), sys_menucolor, sys_windowcolor, sys_menutextcolor);
            M5.Lcd.setTextColor(sys_menutextcolor, sys_windowcolor);
            M5.Lcd.drawNumber(byte(EEPROM.read(0)), 120, 90, 6);
        }
        M5.update();
    }

    menuUpdate(menuidx, menulock);
}

void menuRunApp(byte inmenuidx, byte inmenulock)
{
    if (inmenulock == 2 and inmenuidx == 0)
    {
        M5.setWakeupButton(BUTTON_B_PIN);
        M5.powerOFF();
    }
    if (inmenulock == 2 and inmenuidx == 1)
    {
        appSysInfo();
    }
    if (inmenulock == 2 and inmenuidx == 2)
    {
        menuidx = 2;
        menulock = 0;
        menuUpdate(menuidx, menulock);
    }

    if (inmenulock == 3 and inmenuidx == 0)
    {
        appCfgBrigthness();
    }
    if (inmenulock == 3 and inmenuidx == 1)
    {
        menuidx = 3;
        menulock = 0;
        menuUpdate(menuidx, menulock);
    }

    //   if(inmenulock==1 and inmenuidx==0){
    //     appDHT12();
    //   }
    //   if(inmenulock==1 and inmenuidx==1){
    //     appStopky();
    //   }
    //   if(inmenulock==1 and inmenuidx==2){
    //     appIICScanner();
    //   }
    if (inmenulock == 1 and inmenuidx == 3)
    {
        appBLEBaecon();
    }
    //   if(inmenulock==1 and inmenuidx==4){
    //     appQRPrint();
    //   }
    if (inmenulock == 1 and inmenuidx == 5)
    {
        appSerialBridge(0, LOW);
    }
    //   if(inmenulock==1 and inmenuidx==6){
    //     appGY521();
    //   }
    //   if(inmenulock==1 and inmenuidx==7){
    //     appWiFiScan();
    //   }
    if (inmenulock == 1 and inmenuidx == 8)
    {
        menuidx = 1;
        menulock = 0;
        menuUpdate(menuidx, menulock);
    }
}

void windowPrintInfo(unsigned int intextcolor)
{
    M5.Lcd.setTextColor(intextcolor);
    M5.Lcd.drawCentreString(F("M5Stack @ 2018"), TFT_W / 2, TFT_H / 2, 2);
}

void windowPrintInfoText(String intext, unsigned int intextcolor)
{
    M5.Lcd.setTextColor(intextcolor);
    M5.Lcd.drawCentreString(intext, TFT_W / 2, TFT_H / 2, 2);
}

void menuWindowClr(unsigned int incolor)
{
    M5.Lcd.fillRoundRect(0, 32, TFT_W, TFT_H - 32 - 32, 3, incolor);
}

void menuDrawMenu(String inmenutxt, String inbtnatxt, String inbtnbtxt, String inbtnctxt, unsigned int inmenucolor, unsigned int inwindowcolor, unsigned int intxtcolor)
{
    M5.Lcd.fillRoundRect(31, TFT_H - 28, 60, 28, 3, inmenucolor);
    M5.Lcd.fillRoundRect(126, TFT_H - 28, 60, 28, 3, inmenucolor);
    M5.Lcd.fillRoundRect(221, TFT_H - 28, 60, 28, 3, inmenucolor);
    M5.Lcd.fillRoundRect(0, 0, TFT_W, 28, 3, inmenucolor);
    M5.Lcd.fillRoundRect(0, 32, TFT_W, TFT_H - 32 - 32, 3, inwindowcolor);

    M5.Lcd.setTextColor(intxtcolor);
    M5.Lcd.drawCentreString(inmenutxt, TFT_W / 2, 6, 2);

    M5.Lcd.drawCentreString(inbtnatxt, 31 + 30, TFT_H - 28 + 6, 2);
    M5.Lcd.drawCentreString(inbtnbtxt, 126 + 30, TFT_H - 28 + 6, 2);
    M5.Lcd.drawCentreString(inbtnctxt, 221 + 30, TFT_H - 28 + 6, 2);
}

void menuTopLevel(byte inmenuidx)
{
    menuidxmin = 0;
    menuidxmax = 3;
    switch (inmenuidx)
    {
    case 0:
        menuIsMenu = HIGH;
        menuDrawMenu(F("MAIN MENU"), F("<"), F("OK"), F(">"), sys_menucolor, sys_windowcolor, sys_menutextcolor);
        windowPrintInfo(sys_menutextcolor);
        break;
    case 1:
        menuIsMenu = HIGH;
        menuDrawMenu(F("APPLICATIONS"), F("<"), F("OK"), F(">"), sys_menucolor, sys_windowcolor, sys_menutextcolor);
        break;
    case 2:
        menuIsMenu = HIGH;
        menuDrawMenu(F("SYSTEM"), F("<"), F("OK"), F(">"), sys_menucolor, sys_windowcolor, sys_menutextcolor);
        break;
    case 3:
        menuIsMenu = HIGH;
        menuDrawMenu(F("CONFIGURATION"), F("<"), F("OK"), F(">"), sys_menucolor, sys_windowcolor, sys_menutextcolor);
        break;
    default:
        Serial.println();
    }
}

void menuNastaveniLevel(byte inmenuidx)
{
    menuidxmin = 0;
    menuidxmax = 1;
    switch (inmenuidx)
    {
    case 0:
        menuIsMenu = LOW;
        menuWindowClr(sys_windowcolor);
        windowPrintInfoText(F("DISPLAY - BRIGHTNESS"), sys_menutextcolor);
        break;
    case 1:
        menuIsMenu = LOW;
        menuWindowClr(sys_windowcolor);
        windowPrintInfoText(F("RETURN"), sys_menutextcolor);
        break;
    default:
        Serial.println();
    }
}

void menuSystemLevel(byte inmenuidx)
{
    menuidxmin = 0;
    menuidxmax = 2;
    switch (inmenuidx)
    {
    case 0:
        menuIsMenu = LOW;
        menuWindowClr(sys_windowcolor);
        windowPrintInfoText(F("SLEEP/CHARGING"), sys_menutextcolor);
        break;
    case 1:
        menuIsMenu = LOW;
        menuWindowClr(sys_windowcolor);
        windowPrintInfoText(F("M5 SYSTEM INFO"), sys_menutextcolor);
        break;
    case 2:
        menuIsMenu = LOW;
        menuWindowClr(sys_windowcolor);
        windowPrintInfoText(F("RETURN"), sys_menutextcolor);
        break;
    default:
        Serial.println();
    }
}

void menuAplikaceLevel(byte inmenuidx)
{
    menuidxmin = 0;
    menuidxmax = 8;
    switch (inmenuidx)
    {
    case 0:
        menuIsMenu = LOW;
        menuWindowClr(sys_windowcolor);
        windowPrintInfoText(F("DHT12"), sys_menutextcolor);
        break;
    case 1:
        menuIsMenu = LOW;
        menuWindowClr(sys_windowcolor);
        windowPrintInfoText(F("TIME MEASUREMENT"), sys_menutextcolor);
        break;
    case 2:
        menuIsMenu = LOW;
        menuWindowClr(sys_windowcolor);
        windowPrintInfoText(F("I2C Scanner"), sys_menutextcolor);
        break;
    case 3:
        menuIsMenu = LOW;
        menuWindowClr(sys_windowcolor);
        windowPrintInfoText(F("BLE Beacon SIMULATOR"), sys_menutextcolor);
        break;
    case 4:
        menuIsMenu = LOW;
        menuWindowClr(sys_windowcolor);
        windowPrintInfoText(F("QR CODE"), sys_menutextcolor);
        break;
    case 5:
        menuIsMenu = LOW;
        menuWindowClr(sys_windowcolor);
        windowPrintInfoText(F("SERIAL BRIDGE"), sys_menutextcolor);
        break;
    case 6:
        menuIsMenu = LOW;
        menuWindowClr(sys_windowcolor);
        windowPrintInfoText(F("GY-521"), sys_menutextcolor);
        break;
    case 7:
        menuIsMenu = LOW;
        menuWindowClr(sys_windowcolor);
        windowPrintInfoText(F("WiFi SCANNER"), sys_menutextcolor);
        break;
    case 8:
        menuIsMenu = LOW;
        menuWindowClr(sys_windowcolor);
        windowPrintInfoText(F("RETURN"), sys_menutextcolor);
        break;
    default:
        Serial.println();
    }
}
