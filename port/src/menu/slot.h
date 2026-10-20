/* Player save slots (slot.cpp), kept in SLOT.A2R. */
#ifndef A2_SLOT_H
#define A2_SLOT_H

#include "menu/a2types.h"

extern slot_t slot_list[SLOT_COUNT];
extern int active_slot;

int slot_check_index(int index);
int slot_check_level(int level);
void create_slot(int index, const char *name);
void clear_slot(int index);
void reset_slot(int index);
void init_slotdbase(void);
void save_slotdbase(void);
int get_last_saved_slot(void);
void set_active_slot(int index);
void update_active_slot_optie(void);
void update_active_slot_bediening(void);
const char *slot_naam_string(int index);
const char *slot_tijd_string(int index);
int slot_count_total_tijd(int index, int stages);
/* racer 0..4 for the rivals, 5 for the player. */
int slot_count_total_tijd_speler(int index, int racer);

#endif
