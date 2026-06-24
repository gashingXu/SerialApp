#include <windows.h>
#include <iostream>
#include <string>
#include <chrono>

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
    timeouts.ReadIntervalTimeout = 10;          // 字符间超时(ms)
    timeouts.ReadTotalTimeoutMultiplier = 0;   // 每字节额外乘数
    timeouts.ReadTotalTimeoutConstant = 0;   // 总超时常数(ms)
    // 写超时可选
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 100;

    if (!SetCommTimeouts(hCom, &timeouts))
    {
        std::cerr << "设置超时失败" << std::endl;
        CloseHandle(hCom);
        return INVALID_HANDLE_VALUE;
    }

    std::cout << "串口 " << portName << " 已打开，开始回传数据... (按 Ctrl+C 退出)" << std::endl;
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

static void SerialSendData(uint8_t* send, uint16_t len)
{
    unsigned char buffer[READ_BUFFER_SIZE];
    DWORD bytesWrite = 0;
    DWORD bytesWritten = 0;

    memcpy(buffer, send, len);
    bytesWrite = len;
    bytesWritten = len;

    WriteFile(hCom, buffer, bytesWrite, &bytesWritten, NULL);
    std::cout << "发送了 " << bytesWrite << " 字节" << std::endl;
    //if (bytesWritten != bytesWrite)
    //{
    //    std::cerr << "写入不完整" << std::endl;
    //}
    //else
    //{
    //    std::cout << "回传了 " << bytesRead << " 字节" << std::endl;
    //}
}

int main(int argc, char* argv[])
{
    std::string portName = "COM2";
    hCom = OpenSerialPort(portName);
    if (hCom == INVALID_HANDLE_VALUE)
        return 1;

    uint8_t recv[READ_BUFFER_SIZE] = { 0 };
    uint16_t readlen = 0;

    while (true)
    {
        if (SerialReadData(recv, &readlen))
        {
            SerialSendData(recv, readlen);
            std::cerr << "time=" << getCurrentMillis() << std::endl;
        }
    }

    CloseHandle(hCom);
    return 0;
}