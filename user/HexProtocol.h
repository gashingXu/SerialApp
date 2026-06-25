#ifndef HEXPROTOCOL_H_
#define HEXPROTOCOL_H_

// Hex通讯协议

#include <stdint.h>
#include <stdbool.h>
#include "DataCheck.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    OK_REPLY = 0X01,
    ERR_REPLY = 0X02,
    STATUS_CHECK = 0X03,
    STATUS_REPLY = 0X04,
    ERR_STATUS = 0X05,
    TIME_SET = 0X06,

    MAX_BYTES_CHECK = 0x11,
    MAX_BYTES_REPLY = 0x12,

    OTA_MODE = 0X20,
    ota_status_a4g = 0x21,

    MACHINE_CTL = 0X30,
    WHEEL_CTL = 0X31,
    MECANUM_CTL = 0X32,
    MOTOR_COUNT = 0X33,
    MOTOR_POS_CTL = 0X34,
    MOTOR_SPEED_CTL = 0X35,
    IO_STATUS_CHECK = 0X36,
    IO_IN_OUT_RET = 0X37,
    IO_OUT_CTL = 0X38,

    VISION_RECORD = 0X50,
    SENSER_SYNC = 0X51,
    VISION_NAV_DATA = 0X52,

    VISION_GET_IMAGE = 0X58,
    VISION_RET_IMAGE = 0X59,

    SEND_PACK = 0X80,
    REQ_PACK = 0X81,
    RESEND_REQ = 0X82,
    TRANS_EXIT = 0X83,
    TRANS_END = 0X84,
    CUSTOM_MSG_PACK = 0x8D,
    CUSTOM_MSG_START = 0X8E,
    CUSTOM_MSG_END = 0X8F
} HexProtocolCommand;

void HexProtocol_Init(uint16_t localAddr, uint16_t targetAddr);
// 常规数据接收
void HexProtocol_setBuffer(uint8_t *buffer, int len); // 传入接收到的数据
bool HexProtocol_checkBuffer();                       // 检查帧头、数据长度、CRC
uint8_t HexProtocol_getCommand();
int HexProtocol_getData(unsigned char *data);
// 常规数据发送
int HexProtocol_mergeBuffer(uint8_t command, uint8_t dataLen, uint8_t *data, uint8_t *packBuffer);
// 文件传输
int HexProtocol_getPackData(uint16_t *packNum, uint8_t *packBuffer);

#ifdef __cplusplus
}
#endif

#endif /* HEXPROTOCOL_H_ */
