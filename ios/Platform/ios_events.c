/*
 * 原生事件队列 —— 原样移植 src/platform/bbk9588/native_events.c 的语义
 *
 *  - 定长 32 深环形队列，不做动态分配（原项目 6 MiB 硬上限下必须如此）
 *  - TOUCH_MOVE 与队尾 move 合并，避免 move 洪泛把 VM 饿死
 *  - move / key-repeat 属于「可丢弃」事件：剩余空槽 <= 2 时直接丢弃，
 *    给 down/up 这类关键事件留出顺序位置
 *  - 记录 dropped / dropped_critical / move_coalesced / peak_depth，
 *    与原项目 J2MEPHONE.LOG 的 RAW_* 计数器一一对应
 */
#include "ios_platform.h"

IOSNativeEventQueue g_j2me_events;

void ios_event_queue_reset(IOSNativeEventQueue *q) {
    q->read_index = 0u;
    q->write_index = 0u;
    q->dropped = 0u;
    q->dropped_critical = 0u;
    q->move_coalesced = 0u;
    q->peak_depth = 0u;
}

static uint32_t event_free_slots(const IOSNativeEventQueue *q) {
    uint32_t used = (q->write_index + IOS_EVENT_CAPACITY - q->read_index) %
                    IOS_EVENT_CAPACITY;
    return (IOS_EVENT_CAPACITY - 1u) - used;
}

static int event_try_coalesce_move(IOSNativeEventQueue *q,
                                   int x, int y, uint32_t ms) {
    uint32_t previous;
    if (q->read_index == q->write_index)
        return 0;
    previous = (q->write_index + IOS_EVENT_CAPACITY - 1u) % IOS_EVENT_CAPACITY;
    if (q->events[previous].type != J2ME_INPUT_POINTER_DRAG)
        return 0;
    q->events[previous].x = (int16_t)x;
    q->events[previous].y = (int16_t)y;
    q->events[previous].timestamp_ms = ms;
    ++q->move_coalesced;
    return 1;
}

static void event_push_value(IOSNativeEventQueue *q, J2MEInputType type,
                             int x, int y, int key, uint32_t ms) {
    uint32_t write_index;
    uint32_t next_index;
    uint32_t depth;
    int expendable = (type == J2ME_INPUT_POINTER_DRAG ||
                      type == J2ME_INPUT_KEY_REPEAT);

    if (type == J2ME_INPUT_POINTER_DRAG &&
        event_try_coalesce_move(q, x, y, ms))
        return;

    /* 给 touch/key release 的顺序保留两个槽位 */
    if (expendable && event_free_slots(q) <= 2u) {
        ++q->dropped;
        return;
    }
    write_index = q->write_index;
    next_index = (write_index + 1u) % IOS_EVENT_CAPACITY;
    if (next_index == q->read_index) {
        ++q->dropped;
        if (!expendable)
            ++q->dropped_critical;
        return;
    }
    q->events[write_index].type = type;
    q->events[write_index].x = (int16_t)x;
    q->events[write_index].y = (int16_t)y;
    q->events[write_index].key = (int32_t)key;
    q->events[write_index].timestamp_ms = ms;
    q->write_index = next_index;

    depth = (next_index + IOS_EVENT_CAPACITY - q->read_index) %
            IOS_EVENT_CAPACITY;
    if (depth > q->peak_depth)
        q->peak_depth = depth;
}

void ios_event_push(IOSNativeEventQueue *q, J2MEInputType type,
                    int x, int y, uint32_t ms) {
    event_push_value(q, type, x, y, 0, ms);
}

void ios_event_push_key(IOSNativeEventQueue *q, J2MEInputType type,
                        int key, uint32_t ms) {
    event_push_value(q, type, 0, 0, key, ms);
}

int ios_event_pop(IOSNativeEventQueue *q, IOSNativeEvent *out) {
    uint32_t read_index;
    if (out == NULL)
        return 0;
    read_index = q->read_index;
    if (read_index == q->write_index)
        return 0;
    *out = q->events[read_index];
    q->read_index = (read_index + 1u) % IOS_EVENT_CAPACITY;
    return 1;
}
