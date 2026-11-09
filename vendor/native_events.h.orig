#ifndef J2ME_BBK9588_NATIVE_EVENTS_H
#define J2ME_BBK9588_NATIVE_EVENTS_H

#include "bda_types.h"

#define J2ME_NATIVE_EVENT_CAPACITY 32u

typedef enum j2me_native_event_type {
    J2ME_NATIVE_EVENT_NONE = 0,
    J2ME_NATIVE_EVENT_TOUCH_DOWN = 1,
    J2ME_NATIVE_EVENT_TOUCH_MOVE = 2,
    J2ME_NATIVE_EVENT_TOUCH_UP = 3,
    J2ME_NATIVE_EVENT_KEY_DOWN = 4,
    J2ME_NATIVE_EVENT_KEY_UP = 5,
    J2ME_NATIVE_EVENT_KEY_REPEAT = 6
} j2me_native_event_type_t;

typedef struct j2me_native_event {
    j2me_native_event_type_t type;
    s16 x;
    s16 y;
    s32 key;
    u32 timestamp_ms;
} j2me_native_event_t;

typedef struct j2me_native_event_queue {
    volatile u32 read_index;
    volatile u32 write_index;
    volatile u32 dropped;
    volatile u32 dropped_critical;
    volatile u32 move_coalesced;
    volatile u32 peak_depth;
    j2me_native_event_t events[J2ME_NATIVE_EVENT_CAPACITY];
} j2me_native_event_queue_t;

void j2me_native_event_queue_reset(j2me_native_event_queue_t *queue);
void j2me_native_event_push(
    j2me_native_event_queue_t *queue,
    j2me_native_event_type_t type,
    s16 x,
    s16 y,
    u32 timestamp_ms
);
void j2me_native_event_push_key(
    j2me_native_event_queue_t *queue,
    j2me_native_event_type_t type,
    s32 key,
    u32 timestamp_ms
);
int j2me_native_event_pop(
    j2me_native_event_queue_t *queue,
    j2me_native_event_t *event
);

#endif
