/* guitartuner-window.c
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

#include "guitartuner-config.h"
#include "guitartuner-window.h"
#include "guitartuner-app.h"
#include "guitartuner-note.h"
#include "guitartuner-prefs.h"

#include <glib-object.h>

struct _GuitartunerWindow
{
  GtkApplicationWindow  parent_instance;

  GSettings           *settings;

  GtkWidget           *header_bar;
  GtkWidget           *prefs_button;
  GtkWidget           *stack;
  //GtkWidget           *auto_box;
  GtkWidget           *auto_message_head;
  GtkWidget           *auto_grid;
  GtkWidget           *auto_notes[NO_OCTAVE][NO_NOTES_PER_OCTAVE];
  GtkWidget           *auto_message_head_box;
  GtkWidget           *auto_message_head_nothing;
  GtkWidget           *auto_message_label_sharp;
  GtkWidget           *auto_message_label_octave;
  GtkWidget           *auto_message_label_note;
  GtkWidget           *auto_message_detail;
  GtkWidget           *auto_message_suggestion;
  GtkWidget           *manual_paned;

  GtkWidget           *current_note;
  gdouble              sensitivity;
  gdouble              accuracy;

};

G_DEFINE_TYPE (GuitartunerWindow, guitartuner_window, GTK_TYPE_APPLICATION_WINDOW)

enum {
  PROP_SENSITIVITY = 1,
  PROP_ACCURACY,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = {NULL, };
static void
guitartuner_window_set_property (GObject       *obj,
                                 guint          property_id,
                                 const GValue  *value,
                                 GParamSpec    *pspec)
{
  GuitartunerWindow *self = GUITARTUNER_WINDOW(obj);

  switch (property_id){
    case PROP_SENSITIVITY:
      self->sensitivity = g_value_get_double (value);
      break;
    case PROP_ACCURACY:
      self->accuracy = g_value_get_double (value);
      break;
  }
}

static void
guitartuner_window_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GuitartunerWindow *self = GUITARTUNER_WINDOW (object);

  switch (property_id)
    {
    case PROP_SENSITIVITY:
      g_value_set_double (value, self->sensitivity);
      break;

    case PROP_ACCURACY:
      g_value_set_double (value, self->accuracy);
      break;
    }
}

static void
guitartuner_window_dispose(GObject *obj)
{
  g_clear_object(&GUITARTUNER_WINDOW(obj)->settings);
  G_OBJECT_CLASS (guitartuner_window_parent_class)->dispose(obj);
}

static void set_css(void)
{
  GtkCssProvider *provider;
  GdkDisplay *display;
  GdkScreen *screen;

  provider = gtk_css_provider_new ();
  display = gdk_display_get_default ();
  screen = gdk_display_get_default_screen (display);
  gtk_style_context_add_provider_for_screen (screen, GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  gtk_css_provider_load_from_resource (provider, "/com/github/CkTD/guitartuner/style.css");
  g_object_unref (provider);
}

static void
guitartuner_window_class_init (GuitartunerWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  standard_notes_init ();
  set_css ();

  gtk_widget_class_set_template_from_resource (widget_class, "/com/github/CkTD/guitartuner/guitartuner-window.ui");
  gtk_widget_class_bind_template_child (widget_class, GuitartunerWindow, header_bar);
  gtk_widget_class_bind_template_child (widget_class, GuitartunerWindow, prefs_button);
  gtk_widget_class_bind_template_child (widget_class, GuitartunerWindow, stack);

  object_class->set_property = guitartuner_window_set_property;
  object_class->get_property = guitartuner_window_get_property;
  object_class->dispose = guitartuner_window_dispose;
  obj_properties[PROP_SENSITIVITY] =
    g_param_spec_double ("sensitivity",
                         "sensitivity",
                         "The sensitivity",
                         0,
                         1,
                         0.75,
                         G_PARAM_READWRITE);
  obj_properties[PROP_ACCURACY] =
    g_param_spec_double ("accuracy",
                         "accuracy",
                         "The accuracy",
                         0,
                         1,
                         0.1,
                         G_PARAM_READWRITE);
  g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}


static void 
note_grid_set_text(gint octave, gint note, GtkWidget *label_note, GtkWidget *label_sharp, GtkWidget *label_octave)
{
  char buffer_note_mane[8];
  char buffer_note_octave[8];
  char *buffer_note_sharp;
  g_snprintf (buffer_note_octave,8, "%d", octave);
  buffer_note_mane[0] = notes_to_name[note][0];
  buffer_note_mane[1] = '\0';
  buffer_note_sharp = notes_to_name[note][1] == '#' ? "#" : "";
  gtk_label_set_text(GTK_LABEL(label_note), buffer_note_mane);
  gtk_label_set_text(GTK_LABEL(label_sharp), buffer_note_sharp);
  gtk_label_set_text (GTK_LABEL(label_octave), buffer_note_octave);
}

static void
pref_button_clicked   (GtkButton *button,
                       gpointer  *user_data)
{
  GuitartunerPrefs *prefs;
  //GtkWindow *win;

  //win = gtk_application_get_active_window (GTK_APPLICATION (app));
  prefs = guitartuner_prefs_new (GUITARTUNER_WINDOW(user_data));
  gtk_window_present (GTK_WINDOW (prefs));
}

static void
guitartuner_window_init (GuitartunerWindow *self)
{

  self->settings = g_settings_new ("com.github.CkTD.guitartuner");
  g_settings_bind (self->settings, "sensitivity", self, "sensitivity", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "accuracy", self, "accuracy", G_SETTINGS_BIND_DEFAULT);
  
  GtkBuilder *builder;
  GtkWidget *note_name , *note_sharp, *note_octave;
  int i, j;
  
  //self->sensitivity = 0.5;
  //self->accuracy = 0.1;
  self->current_note = NULL;
  
  gtk_widget_init_template (GTK_WIDGET (self));
  g_signal_connect (self->prefs_button, "clicked", G_CALLBACK (pref_button_clicked), self);
  builder = gtk_builder_new_from_resource ("/com/github/CkTD/guitartuner/guitartuner-stack.ui");
  self->auto_message_head = GTK_WIDGET(gtk_builder_get_object (builder, "auto_message_head"));
  self->auto_grid = GTK_WIDGET(gtk_builder_get_object (builder, "auto_grid"));
  self->auto_message_head_box = GTK_WIDGET(gtk_builder_get_object (builder, "auto_message_head_box"));
  gtk_widget_show_all (self->auto_message_head_box);
  g_object_ref(self->auto_message_head_box);
  self->auto_message_head_nothing = GTK_WIDGET(gtk_builder_get_object (builder, "auto_message_head_nothing"));
  gtk_widget_set_name (self->auto_message_head_nothing, "message_nothing");
  g_object_ref(self->auto_message_head_nothing);
  self->auto_message_label_note = GTK_WIDGET(gtk_builder_get_object (builder, "auto_message_label_note"));
  self->auto_message_label_sharp = GTK_WIDGET(gtk_builder_get_object (builder, "auto_message_label_sharp"));
  self->auto_message_label_octave = GTK_WIDGET(gtk_builder_get_object (builder, "auto_message_label_octave"));
  self->auto_message_suggestion = GTK_WIDGET(gtk_builder_get_object (builder, "auto_message_suggestion"));
  self->auto_message_detail = GTK_WIDGET(gtk_builder_get_object (builder, "auto_message_detail"));
  gtk_widget_set_name (self->auto_message_label_note, "message_note_name");
  gtk_widget_set_name (self->auto_message_label_sharp, "message_note_script");
  gtk_widget_set_name (self->auto_message_label_octave, "message_note_script");
  gtk_widget_set_name (self->auto_message_suggestion, "message_note_suggestion");

  
  for (i = 0; i< NO_OCTAVE; i++)
  {
    for (j = 0; j< NO_NOTES_PER_OCTAVE; j++)
    {   
      self->auto_notes[i][j] = gtk_grid_new ();
      gtk_widget_set_name (self->auto_notes[i][j], "note_grid");
      note_name = gtk_label_new(NULL);
      note_sharp = gtk_label_new(NULL);
      note_octave = gtk_label_new(NULL);
      note_grid_set_text(i, j, note_name, note_sharp, note_octave);
      gtk_widget_set_name (note_name, "note_name");
      gtk_widget_set_name (note_sharp, "note_script");
      gtk_widget_set_name (note_octave, "note_script");
      gtk_grid_attach (GTK_GRID(self->auto_notes[i][j]), note_name, 0, 0, 1, 2);
      gtk_grid_attach (GTK_GRID(self->auto_notes[i][j]), note_sharp, 1, 0, 1, 1);
      gtk_grid_attach (GTK_GRID(self->auto_notes[i][j]), note_octave, 1, 1, 1, 1);  
      gtk_grid_attach (GTK_GRID(self->auto_grid), self->auto_notes[i][j], j, i+3, 1, 1);
    }
  }
  gtk_widget_show_all (self->auto_grid);
  gtk_stack_add_titled (GTK_STACK(self->stack), self->auto_grid, "Auto","Auto");
  
  
  self->manual_paned = GTK_WIDGET (gtk_builder_get_object (builder, "manual_paned"));
  gtk_widget_show_all (self->manual_paned);
  gtk_stack_add_titled (GTK_STACK(self->stack), self->manual_paned, "Manual", "Manual");
  
  g_object_unref (builder);
}
void guitartuner_window_freq_change(GuitartunerWindow *self, gdouble freq, gdouble mag_percent)
{

  gint octave, note;
  gdouble bias;
  char detail_buffer[64];
  const char *suggestion;
  static const char *suggestions[] = {
    "Too sharp, tune down!",
    "Too flat, tune up!",
    "You are in tune!"
  };
  //g_print("Accu: %f, sensi: %f\n", self->sensitivity, self->accuracy);
  
  if(mag_percent < 1 - self->sensitivity) {
    if (self->current_note) {
      gtk_widget_set_name (self->current_note, "note_grid");
      gtk_container_remove (GTK_CONTAINER(self->auto_message_head), self->auto_message_head_box);
      gtk_box_pack_start (GTK_BOX(self->auto_message_head), self->auto_message_head_nothing, FALSE, FALSE, 0);
      self->current_note = NULL;
    }
  }
  else {
    if (!self->current_note) {

      gtk_container_remove (GTK_CONTAINER(self->auto_message_head), self->auto_message_head_nothing);
      gtk_box_pack_start (GTK_BOX(self->auto_message_head), self->auto_message_head_box, FALSE, FALSE, 0);
    }
         
    standard_notes_nearest_note (freq, &octave, &note, &bias);
    if(self->current_note) {
      gtk_widget_set_name (self->current_note, "note_grid");
    }
    gtk_widget_set_name (self->auto_notes[octave][note], "note_grid_current"); 
    self->current_note = self->auto_notes[octave][note];
    if (bias > self->accuracy) {
      suggestion = suggestions[0];
    }
    else if (bias < -1 * self->accuracy) {
      suggestion = suggestions[1];
    }
    else {
      suggestion = suggestions[2];
    }
    g_snprintf (detail_buffer, 64, "%3.2fHz | %3.2fHz | %3.2f%%", freq, STANDARD_NOTES_FREQ(octave, note) ,bias*100);
    gtk_label_set_text (GTK_LABEL(self->auto_message_suggestion), suggestion);
    gtk_label_set_text (GTK_LABEL(self->auto_message_detail), detail_buffer);
    note_grid_set_text(octave, note, self->auto_message_label_note, self->auto_message_label_sharp, self->auto_message_label_octave);
  }
}


GuitartunerWindow *
guitartuner_window_new(GuitartunerApp *app)
{
  return g_object_new(GUITARTUNER_TYPE_WINDOW,
                      "application", app,
                      NULL);
}
