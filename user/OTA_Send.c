#include "OTA_Send.h"
#include "HexProtocol.h"
#include <string.h>
#include <stdio.h>

#define BUF_SIZE 255
#define OTA_DATA_MAX (BUF_SIZE - 2) // 最大不能超过(BUF_SIZE-2)

#define TIMEOUT_MS 3000
#define MAX_RETRY 5

// 状态定义
typedef enum
{
    OTA_STATE_IDLE = 0,
    OTA_STATE_START,
    OTA_STATE_WAIT_START_ACK,
    OTA_STATE_SEND_DATA,
    OTA_STATE_WAIT_RESPONSE,
    OTA_STATE_END,
    OTA_STATE_ERROR
} ota_state_e;

// 内部结构体
typedef struct
{
    fsm_t fsm;
    fsm_state_item_t state_table[8];
    size_t table_size;

    bool running;
    uint16_t total_packets;
    uint16_t current_packet;
    uint16_t reply_packet;

    uint8_t reply_buf[BUF_SIZE];
    uint8_t reply_len;
    bool reply_flag;

    uint8_t retry_count;

    bool need_data;
} ota_send_t;

typedef struct
{
    ota_get_time_ms_t get_time_ms_func;
    ota_get_bin_data_t get_bin_data_func;
    ota_send_data_t send_data_func;
} ota_func_t;

static ota_send_t ota_send_state;
static ota_func_t ota_func;

// forward handlers
static void ota_idle_handler(void *ctx);
static void ota_start_handler(void *ctx);
static void ota_wait_start_ack_handler(void *ctx);
static void ota_send_data_handler(void *ctx);
static void ota_wait_response_handler(void *ctx);
static void ota_end_handler(void *ctx);
static void ota_error_handler(void *ctx);

// state table initializer
static void ota_build_state_table(ota_send_t *ota)
{
    ota->state_table[OTA_STATE_IDLE] = (fsm_state_item_t){.state = OTA_STATE_IDLE, .action = ota_idle_handler};
    ota->state_table[OTA_STATE_START] = (fsm_state_item_t){.state = OTA_STATE_START, .action = ota_start_handler};
    ota->state_table[OTA_STATE_WAIT_START_ACK] = (fsm_state_item_t){.state = OTA_STATE_WAIT_START_ACK, .action = ota_wait_start_ack_handler};
    ota->state_table[OTA_STATE_SEND_DATA] = (fsm_state_item_t){.state = OTA_STATE_SEND_DATA, .action = ota_send_data_handler};
    ota->state_table[OTA_STATE_WAIT_RESPONSE] = (fsm_state_item_t){.state = OTA_STATE_WAIT_RESPONSE, .action = ota_wait_response_handler};
    ota->state_table[OTA_STATE_END] = (fsm_state_item_t){.state = OTA_STATE_END, .action = ota_end_handler};
    ota->state_table[OTA_STATE_ERROR] = (fsm_state_item_t){.state = OTA_STATE_ERROR, .action = ota_error_handler};
    ota->table_size = ARRAY_SIZE(ota->state_table);
}

// API implementation

// 初始化接口函数
void OTA_Send_RegisterFunctions(ota_get_time_ms_t get_time_ms_func,
                                ota_get_bin_data_t get_bin_data_func,
                                ota_send_data_t send_data_func)
{
    ota_func.get_time_ms_func = get_time_ms_func;
    ota_func.get_bin_data_func = get_bin_data_func;
    ota_func.send_data_func = send_data_func;
}

// 状态机表初始化
bool OTA_Send_Init()
{
    ota_send_t *ota = &ota_send_state;
    memset(ota, 0, sizeof(*ota));
    ota_build_state_table(ota);

    // init fsm and execute idle action
    fsm_registerfunc(ota_func.get_time_ms_func);
    return fsm_init(&ota->fsm, OTA_STATE_IDLE, ota->state_table, ota->table_size, NULL);
}

// 在主循环中周期调用以运行 FSM
void OTA_Send_Update(void)
{
    ota_send_t *ota = &ota_send_state;
    fsm_update(&ota->fsm, ota->state_table, ota->table_size, NULL);
}

bool OTA_Send_IsRunning(void)
{
    ota_send_t *ota = &ota_send_state;
    return ota->running;
}

uint8_t OTA_Send_GetProgress(void)
{
    ota_send_t *ota = &ota_send_state;
    if (ota->total_packets == 0)
    {
        return 0;
    }
    float percent = ((float)ota->current_packet / ota->total_packets);
    return percent * 100;
}

uint16_t OTA_Send_GetCurrentState(void)
{
    ota_send_t *ota = &ota_send_state;
    return (uint16_t)fsm_get_current_state(&ota->fsm);
}

uint16_t OTA_Send_GetPreviousState(void)
{
    ota_send_t *ota = &ota_send_state;
    return (uint16_t)fsm_get_previous_state(&ota->fsm);
}

// 启动 OTA 传输
bool OTA_Send_Start(uint32_t bin_size)
{
    ota_send_t *ota = &ota_send_state;

    if (ota->running)
    {
        return false; // already running
    }

    if (bin_size == 0)
    {
        return false;
    }

    ota->running = true;
    ota->total_packets = (bin_size % OTA_DATA_MAX == 0 ? bin_size / OTA_DATA_MAX : 1 + bin_size / OTA_DATA_MAX);
    ota->current_packet = 0;
    ota->reply_packet = 0;

    memset(ota->reply_buf, 0, sizeof(ota->reply_buf));
    ota->reply_len = 0;
    ota->reply_flag = false;
    ota->retry_count = 0;

    return true;
}

// 当串口接收到回复时调用
bool OTA_Send_HandleReply()
{
    ota_send_t *ota = &ota_send_state;

    ota->reply_flag = true;

    return true;
}

// Handlers
static void ota_idle_handler(void *ctx)
{
    // idle: nothing to do
    (void)ctx;
    ota_send_t *ota = &ota_send_state;

    // 有任务则开始
    if (ota->running)
    {
        fsm_goto_state(&ota->fsm, OTA_STATE_START);
    }
}

static void ota_start_handler(void *ctx)
{
    (void)ctx;
    ota_send_t *ota = &ota_send_state;

    // 发送开始帧
    uint8_t pack[64] = {0};
    uint8_t numbuf[2] = {(uint8_t)(ota->total_packets >> 8), (uint8_t)(ota->total_packets & 0xFF)};
    int len = HexProtocol_mergeBuffer(OTA_MODE, 2, numbuf, pack);
    ota_func.send_data_func(pack, len);

    // goto wait ack
    fsm_goto_state(&ota->fsm, OTA_STATE_WAIT_START_ACK);
}

static void ota_wait_start_ack_handler(void *ctx)
{
    (void)ctx;
    ota_send_t *ota = &ota_send_state;

    // 检查回复
    if (ota->reply_flag)
    {
        ota->reply_flag = false;
        // check reply
        uint8_t cmd = HexProtocol_getCommand();
        HexProtocol_getPackData(&ota->reply_packet, ota->reply_buf);
        if (cmd == REQ_PACK && ota->reply_packet == ota->current_packet)
        {
            fsm_goto_state(&ota->fsm, OTA_STATE_SEND_DATA);
            ota->retry_count = 0;
            return;
        }
    }

    // 超时判断(不能与上面的if相对，上面有可能校验失败)
    if (ota_func.get_time_ms_func() - ota->fsm.last_update_time_ms > TIMEOUT_MS)
    {
        if (++ota->retry_count > MAX_RETRY)
        {
            fsm_goto_state(&ota->fsm, OTA_STATE_ERROR);
        }
        else
        {
            fsm_goto_state(&ota->fsm, OTA_STATE_START);
        }
    }
}

static void ota_send_data_handler(void *ctx)
{
    (void)ctx;
    ota_send_t *ota = &ota_send_state;

    if (ota->current_packet < ota->total_packets)
    {
        // 组包
        uint8_t pack[BUF_SIZE + 8] = {0};
        uint8_t data[BUF_SIZE] = {(uint8_t)(ota->current_packet >> 8), (uint8_t)(ota->current_packet & 0xFF)};
        uint8_t data_len = OTA_DATA_MAX;

        // 最后一包填充0xff
        if (ota->current_packet == ota->total_packets - 1)
        {
            memset(&data[2], 0xff, data_len);
        }

        // 先去获取bin数据
        if (!ota_func.get_bin_data_func(data_len * ota->current_packet, &data[2], data_len))
        {
            // 获取数据失败
            fsm_goto_state(&ota->fsm, OTA_STATE_ERROR);
            return;
        }
        else
        {
            int len = HexProtocol_mergeBuffer(SEND_PACK, data_len + 2, data, pack);
            ota_func.send_data_func(pack, len);

            // goto wait response
            fsm_goto_state(&ota->fsm, OTA_STATE_WAIT_RESPONSE);
        }
    }
    else
    {
        fsm_goto_state(&ota->fsm, OTA_STATE_ERROR);
    }

    return;
}

static void ota_wait_response_handler(void *ctx)
{
    (void)ctx;
    ota_send_t *ota = &ota_send_state;

    // 检查回复
    if (ota->reply_flag)
    {
        ota->reply_flag = false;
        // check reply
        uint8_t cmd = HexProtocol_getCommand();
        HexProtocol_getPackData(&ota->reply_packet, ota->reply_buf);

        // 回复正确
        if (cmd == REQ_PACK)
        {
            if (ota->reply_packet == ota->current_packet + 1)
            {
                // 请求下一包
                ota->current_packet++;
                if (ota->reply_packet == ota->total_packets)
                {
                    // 最后一包已经发送完毕
                    fsm_goto_state(&ota->fsm, OTA_STATE_END);
                }
                else
                {
                    fsm_goto_state(&ota->fsm, OTA_STATE_SEND_DATA);
                }
                ota->retry_count = 0;
                return;
            }
            else if (ota->reply_packet == ota->current_packet)
            {
                // 请求当前包则重发
                fsm_goto_state(&ota->fsm, OTA_STATE_SEND_DATA);
                return;
            }
        }
    }

    // 超时判断(不能与上面的if相对，上面有可能校验失败)
    if (ota_func.get_time_ms_func() - ota->fsm.last_update_time_ms > TIMEOUT_MS)
    {
        if (++ota->retry_count > MAX_RETRY)
        {
            // 超时次数过多，进入错误状态
            fsm_goto_state(&ota->fsm, OTA_STATE_ERROR);
        }
        else
        {
            // 超时重发
            fsm_goto_state(&ota->fsm, OTA_STATE_SEND_DATA);
        }
    }
}

static void ota_end_handler(void *ctx)
{
    (void)ctx;
    ota_send_t *ota = &ota_send_state;

    uint8_t pack[64] = {0};
    uint8_t dummy[1] = {0};
    int len = HexProtocol_mergeBuffer(TRANS_END, 0, dummy, pack);
    ota_func.send_data_func(pack, len);

    ota->running = false;

    // back to idle
    fsm_goto_state(&ota->fsm, OTA_STATE_IDLE);
}

static void ota_error_handler(void *ctx)
{
    (void)ctx;
    ota_send_t *ota = &ota_send_state;

    uint8_t pack[64] = {0};
    uint8_t dummy[1] = {0};
    int len = HexProtocol_mergeBuffer(TRANS_EXIT, 0, dummy, pack);
    ota_func.send_data_func(pack, len);

    ota->running = false;

    // back to idle
    fsm_goto_state(&ota->fsm, OTA_STATE_IDLE);
}
