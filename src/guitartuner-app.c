/* guitartuner-app.c
 *
 * Copyright 2019 Unknown
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
 */
#include <math.h>

#include "guitartuner-app.h"
#include "guitartuner-window.h"
#include "guitartuner-freq.h"

struct _GuitartunerApp
{
  GtkApplication     parent;
  GuitartunerFreq   *freq;
  GuitartunerWindow *window;
};

G_DEFINE_TYPE (GuitartunerApp, guitartuner_app, GTK_TYPE_APPLICATION)




static void guitartuner_app_handle_freq(GuitartunerFreq *obj, gdouble freq, gdouble mag_percent, GuitartunerApp *self)
{
  g_print("MSG FROM APP: freq: %f\n", freq);
  guitartuner_window_freq_change (self->window, freq, mag_percent);
}


static void guitartuner_app_activate(GApplication *app)
{
  GuitartunerApp *self = GUITARTUNER_APP (app);
  self->window = guitartuner_window_new(self);
  gtk_window_present (GTK_WINDOW (self->window));
}


static void guitartuner_app_class_init(GuitartunerAppClass * klass)
{
  G_APPLICATION_CLASS (klass)->activate = guitartuner_app_activate;
}

static void guitartuner_app_init(GuitartunerApp *self)
{
  self->freq = guitartuner_freq_new ();
  g_signal_connect (self->freq, "freq-event", (GCallback)guitartuner_app_handle_freq, self);
}


GuitartunerApp *guitartuner_app_new(void)
{
  return g_object_new(GUITARTUNER_TYPE_APP,
                      "application-id", "com.github.CkTD.guitartuner",
                      "flags", G_APPLICATION_FLAGS_NONE,
                      NULL);
}
