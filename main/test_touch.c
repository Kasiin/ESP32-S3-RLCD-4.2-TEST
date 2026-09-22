// Reuse the parent project's touch driver without linking its app/input stack.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "touch_panel.h"
#include "test_touch.h"

static const char *TAG = "TEST_TOUCH";
static uint8_t trail[ST7305_WIDTH * ST7305_HEIGHT / 8];
static const int targets[5][2] = {{20,20},{379,20},{200,150},{20,279},{379,279}};
static tp_point_t previous;
static int sx, sy;
static uint32_t contacts, good_reads, bad_reads, invalid_points;
static unsigned hits;
static bool previous_valid, stale;
static int64_t last_good, last_log;

const char *test_touch_name(void)
{
    switch (touch_panel_get_chip()) {
        case TP_CHIP_CST816: return "CST816";
        case TP_CHIP_GT911: return "GT911";
        case TP_CHIP_FT6236: return "FT6236";
        default: return "NOT DETECTED";
    }
}

void test_touch_clear(void)
{
    memset(trail, 0, sizeof(trail));
    hits = contacts = invalid_points = 0;
    previous_valid = false; // Do not join a new stroke to an old point.
    ESP_LOGI(TAG, "Trail and five-point test cleared");
}

void test_touch_init(void)
{
    touch_panel_init();
    test_touch_clear();
    last_good = esp_timer_get_time();
    test_touch_report();
}

void test_touch_report(void)
{
    int rx, ry;
    touch_panel_get_resolution(&rx, &ry);
    ESP_LOGI(TAG, "%s raw resolution=%dx%d SDA=%d SCL=%d INT=%d RST=%d; I2C1 independent of audio",
             test_touch_name(), rx, ry, TP_SDA_PIN, TP_SCL_PIN, TP_INT_PIN, TP_RST_PIN);
    ESP_LOGI(TAG, "reads_ok=%" PRIu32 " failed=%" PRIu32 " out_of_range=%" PRIu32
             " contacts=%" PRIu32 " targets=0x%02x INT=%d stale=%d",
             good_reads, bad_reads, invalid_points, contacts, hits, gpio_get_level(TP_INT_PIN), stale);
}

static void dot(int x, int y)
{
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        int px = x + dx, py = y + dy;
        if (px < 0 || px >= ST7305_WIDTH || py < 0 || py >= ST7305_HEIGHT) continue;
        unsigned bit = py * ST7305_WIDTH + px;
        trail[bit / 8] |= 0x80 >> (bit % 8);
    }
}

static void stroke(int x0, int y0, int x1, int y1)
{
    int dx = abs(x1-x0), dy = -abs(y1-y0), ex = x0 < x1 ? 1 : -1, ey = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    while (true) {
        dot(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int twice = 2 * error;
        if (twice >= dy) { error += dy; x0 += ex; }
        if (twice <= dx) { error += dx; y0 += ey; }
    }
}

bool test_touch_poll(void)
{
    if (!touch_panel_is_present()) return false;
    tp_point_t point = {0};
    int64_t now = esp_timer_get_time();
    if (!touch_panel_read(&point)) {
        bad_reads++;
        // Failure is not a valid finger-up event; mark stale and break the trail.
        if (now - last_good > 250000 && !stale) {
            stale = true;
            previous_valid = false;
            ESP_LOGW(TAG, "Touch data stale; check driver recovery/I2C logs");
            return true;
        }
        return false;
    }
    good_reads++;
    last_good = now;
    bool changed = stale || !previous_valid || point.pressed != previous.pressed;
    stale = false;
    if (point.pressed) {
        int rx, ry;
        touch_panel_get_resolution(&rx, &ry);
        if (rx <= 0 || ry <= 0 || point.x < 0 || point.y < 0 || point.x >= rx || point.y >= ry) {
            invalid_points++;
            previous_valid = false;
            if (now - last_log >= 250000) {
                ESP_LOGW(TAG, "Out-of-range raw=%d,%d resolution=%dx%d", point.x, point.y, rx, ry);
                last_log = now;
            }
            return true;
        }
        int nx = (int32_t)point.x * ST7305_WIDTH / rx;
        int ny = (int32_t)point.y * ST7305_HEIGHT / ry;
        bool down = !previous_valid || !previous.pressed;
        if (down) contacts++;
        changed |= down || point.x != previous.x || point.y != previous.y;
        // Do not hide noisy large jumps with a long interpolated line.
        if (!down && abs(nx-sx) < 80 && abs(ny-sy) < 80) stroke(sx, sy, nx, ny);
        else dot(nx, ny);
        sx = nx; sy = ny;
        for (int i = 0; i < 5; i++) {
            if (abs(sx-targets[i][0]) <= 18 && abs(sy-targets[i][1]) <= 18 && !(hits & (1U << i))) {
                hits |= 1U << i;
                ESP_LOGI(TAG, "TARGET %d reached (%d,%d), mask=0x%02x", i+1, sx, sy, hits);
                changed = true;
            }
        }
        if (down || (changed && now - last_log >= 150000)) {
            ESP_LOGI(TAG, "%s raw=%d,%d screen=%d,%d INT=%d", down ? "DOWN" : "MOVE",
                     point.x, point.y, sx, sy, gpio_get_level(TP_INT_PIN));
            last_log = now;
        }
    } else if (previous_valid && previous.pressed) {
        ESP_LOGI(TAG, "UP at screen=%d,%d", sx, sy);
    }
    previous = point;
    previous_valid = true;
    return changed;
}

void test_touch_draw(st7305_handle_t *lcd)
{
    // Draw the raw trace behind labels. No app gestures, smoothing or calibration.
    for (int y = 0; y < ST7305_HEIGHT; y++) for (int x = 0; x < ST7305_WIDTH; x++) {
        unsigned bit = y * ST7305_WIDTH + x;
        if (trail[bit / 8] & (0x80 >> (bit % 8))) st7305_draw_pixel(lcd, x, y, ST7305_COLOR_BLACK);
    }
    for (int i = 0; i < 5; i++) {
        int x = targets[i][0], y = targets[i][1];
        for (int d = -12; d <= 12; d++) {
            st7305_draw_pixel(lcd,x+d,y-12,0); st7305_draw_pixel(lcd,x+d,y+12,0);
            st7305_draw_pixel(lcd,x-12,y+d,0); st7305_draw_pixel(lcd,x+12,y+d,0);
            if (hits & (1U << i)) { st7305_draw_pixel(lcd,x+d,y+d,0); st7305_draw_pixel(lcd,x+d,y-d,0); }
        }
        char digit[2] = {(char)('1'+i), 0};
        st7305_draw_text_outlined(lcd,x-5,y-7,digit);
    }
    char line[64];
    snprintf(line,sizeof(line),"TOUCH: %s",test_touch_name());
    st7305_draw_text_outlined(lcd,8,48,line);
    snprintf(line,sizeof(line),"RAW %d,%d  LCD %d,%d",previous.x,previous.y,sx,sy);
    st7305_draw_text_outlined(lcd,8,72,line);
    unsigned count = 0;
    for (int i = 0; i < 5; i++) if (hits & (1U << i)) count++;
    snprintf(line,sizeof(line),"%s  TOUCHES:%" PRIu32 "  HIT:%u/5",
             stale ? "STALE" : (previous_valid && previous.pressed ? "DOWN" : "UP"),contacts,count);
    st7305_draw_text_outlined(lcd,8,96,line);
    st7305_draw_text_outlined(lcd,8,190,"TAP 1-5; DRAG TO DRAW");
    st7305_draw_text_outlined(lcd,8,214,"USB C:CLEAR T:STATUS/TOUCH");
    st7305_draw_text_outlined(lcd,8,238,"BOOT:NEXT KEY:BEEP");
}
