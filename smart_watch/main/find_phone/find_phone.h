#ifndef FIND_PHONE_H
#define FIND_PHONE_H

#include <stdbool.h>

extern volatile bool find_phone_screen_active;
extern volatile bool find_phone_ringing;

void find_phone_init(void);
void show_find_phone_screen(void);
void hide_find_phone_screen(void);
void toggle_find_phone_ring(void);

#endif // FIND_PHONE_H