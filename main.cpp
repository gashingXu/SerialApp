#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <vector>
#include <string>
#include <cstring>
#include <chrono>
#include <filesystem>   // C++17
#include <algorithm>
#include <iomanip>
#include "user/OTA_Send.h"
#include "user/HexProtocol.h"

namespace fs = std::filesystem;

// 获取当前时间戳（毫秒）
uint32_t getCurrentMillis() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    uint32_t time = ms.count() & 0xffffffff;

    return time;
}

int64_t getCurrentMillis64() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return ms.count();
}

// 将毫秒时间戳转换为格式化的日期时间字符串
// 参数:
//   millis: 自1970-01-01 00:00:00 UTC以来的毫秒数
//   localTime: true 表示本地时间，false 表示 UTC 时间
// 返回: 格式为 "YYYY-MM-DD HH:MM:SS.mmm" 的字符串
std::string formatTimeMillis(int64_t millis, bool localTime = true)
{
    // 1. 分离秒和毫秒
    time_t seconds = static_cast<time_t>(millis / 1000);
    int milliseconds = static_cast<int>(millis % 1000);

    // 2. 转换为 tm 结构
    struct tm timeInfo = { 0 };
    if (localTime) {
        localtime_s(&timeInfo, &seconds);   // Windows 安全函数
    }
    else {
        gmtime_s(&timeInfo, &seconds);      // UTC 时间
    }

    // 3. 格式化为字符串
    std::ostringstream oss;
    oss << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setw(3) << std::setfill('0') << milliseconds;

    return oss.str();
}

std::string getCurrentTimeStr(bool local = true) {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    std::cout << "time=" << ms.count() << std::endl;
    return formatTimeMillis(ms.count(), local);
}

// 串口配置
const DWORD BAUD_RATE = CBR_115200;   // 波特率
const BYTE DATA_BITS = 8;
const BYTE STOP_BITS = ONESTOPBIT;
const BYTE PARITY = NOPARITY;
const DWORD READ_BUFFER_SIZE = 512; // 每次读取的字节数
HANDLE hCom;

// 获取系统所有串口名称（如 COM1, COM2 ...）
std::vector<std::string> GetAvailableComPorts()
{
    std::vector<std::string> ports;
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "HARDWARE\\DEVICEMAP\\SERIALCOMM",
        0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        char valueName[256];
        char valueData[256];
        DWORD valueNameSize, valueDataSize, type;
        DWORD index = 0;

        while (true)
        {
            valueNameSize = sizeof(valueName);
            valueDataSize = sizeof(valueData);
            LONG ret = RegEnumValueA(hKey, index, valueName, &valueNameSize,
                nullptr, &type, (LPBYTE)valueData, &valueDataSize);
            if (ret != ERROR_SUCCESS)
                break;

            if (type == REG_SZ)
            {
                ports.push_back(std::string(valueData));
            }
            ++index;
        }
        RegCloseKey(hKey);
    }
    else
    {
        std::cerr << "无法打开注册表项，请检查权限（可能需要管理员权限）" << std::endl;
    }
    return ports;
}

void ShowComPorts(const std::vector<std::string>& ports)
{
    if (ports.empty())
    {
        std::cout << "未检测到任何串口。请确保串口设备已连接。" << std::endl;
        return;
    }
    std::cout << "检测到以下串口：\n";
    std::cout << "序号\t端口名\n";
    std::cout << "-----------------\n";
    for (size_t i = 0; i < ports.size(); ++i)
        std::cout << "[" << (i + 1) << "]\t" << ports[i] << std::endl;
    std::cout << "-----------------\n";
}

int SelectComPort(const std::vector<std::string>& ports)
{
    if (ports.empty())
        return -1;

    int choice = 0;
    while (true)
    {
        std::cout << "请选择要使用的串口号 (1~" << ports.size() << "): ";
        std::cin >> choice;
        if (std::cin.fail() || choice < 1 || choice > static_cast<int>(ports.size()))
        {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            std::cout << "无效输入，请重新输入。" << std::endl;
        }
        else
        {
            break;
        }
    }
    return choice - 1;
}

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
    timeouts.ReadIntervalTimeout = 5;          // 字符间超时(ms)
    timeouts.ReadTotalTimeoutMultiplier = 0;   // 每字节额外乘数
    timeouts.ReadTotalTimeoutConstant = 20;   // 总超时常数(ms)
    // 写超时可选
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 100;

    if (!SetCommTimeouts(hCom, &timeouts))
    {
        std::cerr << "设置超时失败" << std::endl;
        CloseHandle(hCom);
        return INVALID_HANDLE_VALUE;
    }

    std::cout << "串口 " << portName << " 已打开...\r\n" << std::endl;
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

// ------------------------------------------------------------
// 文件选择功能
// ------------------------------------------------------------
std::vector<fs::path> GetBinFiles()
{
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(fs::current_path()))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".bin")
            files.push_back(entry.path());
    }
    return files;
}

void ShowFileList(const std::vector<fs::path>& files)
{
    if (files.empty())
    {
        std::cout << "当前目录下没有找到任何 .bin 文件。\n";
        return;
    }

    std::cout << "当前目录找到以下 .bin 文件：\n";
    std::cout << "序号\t文件名\t\t\t\t大小(字节)\n";
    std::cout << "----------------------------------------------\n";
    for (size_t i = 0; i < files.size(); ++i)
    {
        uintmax_t size = fs::file_size(files[i]);
        std::cout << "[" << (i + 1) << "]\t" << files[i].filename().string()
            << "\t\t" << size << std::endl;
    }
    std::cout << "----------------------------------------------\n";
}

int SelectFile(const std::vector<fs::path>& files)
{
    if (files.empty())
        return -1;

    int choice = 0;
    while (true)
    {
        std::cout << "请输入要发送的文件序号 (1~" << files.size() << "): ";
        std::cin >> choice;
        if (std::cin.fail() || choice < 1 || choice > static_cast<int>(files.size()))
        {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            std::cout << "无效输入，请重新输入。\n";
        }
        else
        {
            break;
        }
    }
    return choice - 1;  // 转为索引
}

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

void log_print(const char* format, ...)
{
    //std::string localStr = formatTimeMillis(getCurrentMillis64(), true);
    ////std::cout << "[" << localStr << "]";
    //printf("[%s]", localStr.c_str());
    printf(format);
}

int main(int argc, char* argv[])
{
    //std::string localStr = formatTimeMillis(getCurrentMillis64(), true);
    //std::cout << "本地时间: " << localStr << std::endl;
    //std::cout << "本地时间: " << getCurrentTimeStr() << std::endl;

    // 1. 枚举串口
    auto ports = GetAvailableComPorts();
    if (ports.empty())
    {
        std::cout << "未找到可用串口，程序退出。" << std::endl;
        system("pause");
        return 0;
    }

    // 2. 显示并选择
    ShowComPorts(ports);
    int index = SelectComPort(ports);
    if (index < 0)
    {
        std::cout << "未选择串口，退出。" << std::endl;
        return 0;
    }
    std::string selectedPort = ports[index];
    std::cout << "你选择了串口: " << selectedPort << std::endl;

    // 3. 打开串口
    std::string portName = selectedPort;
    //std::cout << "请输入需要打开的串口号（COMX）: ";
    //std::cin >> portName;
    hCom = OpenSerialPort(portName);
    if (hCom == INVALID_HANDLE_VALUE)
    {
        system("pause");
        return 1;
    }

    // 4. 扫描 .bin 文件
    auto files = GetBinFiles();
    if (files.empty())
    {
        std::cout << "当前目录下没有 .bin 文件，程序退出。\n";
        system("pause");
        return 0;
    }

    // 5. 显示列表并让用户选择
    ShowFileList(files);
    index = SelectFile(files);
    if (index < 0)
        return 0;
    fs::path selectedFile = files[index];

    std::cout << "你选择了文件: " << selectedFile.filename().string() << std::endl;

    // 6. 打开文件
    std::string fileName = selectedFile.filename().string(); // 默认文件名
    //std::cout << "请输入需要发送的完整文件名（xxx.bin）: ";
    //std::cin >> fileName;
    file.open(fileName, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cerr << "无法打开文件 " << fileName << "，请确保文件存在。" << std::endl;
        CloseHandle(hCom);

        system("pause");
        return 1;
    }
    std::cout << "文件 " << fileName << " 已打开...\r\n" << std::endl;

    // 7. 获取文件大小
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    uint32_t file_size = (uint32_t)fileSize;
    std::cout << "文件大小: " << fileSize << " 字节" << std::endl;

    // 8. 初始化ota发送
    HexProtocol_Init(1, 1);
    std::cout << "hex地址为1 \r\n" << std::endl;
    OTA_Send_RegisterFunctions(getCurrentMillis, get_bin_data, SerialSendData, log_print);
    OTA_Send_Init();

    // 9. 开始任务
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
            //printf("recv %d=", readlen);
            //for (int i = 0; i < readlen; i++)
            //{
            //    printf("%02x ", recv[i]);
            //}
            //printf("\r\n");
            //std::string localStr = formatTimeMillis(getCurrentMillis64(), true);
            //printf("[%s]\r\n", localStr.c_str());
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

    CloseHandle(hCom);
    system("pause");
    return 0;
}