#ifndef __OTA_SEND_H__
#define __OTA_SEND_H__

/*
 * @Author     : xu
 * @Date       : 2026-06-24 15:43:28
 * @Description: OTA发送
 *
 * 使用方式：
 *
 * 1. 传入接口函数
 * OTA_Send_RegisterFunctions(get_time_ms_func, get_bin_total_func, get_bin_data_func, send_data_func);
 *
 * 2. 初始化发送模块
 * OTA_Send_Init();
 *
 * 3. 在主循环中周期调用以运行 FSM
 * OTA_Send_Update();
 *
 * 4. 需要启动OTA传输时，传入固件包总数
 * OTA_Send_Start(total_packets);
 *
 * 5. 当串口接收到ota回复(REQ_PACK)时调用
 * OTA_Send_HandleReply();
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include "fsm.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef uint32_t (*ota_get_time_ms_t)(void);
    typedef bool (*ota_get_bin_data_t)(uint32_t index, uint8_t *data, uint8_t len);
    typedef void (*ota_send_data_t)(const uint8_t *msg, uint16_t len);

    // 初始化接口函数
    void OTA_Send_RegisterFunctions(ota_get_time_ms_t get_time_ms_func,
                                    ota_get_bin_data_t get_bin_data_func,
                                    ota_send_data_t send_data_func);

    // 初始化 OTA 发送模块，返回 true 表示成功
    bool OTA_Send_Init();

    // 在主循环中周期调用以运行 FSM
    void OTA_Send_Update(void);

    // 获取当前状态（用于调试/查询）
    bool OTA_Send_IsRunning(void);
    uint8_t OTA_Send_GetProgress(void);
    uint16_t OTA_Send_GetCurrentState(void);
    uint16_t OTA_Send_GetPreviousState(void);

    // 启动 OTA 传输
    bool OTA_Send_Start(uint32_t bin_size);

    // 当串口接收到回复时调用
    bool OTA_Send_HandleReply();

#ifdef __cplusplus
}
#endif

#endif
