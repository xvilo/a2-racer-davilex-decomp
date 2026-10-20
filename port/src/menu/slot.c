#include "menu/slot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "menu/optie.h"
#include "xvga/fileio.h"

slot_t slot_list[SLOT_COUNT];
int active_slot;
static int slots_already_loaded;

int slot_check_index(int index)
{
	if (index < 0 || index >= SLOT_COUNT) {
		fprintf(stderr, "slot_check_index: unknown slot %d\n", index);
		abort();
	}
	return index;
}

int slot_check_level(int level)
{
	if (level < 0 || level >= STAGE_COUNT) {
		fprintf(stderr, "slot_check_level: unknown stage %d\n", level);
		abort();
	}
	return level;
}

/* Per-stage results of a slot that has not raced yet: rivals 1..5 in order. */
static void clear_results(slot_t *s)
{
	for (int i = 0; i < STAGE_COUNT; i++) {
		s->time[i] = 0;
		s->position[i] = 6;
		for (int r = 0; r < 5; r++) {
			s->racer[r][i] = (int16_t)(r + 1);
			s->time_racer[r][i] = 0;
		}
	}
}

void create_slot(int index, const char *name)
{
	slot_check_index(index);
	/* The new slot becomes the most recently saved one. */
	unsigned last = 0;
	for (int i = 0; i < SLOT_COUNT; i++) {
		if (slot_list[i].save_order > last && i != index)
			last = slot_list[i].save_order;
	}
	slot_t *s = &slot_list[index];
	strncpy(s->name, name, sizeof s->name);
	s->save_order = (uint8_t)(last + 1);
	s->stage = 0;
	s->used = 1;
	s->money = 1000;
	s->opties = opties;
	s->control = control;
	clear_results(s);
	slots_already_loaded = 1;
}

void clear_slot(int index)
{
	slot_check_index(index);
	for (int i = 0; i < SLOT_COUNT; i++) {
		if (slot_list[i].save_order > slot_list[index].save_order)
			slot_list[i].save_order--;
	}
	slot_t *s = &slot_list[index];
	s->used = 0;
	s->save_order = 0;
	s->money = 0;
	clear_results(s);
}

void reset_slot(int index)
{
	slot_t *s = &slot_list[slot_check_index(index)];
	s->save_order = 0;
	s->stage = 0;
	s->money = 1000;
	s->opties = opties;
	s->control = control;
	clear_results(s);
}

void save_slotdbase(void)
{
	if (file_write(slot_list, sizeof slot_list, "SLOT.A2R"))
		fprintf(stderr, "save_slotdbase: cannot write %s\n", save_path("SLOT.A2R"));
}

void init_slotdbase(void)
{
	if (!file_exist("SLOT.A2R") || file_read(slot_list, sizeof slot_list, "SLOT.A2R")) {
		file_delete("SLOT.A2R");
		memset(slot_list, 0, sizeof slot_list);
		for (int i = 0; i < SLOT_COUNT; i++) {
			slot_list[i].opties = opties;
			slot_list[i].control = control;
		}
		save_slotdbase();
	}
	slots_already_loaded = 1;
}

int get_last_saved_slot(void)
{
	int best = 0;
	unsigned order = 0;
	if (!slots_already_loaded)
		return 0;
	for (int i = 0; i < SLOT_COUNT; i++) {
		if (slot_list[i].used && slot_list[i].save_order > order) {
			order = slot_list[i].save_order;
			best = i;
		}
	}
	return best;
}

void set_active_slot(int index)
{
	slot_t *s = &slot_list[slot_check_index(index)];
	if (!s->used) {
		fprintf(stderr, "set_active_slot: slot %d is not in use\n", index);
		abort();
	}
	opties = s->opties;
	control = s->control;
	active_slot = index;
}

void update_active_slot_optie(void)
{
	slot_t *s = &slot_list[slot_check_index(active_slot)];
	if (s->used)
		s->opties = opties;
}

void update_active_slot_bediening(void)
{
	slot_t *s = &slot_list[slot_check_index(active_slot)];
	if (s->used)
		s->control = control;
}

const char *slot_naam_string(int index)
{
	slot_t *s = &slot_list[slot_check_index(index)];
	return s->used ? s->name : "L E E G"; /* "empty" */
}

const char *slot_level_string(int index)
{
	extern const char *level_naam[10];
	slot_t *s = &slot_list[slot_check_index(index)];
	return s->used ? level_naam[s->stage] : "";
}

int slot_count_total_tijd(int index, int stages)
{
	slot_t *s = &slot_list[slot_check_index(index)];
	int total = 0;
	if (s->used) {
		for (int i = 0; i < stages; i++)
			total += s->time[slot_check_level(i)];
	}
	return total;
}

int slot_count_total_tijd_speler(int index, int racer)
{
	slot_t *s = &slot_list[slot_check_index(index)];
	int total = 0;
	if (!s->used)
		return 0;
	for (int i = 0; i < s->stage; i++)
		total += racer == 5 ? s->time[slot_check_level(i)] : s->time_racer[racer][slot_check_level(i)];
	return total;
}

/* "HHuMMmSSs": hours (uur), minutes and seconds of the player's total time. */
const char *slot_tijd_string(int index)
{
	static char slot_str_buffer[32];
	slot_t *s = &slot_list[slot_check_index(index)];
	if (!s->used)
		return "--u--m--s";
	int t = slot_count_total_tijd(index, s->stage);
	snprintf(slot_str_buffer, sizeof slot_str_buffer, "%02du%02dm%02ds", t / 3600, t / 60 % 60,
		t % 60);
	return slot_str_buffer;
}
