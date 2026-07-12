#ifndef UI_H
#define UI_H

#include <stdbool.h>

void spawn_window();
void main_ui_loop();
void destroy_ui();
int spawn_message_box(const char *title, const char *message);
void run(int buttonPressed, void (*action)(void));

#endif 
