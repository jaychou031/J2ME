/*
 * 悬浮点 + 半透明手机键盘 —— 复刻 src/platform/bbk9588/phoneme_bbk9588_port.c
 *
 * 所有几何常量、键位布局、5×7 标签字形、alpha 混合系数、音量条颜色
 * 都与原项目一致，保证 240×320 逻辑画面「原汁原味」。
 *
 * 与原项目的差别只有两点：
 *  1) 不再需要脏矩形 / 每 8 行补 PCM 的优化 —— iPhone 13 上单遍合成
 *     76800 像素每帧的开销可以忽略，换来更简单的正确实现；
 *  2) X 键不调用 BBK SDK 的 bda_confirm()，而是通过 action handler
 *     交给 UIKit 弹「是/否」确认框，语义完全一致（只有确认才销毁 MIDlet）。
 */
#include "ios_platform.h"

#include <string.h>

#define FLOAT_RADIUS          15
#define FLOAT_HIT_RADIUS      22
#define DRAG_THRESHOLD_SQ     144   /* 12px */
#define PANEL_X               33
#define PANEL_WIDTH           174
#define PANEL_HEIGHT          232
#define INNER_X               42
#define ACTION_Y              8
#define ACTION_WIDTH          36
#define ACTION_HEIGHT         28
#define ACTION_GAP_X          4
#define VOLUME_BAR_Y          38
#define VOLUME_BAR_HEIGHT     3
#define LR_Y                  44
#define LR_HEIGHT             30
#define LR_WIDTH              75
#define LR_SECOND_X           (INNER_X + 81)
#define NUMERIC_Y             80
#define KEY_WIDTH             48
#define KEY_HEIGHT            32
#define KEY_GAP_X             6
#define KEY_GAP_Y             5

/* 动作键的伪键码，沿用原项目取值 */
#define A_EXIT   (-1001)
#define A_VOLDN  (-1002)
#define A_MUTE   (-1003)
#define A_VOLUP  (-1004)

static const char kLabels[] = "0123456789*#LRX-M+";
/* 7 行 × 5 位，高 5 位不用，bit4..bit0 从左到右 */
static const uint8_t kGlyphs[][7] = {
    {14,17,19,21,25,17,14}, /* 0 */
    {4,12,4,4,4,4,14},      /* 1 */
    {14,17,1,2,4,8,31},     /* 2 */
    {30,1,1,14,1,1,30},     /* 3 */
    {2,6,10,18,31,2,2},     /* 4 */
    {31,16,16,30,1,1,30},   /* 5 */
    {14,16,16,30,17,17,14}, /* 6 */
    {31,1,2,4,8,8,8},       /* 7 */
    {14,17,17,14,17,17,14}, /* 8 */
    {14,17,17,15,1,1,14},   /* 9 */
    {0,21,14,31,14,21,0},   /* * */
    {10,31,10,10,31,10,0},  /* # */
    {16,16,16,16,16,16,31}, /* L */
    {30,17,17,30,20,18,17}, /* R */
    {17,17,10,4,10,17,17},  /* X */
    {0,0,0,31,0,0,0},       /* - */
    {17,27,21,21,17,17,17}, /* M */
    {0,4,4,31,4,4,0}        /* + */
};

static const int kNumericKeys[12] = {
    '1','2','3','4','5','6','7','8','9','*','0','#'
};
static const int kActions[4] = { A_EXIT, A_VOLDN, A_MUTE, A_VOLUP };

static int g_float_x = 240 - 22;
static int g_float_y = 22;
static int g_visible = 0;
static int g_active_key = 0;
static int g_active_action = 0;
static int g_touch_mode = 0;       /* 0 无 1 游戏 2 浮点 3 按键 4 面板 5 动作 */
static int g_touch_start_x = 0;
static int g_touch_start_y = 0;
static int g_dragging = 0;
static int g_last_valid_x = 0;
static int g_last_valid_y = 0;
static int g_volume = 100;
static int g_muted = 0;
static uint32_t g_toggle_count = 0u;
static uint32_t g_key_event_count = 0u;
static uint32_t g_drag_count = 0u;
static uint32_t g_exit_count = 0u;
static uint32_t g_active_key_time = 0u;
static IOSSoftpadActionHandler g_action_handler = NULL;
static void *g_action_user = NULL;

static int inside(int x, int y, int left, int top, int w, int h) {
    return x >= left && y >= top && x < left + w && y < top + h;
}

static int panel_y_for(int float_y) {
    return float_y < IOS_FB_H / 2 ? IOS_FB_H - PANEL_HEIGHT - 8 : 8;
}
static int panel_y(void) {
    return panel_y_for(g_float_y);
}

static int inside_float(int x, int y) {
    int dx = x - g_float_x;
    int dy = y - g_float_y;
    return dx * dx + dy * dy <= FLOAT_HIT_RADIUS * FLOAT_HIT_RADIUS;
}

static int inside_panel(int x, int y) {
    return g_visible && inside(x, y, PANEL_X, panel_y(),
                               PANEL_WIDTH, PANEL_HEIGHT);
}

static int key_at(int x, int y) {
    int py = panel_y();
    int row, column, key_x, key_y;
    if (!g_visible)
        return 0;
    for (column = 0; column < 4; ++column) {
        key_x = INNER_X + column * (ACTION_WIDTH + ACTION_GAP_X);
        if (inside(x, y, key_x, py + ACTION_Y, ACTION_WIDTH, ACTION_HEIGHT))
            return kActions[column];
    }
    if (inside(x, y, INNER_X, py + LR_Y, LR_WIDTH, LR_HEIGHT))
        return -6;
    if (inside(x, y, LR_SECOND_X, py + LR_Y, LR_WIDTH, LR_HEIGHT))
        return -7;
    key_y = py + NUMERIC_Y;
    if (y < key_y)
        return 0;
    row = (y - key_y) / (KEY_HEIGHT + KEY_GAP_Y);
    column = (x - INNER_X) / (KEY_WIDTH + KEY_GAP_X);
    if (row < 0 || row >= 4 || column < 0 || column >= 3)
        return 0;
    key_x = INNER_X + column * (KEY_WIDTH + KEY_GAP_X);
    key_y += row * (KEY_HEIGHT + KEY_GAP_Y);
    if (!inside(x, y, key_x, key_y, KEY_WIDTH, KEY_HEIGHT))
        return 0;
    return kNumericKeys[row * 3 + column];
}

static char label_for(int key) {
    switch (key) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case '*': case '#':
            return (char)key;
        case -6:  return 'L';
        case -7:  return 'R';
        case A_EXIT:  return 'X';
        case A_VOLDN: return '-';
        case A_MUTE:  return 'M';
        case A_VOLUP: return '+';
        default: return '?';
    }
}

static const uint8_t *glyph_for(char label) {
    size_t i;
    for (i = 0u; i < sizeof(kLabels) - 1u; ++i)
        if (kLabels[i] == label)
            return kGlyphs[i];
    return kGlyphs[0];
}

static int glyph_pixel(int x, int y, int cx, int cy, char label) {
    const uint8_t *rows = glyph_for(label);
    int left = cx - 2;
    int top = cy - 3;
    int gx = x - left;
    int gy = y - top;
    if (gx < 0 || gx >= 5 || gy < 0 || gy >= 7)
        return 0;
    return (rows[gy] & (uint8_t)(0x10u >> gx)) != 0u;
}

/* 返回某个像素落在哪个按钮里，供合成时画边框与标签 */
static int button_at(int x, int y, int *bx, int *by, int *bw, int *bh,
                     int *key, char *label) {
    int py = panel_y();
    int i;
    for (i = 0; i < 4; ++i) {
        int left = INNER_X + i * (ACTION_WIDTH + ACTION_GAP_X);
        if (inside(x, y, left, py + ACTION_Y, ACTION_WIDTH, ACTION_HEIGHT)) {
            *bx = left; *by = py + ACTION_Y;
            *bw = ACTION_WIDTH; *bh = ACTION_HEIGHT;
            *key = kActions[i];
            *label = label_for(*key);
            return 1;
        }
    }
    if (inside(x, y, INNER_X, py + LR_Y, LR_WIDTH, LR_HEIGHT)) {
        *bx = INNER_X; *by = py + LR_Y;
        *bw = LR_WIDTH; *bh = LR_HEIGHT;
        *key = -6; *label = 'L';
        return 1;
    }
    if (inside(x, y, LR_SECOND_X, py + LR_Y, LR_WIDTH, LR_HEIGHT)) {
        *bx = LR_SECOND_X; *by = py + LR_Y;
        *bw = LR_WIDTH; *bh = LR_HEIGHT;
        *key = -7; *label = 'R';
        return 1;
    }
    {
        int rel_x = x - INNER_X;
        int rel_y = y - (py + NUMERIC_Y);
        int column, row, left, top;
        if (rel_x < 0 || rel_y < 0)
            return 0;
        column = rel_x / (KEY_WIDTH + KEY_GAP_X);
        row = rel_y / (KEY_HEIGHT + KEY_GAP_Y);
        if (column < 0 || column >= 3 || row < 0 || row >= 4)
            return 0;
        left = INNER_X + column * (KEY_WIDTH + KEY_GAP_X);
        top = py + NUMERIC_Y + row * (KEY_HEIGHT + KEY_GAP_Y);
        if (!inside(x, y, left, top, KEY_WIDTH, KEY_HEIGHT))
            return 0;
        *bx = left; *by = top;
        *bw = KEY_WIDTH; *bh = KEY_HEIGHT;
        *key = kNumericKeys[row * 3 + column];
        *label = (char)*key;
        return 1;
    }
}

static uint16_t blend(uint16_t base, uint16_t overlay, uint32_t alpha) {
    uint32_t inverse = 256u - alpha;
    uint32_t r = ((((uint32_t)base >> 11) & 31u) * inverse +
                  (((uint32_t)overlay >> 11) & 31u) * alpha) >> 8;
    uint32_t g = ((((uint32_t)base >> 5) & 63u) * inverse +
                  (((uint32_t)overlay >> 5) & 63u) * alpha) >> 8;
    uint32_t b = (((uint32_t)base & 31u) * inverse +
                  ((uint32_t)overlay & 31u) * alpha) >> 8;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static uint16_t composite_pixel(int x, int y, uint16_t base) {
    int py = panel_y();
    int result = base;
    if (g_visible && inside(x, y, PANEL_X, py, PANEL_WIDTH, PANEL_HEIGHT)) {
        int bar_width = ACTION_WIDTH * 4 + ACTION_GAP_X * 3;
        result = blend(base, 0x0000u, 224u);
        if (inside(x, y, INNER_X, py + VOLUME_BAR_Y,
                   bar_width, VOLUME_BAR_HEIGHT)) {
            int rel_x = x - INNER_X;
            int level = (int)((uint32_t)g_volume * (uint32_t)bar_width / 100u);
            uint16_t bar = (rel_x < level && !g_muted) ? 0x07e0u : 0x4208u;
            result = blend(base, bar, 232u);
        }
        {
            int bx, by, bw, bh, key;
            char label;
            if (button_at(x, y, &bx, &by, &bw, &bh, &key, &label)) {
                int active = (key == g_active_key || key == g_active_action);
                uint16_t color = active ? 0xfd20u :
                    ((key == A_MUTE && g_muted) ? 0xb800u : 0x0000u);
                uint32_t alpha = active ? 224u : 208u;
                result = blend(base, color, alpha);
                if (x == bx || y == by || x == bx + bw - 1 || y == by + bh - 1)
                    result = blend(base, 0xffffu, 224u);
                if (glyph_pixel(x, y, bx + bw / 2, by + bh / 2, label))
                    result = 0xffffu;
            }
        }
    }
    {
        int dx = x - g_float_x;
        int dy = y - g_float_y;
        int distance = dx * dx + dy * dy;
        if (distance <= FLOAT_RADIUS * FLOAT_RADIUS) {
            int inner = FLOAT_RADIUS - 3;
            uint16_t color = distance > inner * inner ? 0xffffu : 0x4208u;
            uint32_t alpha = distance > inner * inner ? 240u : 216u;
            result = blend(base, color, alpha);
            if (dy >= -1 && dy <= 1 &&
                ((dx >= -6 && dx <= -4) || (dx >= -1 && dx <= 1) ||
                 (dx >= 4 && dx <= 6)))
                result = 0xffffu;
        }
    }
    return (uint16_t)result;
}

void ios_softpad_composite(uint16_t *fb, int fbw, int fbh) {
    int x, y;
    if (fb == NULL || fbw != IOS_FB_W || fbh != IOS_FB_H)
        return;
    /* 只重绘可能被覆盖的区域：面板矩形 + 浮点圆点外接框。
     * 其余像素原样保留（对应原项目「游戏每次 present 跳过已绘制面板区域」）。 */
    {
        int left = g_visible ? PANEL_X : IOS_FB_W;
        int top = g_visible ? panel_y() : 0;
        int right = g_visible ? PANEL_X + PANEL_WIDTH : 0;
        int bottom = g_visible ? top + PANEL_HEIGHT : 0;
        for (y = top; y < bottom; ++y)
            for (x = left; x < right; ++x)
                fb[(size_t)y * fbw + x] =
                    composite_pixel(x, y, fb[(size_t)y * fbw + x]);
    }
    {
        int left = g_float_x - FLOAT_RADIUS - 1;
        int top = g_float_y - FLOAT_RADIUS - 1;
        int right = g_float_x + FLOAT_RADIUS + 2;
        int bottom = g_float_y + FLOAT_RADIUS + 2;
        if (left < 0) left = 0;
        if (top < 0) top = 0;
        if (right > fbw) right = fbw;
        if (bottom > fbh) bottom = fbh;
        for (y = top; y < bottom; ++y)
            for (x = left; x < right; ++x)
                fb[(size_t)y * fbw + x] =
                    composite_pixel(x, y, fb[(size_t)y * fbw + x]);
    }
}

/* ---------- 生命周期 ---------- */
void ios_softpad_init(void) {
    g_float_x = IOS_FB_W - 22;
    g_float_y = 22;
    g_visible = 0;
    g_active_key = 0;
    g_active_action = 0;
    g_touch_mode = 0;
    g_dragging = 0;
    g_volume = 100;
    g_muted = 0;
    ios_event_queue_reset(&g_j2me_events);
}

void ios_softpad_shutdown(void) {
    g_action_handler = NULL;
    g_action_user = NULL;
}

void ios_softpad_set_action_handler(IOSSoftpadActionHandler handler, void *user) {
    g_action_handler = handler;
    g_action_user = user;
}

void ios_softpad_set_visible(int visible) {
    g_visible = visible ? 1 : 0;
}

int ios_softpad_visible(void) {
    return g_visible;
}

void ios_softpad_set_volume(int percent, int muted) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    g_volume = percent;
    g_muted = muted ? 1 : 0;
}

uint32_t ios_softpad_toggle_count(void)   { return g_toggle_count; }
uint32_t ios_softpad_key_event_count(void){ return g_key_event_count; }
uint32_t ios_softpad_drag_count(void)     { return g_drag_count; }
uint32_t ios_softpad_exit_count(void)     { return g_exit_count; }

/* ---------- 输入 ---------- */
static void release_active(uint32_t now) {
    (void)now;
    if (g_active_key != 0) {
        ios_event_push_key(&g_j2me_events, J2ME_INPUT_KEY_UP,
                           g_active_key, ios_j2me_now_ms());
        ++g_key_event_count;
        g_active_key = 0;
    }
    g_active_action = 0;
}

static void press_key(int key, uint32_t now) {
    release_active(now);
    g_active_key = key;
    g_active_key_time = now;
    ios_event_push_key(&g_j2me_events, J2ME_INPUT_KEY_DOWN, key, now);
    ++g_key_event_count;
}

static void activate_action(int action, uint32_t now) {
    release_active(now);
    g_active_action = action;
    g_active_key_time = now;
    if (action == A_EXIT) {
        ++g_exit_count;
    }
    if (g_action_handler != NULL) {
        IOSSoftpadAction mapped;
        switch (action) {
            case A_EXIT:  mapped = IOS_SOFTPAD_ACTION_EXIT; break;
            case A_VOLDN: mapped = IOS_SOFTPAD_ACTION_VOL_DOWN; break;
            case A_VOLUP: mapped = IOS_SOFTPAD_ACTION_VOL_UP; break;
            case A_MUTE:  mapped = IOS_SOFTPAD_ACTION_MUTE; break;
            default:      mapped = IOS_SOFTPAD_ACTION_NONE; break;
        }
        g_action_handler(mapped, g_action_user);
    }
}

int ios_softpad_handle_touch(J2MEInputType type, int x, int y, uint32_t ms) {
    if (type == J2ME_INPUT_POINTER_DOWN) {
        g_touch_start_x = x;
        g_touch_start_y = y;
        g_last_valid_x = x;
        g_last_valid_y = y;
        g_dragging = 0;
        if (inside_float(x, y)) {
            g_touch_mode = 2;   /* 悬浮点 */
            return 1;
        }
        if (inside_panel(x, y)) {
            int key = key_at(x, y);
            if (key <= A_EXIT && key >= A_VOLUP) { /* -1001..-1004 动作键 */
                g_touch_mode = 5;
                activate_action(key, ms);
            } else if (key != 0) {
                g_touch_mode = 3;
                press_key(key, ms);
            } else {
                g_touch_mode = 4;  /* 面板空白，消费掉 */
            }
            return 1;
        }
        g_touch_mode = 1;        /* 游戏区域 */
        return 0;
    }

    if (type == J2ME_INPUT_POINTER_DRAG) {
        if (x >= 0 && y >= 0) {
            g_last_valid_x = x;
            g_last_valid_y = y;
        }
        if (g_touch_mode == 2) {
            int dx = x - g_touch_start_x;
            int dy = y - g_touch_start_y;
            if (!g_dragging && dx * dx + dy * dy > DRAG_THRESHOLD_SQ) {
                g_dragging = 1;
                ++g_drag_count;
            }
            if (g_dragging) {
                int nx = x, ny = y;
                if (nx < FLOAT_RADIUS) nx = FLOAT_RADIUS;
                if (nx > IOS_FB_W - FLOAT_RADIUS) nx = IOS_FB_W - FLOAT_RADIUS;
                if (ny < FLOAT_RADIUS) ny = FLOAT_RADIUS;
                if (ny > IOS_FB_H - FLOAT_RADIUS) ny = IOS_FB_H - FLOAT_RADIUS;
                g_float_x = nx;
                g_float_y = ny;
            }
            return 1;
        }
        if (g_touch_mode == 3 || g_touch_mode == 4 || g_touch_mode == 5)
            return 1;
        return 0;   /* 游戏区拖动交给内核 */
    }

    if (type == J2ME_INPUT_POINTER_UP) {
        /* 真机 UP 坐标可能失效：沿用最后一次有效坐标完成短按判定 */
        if (x < 0 || y < 0) {
            x = g_last_valid_x;
            y = g_last_valid_y;
        }
        if (g_touch_mode == 2) {
            if (!g_dragging && inside_float(x, y)) {
                g_visible = g_visible ? 0 : 1;
                ++g_toggle_count;
            }
            g_touch_mode = 0;
            g_dragging = 0;
            return 1;
        }
        if (g_touch_mode == 3 || g_touch_mode == 5) {
            release_active(ms);
            g_touch_mode = 0;
            return 1;
        }
        if (g_touch_mode == 4) {
            g_touch_mode = 0;
            return 1;
        }
        g_touch_mode = 0;
        return 0;   /* 游戏区抬起交给内核 */
    }
    return 0;
}
