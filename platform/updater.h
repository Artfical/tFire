#ifndef UPDATER_H
#define UPDATER_H
#include "uiapp.h"
void updater_start(void);          /* asks the update server (in the background) whether there is a newer tFire */
void updater_poll(uiapp_t *a);     /* called every frame: shows what the check found */
#endif
