/* guitartuner-pref.c
 *
 * Copyright 2019 yzn <y4812@hotmail.com>
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


#include <gtk/gtk.h>

#include "guitartuner-prefs.h"
#include "guitartuner-window.h"

struct _GuitartunerPrefs
{
  GtkDialog    parent;
  GSettings   *settings;
  GtkWidget   *sensitivity_scale;
  GtkWidget   *accuracy_scale;
};

G_DEFINE_TYPE (GuitartunerPrefs, guitartuner_prefs, GTK_TYPE_DIALOG)

static void
guitartuner_prefs_dispose(GObject *obj)
{
  g_clear_object(&GUITARTUNER_PREFS(obj)->settings);
  G_OBJECT_CLASS (guitartuner_prefs_parent_class)->dispose(obj);
}

static void
guitartuner_prefs_class_init(GuitartunerPrefsClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = guitartuner_prefs_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/com/github/CkTD/guitartuner/guitartuner-prefs.ui");
  gtk_widget_class_bind_template_child (widget_class, GuitartunerPrefs, sensitivity_scale);
  gtk_widget_class_bind_template_child (widget_class, GuitartunerPrefs, accuracy_scale);
}

static void
guitartuner_prefs_init(GuitartunerPrefs *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_widget_show_all (GTK_WIDGET(self));
  self->settings = g_settings_new ("com.github.CkTD.guitartuner");
  g_settings_bind (self->settings, "sensitivity", self->sensitivity_scale, "value", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "accuracy", self->accuracy_scale, "value", G_SETTINGS_BIND_DEFAULT);
}

GuitartunerPrefs *guitartuner_prefs_new(GuitartunerWindow *win)
{
  return g_object_new(GUITARTUNER_TYPE_PREF, "transient-for", win, "use-header-bar", TRUE, NULL);
}
