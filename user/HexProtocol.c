#include "HexProtocol.h"
#include <string.h>

struct
{
    uint16_t localAddr;
    uint16_t targetAddr;
    bool passCheck;
    uint8_t *receiveBuff;
    int receiveLen;
} HexProtocol_data;

void HexProtocol_Init(uint16_t localAddr, uint16_t targetAddr)
{
    HexProtocol_data.localAddr = localAddr;
    HexProtocol_data.targetAddr = targetAddr;
}

// 常规数据接收
void HexProtocol_setBuffer(uint8_t *buffer, int len) // 传入接收到的数据
{
    HexProtocol_data.passCheck = false;
    HexProtocol_data.receiveBuff = buffer;
    HexProtocol_data.receiveLen = len;
}

bool HexProtocol_checkBuffer() // 检查帧头、数据长度、CRC
{
    if (HexProtocol_data.receiveBuff != 0 && HexProtocol_data.receiveLen >= 6)
    {
        if (HexProtocol_data.receiveBuff[0] == 0x1a)
        {
            uint16_t addr = HexProtocol_data.receiveBuff[1] << 8 | HexProtocol_data.receiveBuff[2];
            if (addr == HexProtocol_data.localAddr || addr == 0)
            {
                uint8_t cmd = HexProtocol_data.receiveBuff[3];
                if (cmd == 0x80) // 文件传输
                {
                    uint8_t msgcrc = HexProtocol_data.receiveBuff[HexProtocol_data.receiveLen - 1];
                    uint8_t checkCRC = 0;
                    DataCheck_GetCRC8(HexProtocol_data.receiveBuff, HexProtocol_data.receiveLen - 1, &checkCRC);
                    if (msgcrc == checkCRC)
                    {
                        HexProtocol_data.passCheck = true;
                        return true;
                    }
                }
                else // 常规通讯
                {
                    int len = HexProtocol_data.receiveBuff[4];
                    if (HexProtocol_data.receiveLen >= 5 + len + 1 && HexProtocol_data.receiveLen < 0xff)
                    {
                        uint8_t msgcrc = HexProtocol_data.receiveBuff[5 + len];
                        uint8_t checkCRC = 0;
                        DataCheck_GetCRC8(HexProtocol_data.receiveBuff, 5 + len, &checkCRC);
                        if (msgcrc == checkCRC)
                        {
                            HexProtocol_data.passCheck = true;
                            return true;
                        }
                    }
                    else if (len == 0xff)
                    {
                        uint8_t msgcrc = HexProtocol_data.receiveBuff[6];
                        uint8_t checkCRC = 0;
                        DataCheck_GetCRC8(HexProtocol_data.receiveBuff, 6, &checkCRC);
                        if (msgcrc == checkCRC)
                        {
                            HexProtocol_data.passCheck = true;
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

uint8_t HexProtocol_getCommand()
{
    if (HexProtocol_data.passCheck)
    {
        return HexProtocol_data.receiveBuff[3];
    }
    return 0;
}

int HexProtocol_getData(uint8_t *data)
{
    int len = 0;
    if (HexProtocol_data.passCheck)
    {
        len = HexProtocol_data.receiveBuff[4];
        memcpy(data, &HexProtocol_data.receiveBuff[5], len);
    }
    return len;
}

// 常规数据发送
int HexProtocol_mergeBuffer(uint8_t command, uint8_t dataLen, uint8_t *data, uint8_t *packBuffer)
{
    int totalLen = 0;
    packBuffer[0] = 0x1a;
    packBuffer[1] = HexProtocol_data.targetAddr >> 8;
    packBuffer[2] = HexProtocol_data.targetAddr;
    packBuffer[3] = command;
    packBuffer[4] = dataLen;
    if (dataLen > 0)
    {
        memcpy(&packBuffer[5], data, dataLen);
        DataCheck_GetCRC8(packBuffer, 5 + dataLen, &packBuffer[5 + dataLen]);
        totalLen = 5 + dataLen + 1;
    }
    else
    {
        DataCheck_GetCRC8(packBuffer, 5, &packBuffer[5]);
        totalLen = 6;
    }
    return totalLen;
}

int HexProtocol_getPackData(uint16_t *packNum, uint8_t *packBuffer)
{
    if (HexProtocol_data.passCheck)
    {
        if (HexProtocol_getCommand() == OTA_MODE || HexProtocol_getCommand() == SEND_PACK || HexProtocol_getCommand() == REQ_PACK)
        {
            if (HexProtocol_data.receiveLen >= 8)
            {
                *packNum = HexProtocol_data.receiveBuff[5] << 8 | HexProtocol_data.receiveBuff[6] << 0;
                memcpy(packBuffer, &HexProtocol_data.receiveBuff[7], HexProtocol_data.receiveLen - 8);
                return HexProtocol_data.receiveLen - 8;
            }
        }
    }
    return 0;
}