#ifndef SPEAKER_H
#define SPEAKER_H

void beep(unsigned int frequency);
void nosound(void);
void play_note(unsigned int freq, unsigned int duration_ticks, unsigned int gap_ticks);
void play_boot_jingle(void);

#endif