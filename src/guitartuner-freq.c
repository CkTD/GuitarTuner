/* guitartuner-freq.c
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

#include <gst/gst.h>
#include <glib-object.h>
#include <string.h>
#include <math.h>

#include "guitartuner-freq.h"

struct _GuitartunerFreq
{
  GObject     parent_instance;
  int         rate;           // resample audio to this rate befor analysis
  guint       spect_bands;    // see GstElement spectrum'property
  gint        threshold;      // see GstElement spectrum'property
  GstElement *pipeline, *pulsesrc, *tee;
  GstElement *audio_queue, *audio_convert1, *audio_resample1, *audio_sink;
  GstElement *app_queue, *audio_resample2, *capsfilter, *audio_convert3, *spectrum, *fake_sink;
  GstPad     *tee_audio_pad, *tee_app_pad;
  GstPad     *queue_audio_pad, *queue_app_pad;
  GstBus     *bus;
  GstCaps    *caps;
};

G_DEFINE_TYPE (GuitartunerFreq, guitartuner_freq, G_TYPE_OBJECT)

enum GUITARTUNERFREQ_SIGNALS
{
  FREQ_EVENT = 0,
  N_SIGNALS
};

static guint guitartuner_freq_signals[N_SIGNALS] = { 0 };


static void guitartuner_freq_dispose(GObject *obj)
{
  GuitartunerFreq *self = GUITARTUNER_FREQ (obj);
  gst_element_release_request_pad (self->tee, self->tee_audio_pad);
  gst_element_release_request_pad (self->tee, self->tee_app_pad);
  gst_object_unref (self->tee_audio_pad);
  gst_object_unref (self->tee_app_pad);

  /* Free resources */
  gst_element_set_state (self->pipeline, GST_STATE_NULL);
  gst_object_unref (self->pipeline);
}


static void guitartuner_freq_class_init(GuitartunerFreqClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = guitartuner_freq_dispose;
  
  gst_init (NULL, NULL);
  guitartuner_freq_signals[FREQ_EVENT] = g_signal_new(
    "freq-event",
    GUITARTUNER_TYPE_FREQ,
    G_SIGNAL_RUN_LAST,
    0,
    NULL,
    NULL,
    NULL,
    G_TYPE_NONE,
    2,
    G_TYPE_DOUBLE,
    G_TYPE_DOUBLE);
}

/* This function is called when an error message is posted on the bus */
static void pipeline_error_cb (GstBus *bus, GstMessage *msg, GuitartunerFreq *self) {
  GError *err;
  gchar *debug_info;

  /* Print error details on the screen */
  gst_message_parse_error (msg, &err, &debug_info);
  g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
  g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
  g_clear_error (&err);
  g_free (debug_info);
}


/* 
  This function execute Harmonic Product Spectrum on the result of FFT
  and return fundamental frequency
*/
static void hps(const GValue *magnitudes, GuitartunerFreq *self, gdouble *res_freq, gdouble *res_mag_percent) {
  //g_print("HPS, %d\n", self->rate);
  const GValue *mag;
  gint i;
  gdouble freq;
  gdouble *original_mag, *compress2_mag, *compress3_mag; 
  gdouble min_value = self->threshold * self->threshold * self->threshold;
  gint    min_index = 0;
  gdouble product;

  original_mag = (gdouble *)g_malloc(sizeof(gdouble) * self->spect_bands);
  compress2_mag = (gdouble *)g_malloc(sizeof(gdouble) * self->spect_bands / 2);
  compress3_mag = (gdouble *)g_malloc(sizeof(gdouble) * self->spect_bands / 3);

  // TODO: use only one loop and one buffer.
  for (i = 0; i < self->spect_bands && i < 4500; i++)
  {
    freq = (gdouble)self->rate / 2 / self->spect_bands * (i + 0.5);
    mag = gst_value_list_get_value (magnitudes , i);
    //g_print("Band %d, freq %f, magnitude %f\n", i, freq, g_value_get_float(mag));
    original_mag[i] = g_value_get_float(mag);
    if ((i + 1) % 2 == 0) {
      compress2_mag[(i + 1) / 2 -1] = (original_mag[i] + original_mag[i - 1]) / 2;
    }
    if ((i + 1) % 3 == 0) {
      compress3_mag[(i + 1) / 3 -1] = (original_mag[i] + original_mag[i - 1] + original_mag[i - 2]) / 3;
    }
  }

  for (i = 0; i < self->spect_bands / 3; i++) {
    product = original_mag[i] * compress2_mag[i] * compress3_mag[i];
    //g_print("i: %d freq: %f, mag: %f\n", i, (gdouble)self->rate / 2 / self->spect_bands * (i + 0.5), product);
    if (product > min_value) {
      min_index = i;
      min_value = product;
    }
  }
  
  freq = (gdouble)self->rate / 2 / self->spect_bands * (min_index + 0.5);
  //g_print("%d, %f, %f\n", min_index, min_value, freq);
  g_free(original_mag);
  g_free(compress2_mag);
  g_free(compress3_mag);

  *res_freq = freq;
  *res_mag_percent = 1 -  min_value / (self->threshold * self->threshold * self->threshold);
}

/* This function is called evertime spectrum element fininsh a FFT */
static void handle_spectrum(const GstStructure *s, GuitartunerFreq *self)
{
  /*
  GstClockTime timestamp = GST_CLOCK_TIME_NONE; 
  GstClockTime duration = GST_CLOCK_TIME_NONE;
  GstClockTime endtime = GST_CLOCK_TIME_NONE;
  gst_structure_get_clock_time (s, "running-time", &timestamp);
  gst_structure_get_clock_time (s, "duration", &duration);
  gst_structure_get_clock_time (s, "endtime", &endtime);
  g_print ("New spectrum message, timestamp %" GST_TIME_FORMAT 
                               ", duration %" GST_TIME_FORMAT
                               ", endtime %" GST_TIME_FORMAT ".\n",
      GST_TIME_ARGS (timestamp),
      GST_TIME_ARGS (duration),
      GST_TIME_ARGS (endtime));
  */
  const GValue *magnitudes;
  gdouble        freq, mag_percent;
  magnitudes = gst_structure_get_value (s, "magnitude");  

  hps(magnitudes, self, &freq, &mag_percent);

  g_signal_emit (self, guitartuner_freq_signals[FREQ_EVENT], 0, freq, mag_percent);

  //g_print("Freq: %f\n", freq);
}

static void pipeline_element_cb(GstBus *bus, GstMessage *msg, GuitartunerFreq *self) {
    const GstStructure *s = gst_message_get_structure (msg);
    const gchar *name = gst_structure_get_name (s);
    if (strcmp (name, "spectrum") == 0) {
      handle_spectrum(s, self);
    }
}


static void guitartuner_freq_init(GuitartunerFreq *self)
{
  self->spect_bands = 4500;
  self->rate = 3000;
  self->threshold = -80;
  
  self->pulsesrc = gst_element_factory_make ("pulsesrc", "audio_source");


  self->tee = gst_element_factory_make ("tee", "tee");
  self->audio_queue = gst_element_factory_make ("queue", "audio_queue");
  self->audio_convert1 = gst_element_factory_make ("audioconvert", "audio_convert1");
  self->audio_resample1 = gst_element_factory_make ("audioresample", "audio_resample1");
  self->audio_sink = gst_element_factory_make ("autoaudiosink", "audio_sink");


  self->app_queue = gst_element_factory_make ("queue", "app_queue");
  self->audio_convert3 = gst_element_factory_make ("audioconvert", "audio_convert3");
  self->audio_resample2 = gst_element_factory_make ("audioresample", "audio_resample2");
  self->capsfilter = gst_element_factory_make ("capsfilter", "filter");
  self->spectrum = gst_element_factory_make ("spectrum", "spectrum");
  self->fake_sink = gst_element_factory_make ("fakesink", "fake_sink");

  /* Create the empty pipeline */
  self->pipeline = gst_pipeline_new ("test-pipeline");


  if (!self->pipeline || !self->pulsesrc || !self->tee || 
      !self->audio_queue || !self->audio_convert1 || !self->audio_resample1 || !self->audio_sink || 
      !self->capsfilter || !self->spectrum || !self->fake_sink) {
    g_printerr ("Not all elements could be created.\n");
    return;
  }

  /* Configure resample */
  self->caps = gst_caps_new_simple ("audio/x-raw", "rate", G_TYPE_INT, self->rate, NULL);
  g_object_set (self->capsfilter, "caps", self->caps, NULL);
  
  /* Configure spectrum */
  g_object_set (G_OBJECT (self->spectrum), "bands", self->spect_bands, "interval", 100000000,"threshold", self->threshold, "post-messages", TRUE, "message-phase", TRUE, NULL);

  gst_bin_add_many (GST_BIN (self->pipeline), self->pulsesrc, self->tee, 
      self->audio_queue, self->audio_convert1, self->audio_resample1, self->audio_sink,
      self->app_queue, self->audio_convert3, self->audio_resample2, self->capsfilter, self->spectrum, self->fake_sink, NULL);
  /* Link all elements that can be automatically linked because they have "Always" pads */
  if (!gst_element_link_many (self->pulsesrc, self->tee, NULL) ||
      !gst_element_link_many (self->audio_queue, self->audio_convert1, self->audio_resample1, self->audio_sink, NULL) ||
      !gst_element_link_many (self->app_queue, self->audio_convert3,  self->audio_resample2, self->capsfilter, self->spectrum, self->fake_sink, NULL)) {
    g_printerr ("Elements could not be linked.\n");
    gst_object_unref (self->pipeline);
    return;
  }

  /* Manually link the Tee, which has "Request" pads */
  self->tee_audio_pad = gst_element_get_request_pad (self->tee, "src_%u");
  //g_print ("Obtained request pad %s for audio branch.\n", gst_pad_get_name (tee_audio_pad));
  self->queue_audio_pad = gst_element_get_static_pad (self->audio_queue, "sink");
  self->tee_app_pad = gst_element_get_request_pad (self->tee, "src_%u");
  //g_print ("Obtained request pad %s for app branch.\n", gst_pad_get_name (tee_app_pad));
  self->queue_app_pad = gst_element_get_static_pad (self->app_queue, "sink");
  if (gst_pad_link (self->tee_audio_pad, self->queue_audio_pad) != GST_PAD_LINK_OK ||
      gst_pad_link (self->tee_app_pad, self->queue_app_pad) != GST_PAD_LINK_OK) {
    g_printerr ("Tee could not be linked\n");
    gst_object_unref (self->pipeline);
    return;
  }
  
  gst_object_unref (self->queue_audio_pad);
  gst_object_unref (self->queue_app_pad);

  /* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
  self->bus = gst_element_get_bus (self->pipeline);
  gst_bus_add_signal_watch (self->bus);
  g_signal_connect (G_OBJECT (self->bus), "message::error", (GCallback)pipeline_error_cb, self);
  g_signal_connect (G_OBJECT (self->bus), "message::element", (GCallback)pipeline_element_cb, self);
  gst_object_unref (self->bus);

  /* Start playing the pipeline */

  gst_element_set_state (self->pipeline, GST_STATE_PLAYING);
  return ;
}



GuitartunerFreq *guitartuner_freq_new(void)
{
  return g_object_new(GUITARTUNER_TYPE_FREQ,NULL);
}
