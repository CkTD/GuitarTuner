/* guitartuner-note.h
 *
 * Copyright 2019 Unknown <unknown@domain.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// frequency for standard pitch
// we detect note range from  C0 - B4, which below 500 HZ

#pragma once
#include <glib-2.0/glib.h>

#define NO_NOTES_PER_OCTAVE  12       // 12 notes per octave
#define NO_OCTAVE            5        // 5 octaves
#define A440                 440.0    // standard frequency of note A4


#define STANDARD_NOTES_FREQ(octave, note) (standard_notes[octave * NO_NOTES_PER_OCTAVE + note])  


enum TUNINGS
{
  AUTO,
  STANDARD,
  DROP_D,
  OPEN_D,
};


extern gdouble standard_notes[NO_OCTAVE * NO_NOTES_PER_OCTAVE];
extern const char* notes_to_name[NO_NOTES_PER_OCTAVE];
void standard_notes_nearest_note(gdouble freq, gint *octave, gint *note, gdouble *bias);
void standard_notes_init();
