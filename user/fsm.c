#include "fsm.h"
#include <stdbool.h>
#include <stddef.h>

uint32_t (*fsm_get_time_ms)(void) = NULL;

// 查找状态表中指定状态的项
static const fsm_state_item_t *fsm_find_state_item(const fsm_state_item_t *table,
                                                   size_t table_size,
                                                   fsm_state_t state)
{
    if (table == NULL || table_size == 0)
    {
        return NULL;
    }

    for (size_t i = 0; i < table_size; ++i)
    {
        if (table[i].state == state)
        {
            return &table[i];
        }
    }

    return NULL;
}

void fsm_registerfunc(uint32_t (*get_time_ms_func)(void))
{
    fsm_get_time_ms = get_time_ms_func;
}

// 初始化状态机
bool fsm_init(fsm_t *fsm,
              fsm_state_t initial_state,
              const fsm_state_item_t *state_table,
              size_t table_size,
              void *context)
{
    if (fsm == NULL || state_table == NULL || table_size == 0)
    {
        return false;
    }

    // 设置初始状态和运行标志
    fsm->current_state = initial_state;
    fsm->previous_state = FSM_STATE_NONE;
    fsm->next_state = initial_state;
    fsm->running = true;

    // 查找初始状态的处理函数
    const fsm_state_item_t *item = fsm_find_state_item(state_table, table_size, initial_state);
    if (item == NULL)
    {
        fsm->current_action = NULL;
        return false;
    }

    // 初始化状态切换时间并设置当前状态的处理函数
    fsm->last_update_time_ms = fsm_get_time_ms();
    fsm->current_action = item->action;
    if (fsm->current_action != NULL)
    {
        fsm->current_action(context);
    }

    return true;
}

// 切换到指定状态
void fsm_goto_state(fsm_t *fsm, fsm_state_t next_state)
{
    if (fsm == NULL)
    {
        return;
    }

    fsm->next_state = next_state;
    fsm->last_update_time_ms = fsm_get_time_ms(); // 更新状态切换时间
}

// 暂停状态机运行
void fsm_pause(fsm_t *fsm)
{
    if (fsm == NULL)
    {
        return;
    }

    fsm->running = false;
}

// 恢复状态机运行
void fsm_resume(fsm_t *fsm)
{
    if (fsm == NULL)
    {
        return;
    }

    fsm->running = true;
}

// 更新状态机，执行当前状态的处理函数
bool fsm_update(fsm_t *fsm,
                const fsm_state_item_t *state_table,
                size_t table_size,
                void *context)
{
    if (fsm == NULL || state_table == NULL || table_size == 0)
    {
        return false;
    }

    if (!fsm->running)
    {
        return false;
    }

    fsm_state_t target_state = fsm->next_state;

    // 如果目标状态与当前状态不同，则切换状态
    if (target_state != fsm->current_state)
    {
        const fsm_state_item_t *next_item = fsm_find_state_item(state_table, table_size, target_state);
        if (next_item == NULL)
        {
            return false;
        }

        fsm->previous_state = fsm->current_state;
        fsm->current_state = target_state;
        fsm->current_action = next_item->action;
        fsm->next_state = fsm->current_state;
    }
    else
    {
        const fsm_state_item_t *current_item = fsm_find_state_item(state_table, table_size, fsm->current_state);
        if (current_item == NULL)
        {
            return false;
        }

        fsm->current_action = current_item->action;
    }

    // 执行当前状态的处理函数
    if (fsm->current_action != NULL)
    {
        fsm->current_action(context);
        return true;
    }

    return false;
}

// 获取当前状态
fsm_state_t fsm_get_current_state(const fsm_t *fsm)
{
    return (fsm != NULL) ? fsm->current_state : FSM_STATE_NONE;
}

// 获取上一状态
fsm_state_t fsm_get_previous_state(const fsm_t *fsm)
{
    return (fsm != NULL) ? fsm->previous_state : FSM_STATE_NONE;
}

uint32_t fsm_get_last_update_time(const fsm_t *fsm)
{
    return (fsm != NULL) ? fsm->last_update_time_ms : 0;
}
