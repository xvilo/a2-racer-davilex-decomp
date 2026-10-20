/* Option settings (optie.cpp) and controls (bedien.cpp), kept in OPTIE.A2R and
 * BEDIEN.A2R. */
#ifndef A2_OPTIE_H
#define A2_OPTIE_H

#include "menu/a2types.h"

extern opties_t opties;
extern bedien_t control;
extern int available_joy;

void init_optie(void);
void save_optie(void);
void init_bedien(void);
void save_bedien(void);

#endif
