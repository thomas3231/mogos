#ifndef TIMER_H
#define TIMER_H

void timer_install(void);
unsigned int timer_get_ticks(void);
void timer_wait(unsigned int ticks);

#endif