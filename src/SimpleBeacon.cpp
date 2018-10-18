#include "SimpleBeacon.h"
#include "esp32-hal-log.h"

/*  HCI Command opcode group field(OGF) */
#define HCI_GRP_HOST_CONT_BASEBAND_CMDS    (0x03 << 10)            /* 0x0C00 */
#define HCI_GRP_BLE_CMDS                   (0x08 << 10)

/*  HCI Command opcode command field(OCF) */
#define HCI_RESET                          (0x0003 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_BLE_WRITE_ADV_ENABLE           (0x000A | HCI_GRP_BLE_CMDS)
#define HCI_BLE_WRITE_ADV_PARAMS           (0x0006 | HCI_GRP_BLE_CMDS)
#define HCI_BLE_WRITE_ADV_DATA             (0x0008 | HCI_GRP_BLE_CMDS)

#define HCI_H4_CMD_PREAMBLE_SIZE                (4)
#define HCIC_PARAM_SIZE_WRITE_ADV_ENABLE        (1)
#define HCIC_PARAM_SIZE_BLE_WRITE_ADV_PARAMS    (15)
#define HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA      (31)

/* EIR/AD data type definitions */
#define BT_DATA_FLAGS               0x01 /* AD flags */
#define BT_DATA_UUID16_SOME         0x02 /* 16-bit UUID, more available */
#define BT_DATA_UUID16_ALL          0x03 /* 16-bit UUID, all listed */
#define BT_DATA_UUID32_SOME         0x04 /* 32-bit UUID, more available */
#define BT_DATA_UUID32_ALL          0x05 /* 32-bit UUID, all listed */
#define BT_DATA_UUID128_SOME        0x06 /* 128-bit UUID, more available */
#define BT_DATA_UUID128_ALL         0x07 /* 128-bit UUID, all listed */
#define BT_DATA_NAME_SHORTENED      0x08 /* Shortened name */
#define BT_DATA_NAME_COMPLETE       0x09 /* Complete name */
#define BT_DATA_TX_POWER            0x0a /* Tx Power */
#define BT_DATA_SOLICIT16           0x14 /* Solicit UUIDs, 16-bit */
#define BT_DATA_SOLICIT128          0x15 /* Solicit UUIDs, 128-bit */
#define BT_DATA_SVC_DATA16          0x16 /* Service data, 16-bit UUID */
#define BT_DATA_GAP_APPEARANCE      0x19 /* GAP appearance */
#define BT_DATA_SOLICIT32           0x1f /* Solicit UUIDs, 32-bit */
#define BT_DATA_SVC_DATA32          0x20 /* Service data, 32-bit UUID */
#define BT_DATA_SVC_DATA128         0x21 /* Service data, 128-bit UUID */
#define BT_DATA_MANUFACTURER_DATA   0xff /* Manufacturer Specific Data */


/* Advertising types */
#define     BLE_GAP_ADV_TYPE_ADV_IND         0x00
#define     BLE_GAP_ADV_TYPE_ADV_DIRECT_IND  0x01
#define     BLE_GAP_ADV_TYPE_ADV_SCAN_IND    0x02
#define     BLE_GAP_ADV_TYPE_ADV_NONCONN_IND 0x03


/* Advertising Discovery Flags */
#define     BLE_GAP_ADV_FLAG_LE_LIMITED_DISC_MODE       (0x01)
#define     BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE       (0x02)
#define     BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED       (0x04)
#define     BLE_GAP_ADV_FLAG_LE_BR_EDR_CONTROLLER       (0x08)
#define     BLE_GAP_ADV_FLAG_LE_BR_EDR_HOST             (0x10)
#define     BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE (BLE_GAP_ADV_FLAG_LE_LIMITED_DISC_MODE | BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED)
#define     BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE (BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE | BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED)


/* Advertising Filter Policies */
#define     BLE_GAP_ADV_FP_ANY              0x00
#define     BLE_GAP_ADV_FP_FILTER_SCANREQ   0x01
#define     BLE_GAP_ADV_FP_FILTER_CONNREQ   0x02
#define     BLE_GAP_ADV_FP_FILTER_BOTH      0x03


/* Advertising Device Address Types */
#define     BLE_GAP_ADDR_TYPE_PUBLIC                          0x00
#define     BLE_GAP_ADDR_TYPE_RANDOM_STATIC                   0x01
#define     BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE       0x02
#define     BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_NON_RESOLVABLE   0x03


/* GAP Advertising Channel Maps */
#define     GAP_ADVCHAN_37          0x01
#define     GAP_ADVCHAN_38          0x02
#define     GAP_ADVCHAN_39          0x04
#define     GAP_ADVCHAN_ALL         GAP_ADVCHAN_37 | GAP_ADVCHAN_38 | GAP_ADVCHAN_39


/* GAP Filter Policies */
#define     BLE_GAP_ADV_FP_ANY              0x00
#define     BLE_GAP_ADV_FP_FILTER_SCANREQ   0x01
#define     BLE_GAP_ADV_FP_FILTER_CONNREQ   0x02
#define     BLE_GAP_ADV_FP_FILTER_BOTH      0x03

#define BD_ADDR_LEN     (6)                     /* Device address length */


/*
 * BLE System
 *
 * */

/* HCI H4 message type definitions */

enum {
    H4_TYPE_COMMAND = 1,
    H4_TYPE_ACL     = 2,
    H4_TYPE_SCO     = 3,
    H4_TYPE_EVENT   = 4
};

volatile bool _vhci_host_send_available = false;
volatile bool _vhci_host_command_running = false;
static uint16_t _vhci_host_command = 0x0000;
static uint8_t _vhci_host_command_result = 0x00;

//controller is ready to receive command
static void _on_tx_ready(void)
{
    _vhci_host_send_available = true;
}
/*
static void _dump_buf(const char * txt, uint8_t *data, uint16_t len){
    log_printf("%s[%u]:", txt, len);
    for (uint16_t i=0; i<len; i++)
        log_printf(" %02x", data[i]);
    log_printf("\n");
}
*/
//controller has a packet
static int _on_rx_data(uint8_t *data, uint16_t len)
{
    if(len == 7 && *data == 0x04){
        //baseband response
        uint16_t cmd = (((uint16_t)data[5] << 8) | data[4]);
        uint8_t res = data[6];
        if(_vhci_host_command_running && _vhci_host_command == cmd){
            //_dump_buf("BLE: res", data, len);
            _vhci_host_command_result = res;
            _vhci_host_command_running = false;
            return 0;
        } else if(cmd == 0){
            log_e("error %u", res);
        }
    }

    //_dump_buf("BLE: rx", data, len);
    return 0;
}




static esp_vhci_host_callback_t vhci_host_cb = {
        _on_tx_ready,
        _on_rx_data
};

static bool _esp_ble_start()
{
    if(btStart()){
        esp_vhci_host_register_callback(&vhci_host_cb);
        uint8_t i = 0;
        while(!esp_vhci_host_check_send_available() && i++ < 100){
            delay(10);
        }
        if(i >= 100){
            log_e("esp_vhci_host_check_send_available failed");
            return false;
        }
        _vhci_host_send_available = true;
    } else 
        log_e("BT Failed");
    return true;
}

static bool _esp_ble_stop()
{
    if(btStarted()){
        _vhci_host_send_available = false;
        btStop();
        esp_vhci_host_register_callback(NULL);
    }
    return true;
}

//public

static uint8_t ble_send_cmd(uint16_t cmd, uint8_t * data, uint8_t len){
    static uint8_t buf[36];
    if(len > 32){
        //too much data
        return 2;
    }
    uint16_t i = 0;
    while(!_vhci_host_send_available && i++ < 1000){
        delay(1);
    }
    if(i >= 1000){
        log_e("_vhci_host_send_available failed");
        return 1;
    }
    uint8_t outlen = len + HCI_H4_CMD_PREAMBLE_SIZE;
    buf[0] = H4_TYPE_COMMAND;
    buf[1] = (uint8_t)(cmd & 0xFF);
    buf[2] = (uint8_t)(cmd >> 8);
    buf[3] = len;
    if(len){
        memcpy(buf+4, data, len);
    }
    _vhci_host_send_available = false;
    _vhci_host_command_running = true;
    _vhci_host_command = cmd;

    //log_printf("BLE: cmd: 0x%04X, data[%u]:", cmd, len);
    //for (uint16_t i=0; i<len; i++) log_printf(" %02x", buf[i+4]);
    //log_printf("\n");
    
    esp_vhci_host_send_packet(buf, outlen);
    while(_vhci_host_command_running);
    int res = _vhci_host_command_result;
    //log_printf("BLE: cmd: 0x%04X, res: %u\n", cmd, res);
    return res;
}


/*
 * BLE Arduino
 *
 * */

enum {
    UNIT_0_625_MS = 625,  /* Number of microseconds in 0.625 milliseconds. */
    UNIT_1_25_MS = 1250,  /* Number of microseconds in 1.25 milliseconds. */
    UNIT_10_MS = 10000  /* Number of microseconds in 10 milliseconds. */
};

/* BLE Advertising parameters struct */
typedef struct ble_gap_adv_params_s {
        uint8_t type;
        uint8_t own_addr_type;
        uint8_t addr_type;
        uint8_t addr[BD_ADDR_LEN];
        uint8_t fp;  // filter policy
        uint16_t interval_min;  // minimum advertising interval between 0x0020 and 0x4000 in 0.625 ms units (20ms to 10.24s)
        uint16_t interval_max;
        uint8_t chn_map;
} ble_adv_params_t;

#define MSEC_TO_UNITS(TIME, RESOLUTION)  (((TIME) * 1000) / (RESOLUTION))
#define UINT16_TO_STREAM(p, u16)         {*(p)++ = (uint8_t)(u16); *(p)++ = (uint8_t)((u16) >> 8);}
#define UINT8_TO_STREAM(p, u8)           {*(p)++ = (uint8_t)(u8);}
#define BDADDR_TO_STREAM(p, a)           {int i; for (i = 0; i < BD_ADDR_LEN;  i++) *(p)++ = (uint8_t) a[BD_ADDR_LEN - 1 - i];}
#define ARRAY_TO_STREAM(p, a, len)       {int i; for (i = 0; i < len;          i++) *(p)++ = (uint8_t) a[i];}

SimpleBeacon::SimpleBeacon()
{
    uint8_t peerAddr[BD_ADDR_LEN] = {0x80, 0x81, 0x82, 0x83, 0x84, 0x85};
    _ble_adv_param = (ble_adv_params_t*)malloc(sizeof(ble_adv_params_t));
    memset(_ble_adv_param, 0x00, sizeof(ble_adv_params_t));
    _ble_adv_param->type = BLE_GAP_ADV_TYPE_ADV_NONCONN_IND;//not connectable
    _ble_adv_param->chn_map = GAP_ADVCHAN_ALL; // 37, 38, 39 channels
    _ble_adv_param->fp = 0;//any
    _ble_adv_param->interval_min = 512;
    _ble_adv_param->interval_max = 1024;
    _ble_adv_param->addr_type = 0;//public
    memcpy(_ble_adv_param->addr, peerAddr, BD_ADDR_LEN);
    local_name = "esp32";
}

SimpleBeacon::~SimpleBeacon(void)
{
    free(_ble_adv_param);
    _esp_ble_stop();
}

bool SimpleBeacon::iBeacon(uint16_t inMajor, uint16_t inMinor, uint8_t inPWR)
{
    if(!_esp_ble_start()){
        return false;
    }
    ble_send_cmd(HCI_RESET, NULL, 0);

    _ble_send_adv_param();
    _ble_send_ibeacon(inMajor, inMinor, inPWR);

    uint8_t adv_enable = 1;
    ble_send_cmd(HCI_BLE_WRITE_ADV_ENABLE, &adv_enable, HCIC_PARAM_SIZE_WRITE_ADV_ENABLE);
    return true;
}

bool SimpleBeacon::AltBeacon(void)
{
    if(!_esp_ble_start()){
        return false;
    }
    ble_send_cmd(HCI_RESET, NULL, 0);

    _ble_send_adv_param();
    _ble_send_AltBeacon();

    uint8_t adv_enable = 1;
    ble_send_cmd(HCI_BLE_WRITE_ADV_ENABLE, &adv_enable, HCIC_PARAM_SIZE_WRITE_ADV_ENABLE);
    return true;
}


bool SimpleBeacon::EddystoneTLM(uint16_t inVoltage, uint16_t inTemp, uint32_t inCount, uint32_t inTime)
{
    if(!_esp_ble_start()){
        return false;
    }
    ble_send_cmd(HCI_RESET, NULL, 0);

    _ble_send_adv_param();
    _ble_send_EddystoneTLM(inVoltage, inTemp, inCount, inTime);

    uint8_t adv_enable = 1;
    ble_send_cmd(HCI_BLE_WRITE_ADV_ENABLE, &adv_enable, HCIC_PARAM_SIZE_WRITE_ADV_ENABLE);
    return true;
}

bool SimpleBeacon::EddystoneURIPlain(uint8_t inPrefix, String inURI, uint8_t inPWR)
{
    if(!_esp_ble_start()){
        return false;
    }
    ble_send_cmd(HCI_RESET, NULL, 0);

    _ble_send_adv_param();
    _ble_send_EddystoneURIPlain(inPrefix, inURI, inPWR);

    uint8_t adv_enable = 1;
    ble_send_cmd(HCI_BLE_WRITE_ADV_ENABLE, &adv_enable, HCIC_PARAM_SIZE_WRITE_ADV_ENABLE);
    return true;
}

void SimpleBeacon::end()
{
    uint8_t adv_enable = 0;
    ble_send_cmd(HCI_BLE_WRITE_ADV_ENABLE, &adv_enable, HCIC_PARAM_SIZE_WRITE_ADV_ENABLE);
    ble_send_cmd(HCI_RESET, NULL, 0);
    _esp_ble_stop();
}

void SimpleBeacon::_ble_send_adv_param(void)
{
    uint8_t dbuf[HCIC_PARAM_SIZE_BLE_WRITE_ADV_PARAMS];
    uint8_t *buf = dbuf;
    UINT16_TO_STREAM (buf, _ble_adv_param->interval_min);
    UINT16_TO_STREAM (buf, _ble_adv_param->interval_max);
    UINT8_TO_STREAM (buf, _ble_adv_param->type);
    UINT8_TO_STREAM (buf, _ble_adv_param->own_addr_type);
    UINT8_TO_STREAM (buf, _ble_adv_param->addr_type);
    ARRAY_TO_STREAM (buf, _ble_adv_param->addr, BD_ADDR_LEN);
    UINT8_TO_STREAM (buf, _ble_adv_param->chn_map);
    UINT8_TO_STREAM (buf, _ble_adv_param->fp);
    ble_send_cmd(HCI_BLE_WRITE_ADV_PARAMS, dbuf, HCIC_PARAM_SIZE_BLE_WRITE_ADV_PARAMS);
}



void SimpleBeacon::_ble_send_ibeacon(uint16_t inMajor, uint16_t inMinor, uint8_t inPWR)
{
    uint8_t adv_data_len = 32;
    uint8_t adv_data[HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA + 1] = {
        adv_data_len, 0x02, 0x01, 0x06, 0x1A, 0xFF, 0x4C, 0x00, 0x02, 0x15,
        0x00, 0x00, 0x00, 0x00, 0x1A, 0xE9, 0x10, 0x01, 0xB0, 0x00, 0x00, 0x1C, 0x4D, 0x64, 0x65, 0xE3,
        // 00 00 00 00 1A E9 10 01 B0 00 00 1C 4D 64 65 E3
        // 00000000-1ae9-1001-b000-001c4d6465e3
        highByte(inMajor), lowByte(inMajor),
        highByte(inMinor), lowByte(inMinor),
        inPWR
    };
    adv_data[0] = adv_data_len - 1;
    ble_send_cmd(HCI_BLE_WRITE_ADV_DATA, (uint8_t *)adv_data, HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA + 1);
}

void SimpleBeacon::_ble_send_EddystoneTLM(uint16_t inVoltage, uint16_t inTemp, uint32_t inCount, uint32_t inTime)
{
    union {
      unsigned long clval;
      byte cbval[4];
    } countAsBytes;

    union {
      unsigned long tlval;
      byte tbval[4];
    } timeAsBytes;

    timeAsBytes.tlval = inTime;
    countAsBytes.clval = inCount;
    
    uint8_t adv_data_len = 27;
    uint8_t adv_data[HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA + 1] = {
        adv_data_len, 0x02, 0x01, 0x06, 0x03, 0x03, 0xAA, 0xFE, 
        0x11, 0x16, 0xAA, 0xFE, 0x20, 0x00, highByte(inVoltage), lowByte(inVoltage), highByte(inTemp), lowByte(inTemp), countAsBytes.cbval[3], countAsBytes.cbval[2], countAsBytes.cbval[1], countAsBytes.cbval[0], timeAsBytes.tbval[3], timeAsBytes.tbval[2], timeAsBytes.tbval[1], timeAsBytes.tbval[0]
    };
    adv_data[0] = adv_data_len - 1;
    ble_send_cmd(HCI_BLE_WRITE_ADV_DATA, (uint8_t *)adv_data, HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA + 1);
}

void SimpleBeacon::_ble_send_AltBeacon(void)
{
    uint8_t adv_data_len = 32;
    uint8_t adv_data[HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA + 1] = {
        adv_data_len, 0x02, 0x01, 0x06, 0x1B, 0xFF, 0x4C, 0x00, 0xBE, 0xAC,
        0xAA, 0xBB, 0xAA, 0xBB, 0xAA, 0xBB, 0xAA, 0xBB, 0xAA, 0xBB, 0xAA, 0xBB, 0xAA, 0xBB, 0xAA, 0xBB, 0xAA, 0xBB, 0xAA, 0xBB, 0xA0, 0x00
    };
    adv_data[0] = adv_data_len - 1;
    ble_send_cmd(HCI_BLE_WRITE_ADV_DATA, (uint8_t *)adv_data, HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA + 1);
}

void SimpleBeacon::_ble_send_EddystoneURIPlain(uint8_t inPrefix, String inURI, uint8_t inPWR)
{
    byte tmpURI[inURI.length()];
    inURI.getBytes(tmpURI, inURI.length()+1);
    
    byte tmpSize = 6 + sizeof(tmpURI);
    uint8_t adv_data[HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA + 1];
    
    uint8_t adv_data_len = 9 + tmpSize;
    adv_data[0] = adv_data_len - 1;

    adv_data[1] = 0x02;
    adv_data[2] = 0x01;
    adv_data[3] = 0x06;

    adv_data[4] = 0x03;
    adv_data[5] = 0x03;
    adv_data[6] = 0xAA;
    adv_data[7] = 0xFE;

    adv_data[8] = tmpSize;
    adv_data[9] = 0x16;
    adv_data[10] = 0xAA;
    adv_data[11] = 0xFE;
    adv_data[12] = 0x10;
    adv_data[13] = inPWR;
    adv_data[14] = inPrefix;

    for(byte i=0;i<sizeof(tmpURI);i++){
      adv_data[15+i] = tmpURI[i];
    }
            
    ble_send_cmd(HCI_BLE_WRITE_ADV_DATA, (uint8_t *)adv_data, HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA + 1);
        
}
