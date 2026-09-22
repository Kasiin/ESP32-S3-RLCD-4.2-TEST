#pragma once
#include <stdbool.h>
#include "st7305.h"

void test_touch_init(void);
bool test_touch_poll(void); // True when diagnostics/coordinates change.
void test_touch_draw(st7305_handle_t *lcd);
void test_touch_clear(void);
void test_touch_report(void);
const char *test_touch_name(void);
