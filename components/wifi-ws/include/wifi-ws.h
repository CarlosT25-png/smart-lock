#pragma once

void init_wifi(void);

void websocket_app_start(void (*lock_func)(void), void (*unlock_func)(void));