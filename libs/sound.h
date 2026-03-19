#ifndef SOUND_H
#define SOUND_H

void sound_click(void);
void sound_error(void);

// Попытка выключить ПК через APM BIOS.
// Возвращает 0 если команда отправлена, иначе -1.
int apm_poweroff(void);

#endif

