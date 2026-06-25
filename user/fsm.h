#ifndef __FSM_H__
#define __FSM_H__

/*
 * @Author     : xu
 * @Date       : 2026-06-24 10:55:22
 * @Description: 通用状态机
 *
 * 使用方式：
 *
 *  1. 定义状态
 * typedef enum {
 *     APP_STATE_IDLE,
 *     APP_STATE_RUN,
 *     APP_STATE_ERROR,
 * } app_state_t;
 *
 * 2. 定义状态处理函数
 * void app_idle_handler(void *context)
 * {
 *     // 状态处理逻辑
 *     // 需要获取当前状态时可调用 fsm_get_current_state()
 *     // 需要获取上一状态时可调用 fsm_get_previous_state()
 * }
 *
 * 3. 定义状态机结构实体
 * fsm_t app_fsm;
 *
 * 4. 定义状态表
 * static const fsm_state_item_t app_state_table[] = {
 *     { APP_STATE_IDLE,  app_idle_handler },
 *     { APP_STATE_RUN,   app_run_handler  },
 *     { APP_STATE_ERROR, app_error_handler },
 * };
 *
 * 5. 初始化状态机
 * void *context = ...; // 上下文数据
 * fsm_init(&app_fsm,
 *          APP_STATE_IDLE,
 *          app_state_table,
 *          ARRAY_SIZE(app_state_table),
 *          context);
 *
 * 6. 在主循环中调用状态机更新
 * fsm_update(&app_fsm, app_state_table, ARRAY_SIZE(app_state_table), context);
 *
 * 7. 在需要切换状态时调用
 * fsm_goto_state(&app_fsm, APP_STATE_RUN);
 * 
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef uint16_t fsm_state_t;

#define FSM_STATE_NONE ((fsm_state_t)0xFFFFu)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

    typedef void (*fsm_state_func_t)(void *context);

    typedef struct
    {
        fsm_state_t state;
        fsm_state_func_t action;
    } fsm_state_item_t;

    typedef struct
    {
        uint32_t last_update_time_ms;
        fsm_state_t current_state;
        fsm_state_t previous_state;
        fsm_state_t next_state;
        fsm_state_func_t current_action;
        bool running;
    } fsm_t;

    void fsm_registerfunc(uint32_t (*get_time_ms_func)(void));
    bool fsm_init(fsm_t *fsm,
                  fsm_state_t initial_state,
                  const fsm_state_item_t *state_table,
                  size_t table_size,
                  void *context);
    void fsm_goto_state(fsm_t *fsm, fsm_state_t next_state);
    void fsm_pause(fsm_t *fsm);
    void fsm_resume(fsm_t *fsm);
    bool fsm_update(fsm_t *fsm,
                    const fsm_state_item_t *state_table,
                    size_t table_size,
                    void *context);
    fsm_state_t fsm_get_current_state(const fsm_t *fsm);
    fsm_state_t fsm_get_previous_state(const fsm_t *fsm);
    uint32_t fsm_get_last_update_time(const fsm_t *fsm);

#ifdef __cplusplus
}
#endif

#endif /* __FSM_H__ */
