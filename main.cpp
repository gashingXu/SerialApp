#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <chrono>
#include "user/OTA_Send.h"
#include "user/HexProtocol.h"

// 获取当前时间戳（毫秒）
uint32_t getCurrentMillis() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    uint32_t time = ms.count() & 0xffffffff;

    return time;
}

// 串口配置
const DWORD BAUD_RATE = CBR_115200;   // 波特率
const BYTE DATA_BITS = 8;
const BYTE STOP_BITS = ONESTOPBIT;
const BYTE PARITY = NOPARITY;
const DWORD READ_BUFFER_SIZE = 512; // 每次读取的字节数
HANDLE hCom;

// 打开并配置串口
HANDLE OpenSerialPort(const std::string& portName)
{
    // 处理COM10及以上端口名
    std::string fullPortName = portName;
    if (portName.size() > 3 && portName.substr(0, 3) == "COM")
    {
        int portNum = std::stoi(portName.substr(3));
        if (portNum >= 10)
            fullPortName = "\\\\.\\" + portName;
    }

    HANDLE hCom = CreateFileA(
        fullPortName.c_str(),
        GENERIC_READ | GENERIC_WRITE, // 需要读写权限
        0,                           // 独占
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hCom == INVALID_HANDLE_VALUE)
    {
        std::cerr << "打开串口失败，错误码: " << GetLastError() << std::endl;
        return INVALID_HANDLE_VALUE;
    }

    // 配置串口参数
    DCB dcb = { 0 };
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(hCom, &dcb))
    {
        std::cerr << "获取串口状态失败" << std::endl;
        CloseHandle(hCom);
        return INVALID_HANDLE_VALUE;
    }

    dcb.BaudRate = BAUD_RATE;
    dcb.ByteSize = DATA_BITS;
    dcb.StopBits = STOP_BITS;
    dcb.Parity = PARITY;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    if (!SetCommState(hCom, &dcb))
    {
        std::cerr << "设置串口参数失败" << std::endl;
        CloseHandle(hCom);
        return INVALID_HANDLE_VALUE;
    }

    // 设置超时（避免ReadFile无限阻塞）
    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 0;          // 字符间超时(ms)
    timeouts.ReadTotalTimeoutMultiplier = 0;   // 每字节额外乘数
    timeouts.ReadTotalTimeoutConstant = 15;   // 总超时常数(ms)
    // 写超时可选
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 100;

    if (!SetCommTimeouts(hCom, &timeouts))
    {
        std::cerr << "设置超时失败" << std::endl;
        CloseHandle(hCom);
        return INVALID_HANDLE_VALUE;
    }

    std::cout << "串口 " << portName << " 已打开... (按 Ctrl+C 退出)" << std::endl;
    return hCom;
}

static bool SerialReadData(uint8_t *recv, uint16_t *readlen)
{
    unsigned char buffer[READ_BUFFER_SIZE];
    DWORD bytesRead = 0;
    DWORD bytesWritten = 0;

    // 读取数据
    BOOL readSuccess = ReadFile(hCom, buffer, READ_BUFFER_SIZE, &bytesRead, NULL);
    if (!readSuccess)
    {
        std::cerr << "读取失败，错误码: " << GetLastError() << std::endl;
        //break;
        return false;
    }

    if (bytesRead > 0)
    {
        memcpy(recv, buffer, bytesRead);
        *readlen = bytesRead;
        return true;
    }
    else
    {
        return false;
    }
}

static void SerialSendData(const uint8_t* send, uint16_t len)
{
    unsigned char buffer[READ_BUFFER_SIZE];
    DWORD bytesWrite = 0;
    DWORD bytesWritten = 0;

    memcpy(buffer, send, len);
    bytesWrite = len;
    bytesWritten = len;

    WriteFile(hCom, buffer, bytesWrite, &bytesWritten, NULL);
    //std::cout << "发送了 " << bytesWrite << " 字节" << std::endl;
    //if (bytesWritten != bytesWrite)
    //{
    //    std::cerr << "写入不完整" << std::endl;
    //}
    //else
    //{
    //    std::cout << "回传了 " << bytesRead << " 字节" << std::endl;
    //}
}

std::ifstream file;

bool get_bin_data(uint32_t index, uint8_t* data, uint8_t len)
{
    char buf[READ_BUFFER_SIZE] = { 0 };
    file.read(buf, len);
    memcpy(data, buf, len);

    std::streamsize bytesRead = file.gcount();
    if (bytesRead == 0)
    {
        return false;
    }

    return true;
}

int main(int argc, char* argv[])
{
    // 1. 打开串口
    std::string portName = "COM";
    std::cout << "请输入需要打开的串口号（COMX）: ";
    std::cin >> portName;
    hCom = OpenSerialPort(portName);
    if (hCom == INVALID_HANDLE_VALUE)
    {
        system("pause");
        return 1;
    }

    // 2. 打开文件
    std::string fileName = "data.bin"; // 默认文件名
    std::cout << "请输入需要发送的完整文件名（xxx.bin）: ";
    std::cin >> fileName;
    file.open(fileName, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cerr << "无法打开文件 " << fileName << "，请确保文件存在。" << std::endl;
        CloseHandle(hCom);

        system("pause");
        return 1;
    }

    // 3. 获取文件大小
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    uint32_t file_size = (uint32_t)fileSize;
    std::cout << "文件大小: " << fileSize << " 字节" << std::endl;

    // 4. 初始化ota发送
    HexProtocol_Init(1, 1);
    std::cout << "hex地址为1 " << std::endl;
    OTA_Send_RegisterFunctions(getCurrentMillis, get_bin_data, SerialSendData);
    OTA_Send_Init();

    // 5. 开始任务
    if (!OTA_Send_Start(file_size))
    {
        std::cout << "ota 发送开始失败" << std::endl;
    }
    else
    {
        std::cout << "ota 发送开始！！！" << std::endl;
    }

    uint8_t recv[READ_BUFFER_SIZE] = { 0 };
    uint16_t readlen = 0;

    while (OTA_Send_IsRunning())
    {
        OTA_Send_Update();
        if (SerialReadData(recv, &readlen))
        {
            printf("当前进度 %d%%\r\n", OTA_Send_GetProgress());
            HexProtocol_setBuffer(recv, readlen);
            if (HexProtocol_checkBuffer())
            {
                uint8_t cmd = HexProtocol_getCommand();
                if (cmd == REQ_PACK)
                {
                    OTA_Send_HandleReply();
                }
            }
        }
    }
    printf("当前进度 %d%%\r\n", OTA_Send_GetProgress());
    std::cout << "ota 发送完成！！！" << std::endl;

    CloseHandle(hCom);
    system("pause");
    return 0;
}