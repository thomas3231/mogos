#ifndef TIMER_H
#define TIMER_H

void timer_install(void);
unsigned int timer_get_ticks(void);
void timer_wait(unsigned int ticks);
void timer_handler_c(void);

unsigned int timer_get_uptime_seconds(void);
unsigned int timer_get_cpu_usage_percent(void);
void timer_set_idle_state(unsigned int is_idle);

#endif