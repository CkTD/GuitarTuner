

#include "guitartuner-note.h"

#include <math.h>

gdouble standard_notes[NO_OCTAVE * NO_NOTES_PER_OCTAVE];

const char* notes_to_name[] = {
  "C",
  "C#",
  "D",
  "D#",
  "E",
  "F",
  "F#",
  "G",
  "G#",
  "A",
  "A#",
  "B"
};

void standard_notes_init()
{
  int    i;
  gfloat base;

  base = pow(2.0, 1.0/12.0);

  // calculate freq of notes in octave 4, according to standard pitch A4
  for (i = 0; i < NO_NOTES_PER_OCTAVE ; i++) {
    STANDARD_NOTES_FREQ(4, i) = A440 * pow(base, (i - 9));
  }

  // calculate freqs of note in octave 3,2,1,0, according octave 4
  for (i = 0; i < NO_NOTES_PER_OCTAVE; i++) {
    STANDARD_NOTES_FREQ(3, i) = STANDARD_NOTES_FREQ(4, i) / 2;
    STANDARD_NOTES_FREQ(2, i) = STANDARD_NOTES_FREQ(4, i) / 4;
    STANDARD_NOTES_FREQ(1, i) = STANDARD_NOTES_FREQ(4, i) / 8;
    STANDARD_NOTES_FREQ(0, i) = STANDARD_NOTES_FREQ(4, i) / 16;
  }

  // --------------- 
  /*
  g_print ("Standard frequency table");
  for (i = 0;i < NO_OCTAVE * NO_NOTES_PER_OCTAVE; i ++) {
    if (i && (i % NO_NOTES_PER_OCTAVE == 0))
     g_print ("\n");
    g_print ("%f\t", standard_notes[i]);
  }
  g_print ("\n");
  */
  // ----------------
}




void standard_notes_nearest_note(gdouble freq, gint *octave, gint *note, gdouble *bias) 
{
  int      i, nearest_index;
  gdouble  mid_freq;

  i = 0;
  while(freq > standard_notes[i] && i < NO_NOTES_PER_OCTAVE * NO_OCTAVE - 1)
    i++;
  if(i == 0) {
    nearest_index = i;
    *bias = (standard_notes[0] - freq) / standard_notes[0];
  }
  else {
    mid_freq = standard_notes[i] * 1 / pow(2.0, 1.0/24.0);
    if (freq > mid_freq) {
      nearest_index = i;
      *bias = -1;
    }
    else {
      nearest_index = i - 1;
      *bias = 1;
    }
    *bias *= (freq - standard_notes[nearest_index]) / (mid_freq - standard_notes[nearest_index]);
  }
                                                                                          
  *octave = nearest_index / NO_NOTES_PER_OCTAVE;
  *note =  nearest_index % NO_NOTES_PER_OCTAVE; 
  g_assert (*octave < NO_OCTAVE);
  g_assert (*note < NO_NOTES_PER_OCTAVE);
}
