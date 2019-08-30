#include <gst/gst.h>
//#include <gst/audio/audio.h>
#include <string.h>
#include <math.h>


// frequency for standard pitch
// we detect note range from  C0 - B4, which below 500 HZ

#define NO_NOTES_PER_OCTAVE  12       // 12 notes per octave
#define NO_OCTAVE            5        // 5 octaves
#define A440                 440.0    // standard frequency of note A4

static gdouble standard_notes[NO_OCTAVE * NO_NOTES_PER_OCTAVE];

#define OCTAVE_0             0
#define OCTAVE_1             1
#define OCTAVE_2             2
#define OCTAVE_3             3
#define OCTAVE_4             4

#define NOTE_C               0
#define NOTE_CS              1
#define NOTE_D               2
#define NOTE_DS              3
#define NOTE_E               4
#define NOTE_F               5
#define NOTE_FS              6
#define NOTE_G               7
#define NOTE_GS              8
#define NOTE_A               9
#define NOTE_AS              10
#define NOTE_B               11

#define STANDARD_NOTES_FREQ(octave, note) (standard_notes[octave * NO_NOTES_PER_OCTAVE + note])  

static void standard_notes_init()
{
  int    i;
  gfloat base;

  base = pow(2.0, 1.0/12.0);

  // calculate freq of notes in octave 4, according to standard pitch A4
  for (i = 0; i < NO_NOTES_PER_OCTAVE ; i++) {
    STANDARD_NOTES_FREQ(OCTAVE_4, i) = A440 * pow(base, (i - NOTE_A));
  }

  // calculate freqs of note in octave 3,2,1,0, according octave 4
  for (i = NOTE_C; i <= NOTE_B; i++) {
    STANDARD_NOTES_FREQ(OCTAVE_3, i) = STANDARD_NOTES_FREQ(OCTAVE_4, i) / 2;
    STANDARD_NOTES_FREQ(OCTAVE_2, i) = STANDARD_NOTES_FREQ(OCTAVE_4, i) / 4;
    STANDARD_NOTES_FREQ(OCTAVE_1, i) = STANDARD_NOTES_FREQ(OCTAVE_4, i) / 8;
    STANDARD_NOTES_FREQ(OCTAVE_0, i) = STANDARD_NOTES_FREQ(OCTAVE_4, i) / 16;
  }

  // --------------- 
  /* 
  g_print ("Standard frequency table")
  for (i = 0;i < NO_OCTAVE * NO_NOTES_PER_OCTAVE; i ++) {
    if (i && (i % NO_NOTES_PER_OCTAVE == 0))
     g_print ("\n");
    g_print ("%f\t", standard_notes[i]);
  }
  g_print ("\n");
  */
  // ----------------
}

static void standard_notes_index_to_name(gint index, gint *octave, gchar **note) {
  gint relative;
  *octave = index / NO_NOTES_PER_OCTAVE;
  switch (index % NO_NOTES_PER_OCTAVE)
  {
    case NOTE_C:
      *note = "C";
      break;
    case NOTE_CS:
      *note = "C#";
      break;
    case NOTE_D:
      *note = "D";
      break;
    case NOTE_DS:
      *note = "D#";
      break;
    case NOTE_E:
      *note = "E";
      break;
    case NOTE_F:
      *note = "F";
      break;
    case NOTE_FS:
      *note = "F#";
      break;
    case NOTE_G:
      *note = "G";
      break;
    case NOTE_GS:
      *note = "G#";
      break;
    case NOTE_A:
      *note = "A";
      break;
    case NOTE_AS:
      *note = "A#";
      break;
    case NOTE_B:
      *note = "B";
      break;
    default:
      break;  // impossible
  }
}

static void standard_notes_nearest_note(gfloat freq, gint *octave, gchar **note, gfloat *bias) {
  int      i, nearest_index;
  gdouble  mid_freq;

  i = 0;
  while(freq > standard_notes[i])
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

  standard_notes_index_to_name(nearest_index, octave,  note);
}

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData {
  GstElement *pipeline, *pulsesrc, *tee;
  GstElement *audio_queue, *audio_convert1, *audio_resample1, *audio_sink;
  GstElement *video_queue, *audio_convert2, *visual, *video_convert, *video_sink;
  GstElement *app_queue, *audio_resample2, *capsfilter, *audio_convert3, *spectrum, *fake_sink;
  GMainLoop  *main_loop;
  guint       rate;           // resample audio to this rate befor analysis
  guint       spect_bands;    // see GstElement spectrum'property
  gint        threshold;      // see GstElement spectrum'property
} CustomData;


/* This function is called when an error message is posted on the bus */
static void pipeline_error_cb (GstBus *bus, GstMessage *msg, CustomData *data) {
  GError *err;
  gchar *debug_info;

  /* Print error details on the screen */
  gst_message_parse_error (msg, &err, &debug_info);
  g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
  g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
  g_clear_error (&err);
  g_free (debug_info);

  g_main_loop_quit (data->main_loop);
}

/* 
static void pipeline_state_changed_cb(GstBus *bus, GstMessage *msg, CustomData *data) {
  //GstState old_state, new_state, pending_state;
  //gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
  //g_print ("\nPipeline state changed from %s to %s:\n",
  //    gst_element_state_get_name (old_state), gst_element_state_get_name (new_state));

  GstPad  *pad = NULL;
  GstCaps *caps = NULL;
  const GstStructure *str;

  pad = gst_element_get_static_pad (data->spectrum, "sink");
  if (!pad) {
    g_printerr ("Could not retrieve pad.\n");
    return;
  }
  caps = gst_pad_get_current_caps (pad);
  if (!caps)
    return;
  g_return_if_fail(gst_caps_is_fixed (caps));

  str = gst_caps_get_structure (caps, 0);
  if (!gst_structure_get_int (str, "rate", &(data->rate))) {
    g_print ("No ratio available\n");
    return;
  }
  g_print("spectrum RATE: %d\n", data->rate);
}
*/

/* 
  This function execute Harmonic Product Spectrum on the result of FFT
  and return fundamental frequency
*/
static gdouble hps(const GValue *magnitudes, CustomData *data) {
  const GValue *mag;
  gint i;
  gdouble freq;
  gdouble *original_mag, *compress2_mag, *compress3_mag; 
  gdouble min_value = data->threshold * data->threshold * data->threshold;
  gint    min_index = 0;
  gdouble product;

  original_mag = (gdouble *)g_malloc(sizeof(gdouble) * data->spect_bands);
  compress2_mag = (gdouble *)g_malloc(sizeof(gdouble) * data->spect_bands / 2);
  compress3_mag = (gdouble *)g_malloc(sizeof(gdouble) * data->spect_bands / 3);

  // TODO: use only one loop and one buffer.
  for (i = 0; i< data->spect_bands; i++)
  {
    freq = (gdouble)data->rate / 2 / data->spect_bands * (i + 0.5);
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

  for (i = 0; i < data->spect_bands / 3; i++) {
    product = original_mag[i] * compress2_mag[i] * compress3_mag[i];
    //g_print("i: %d freq: %f, mag: %f\n", i, (gdouble)data->rate / 2 / data->spect_bands * (i + 0.5), product);
    if (product > min_value) {
      min_index = i;
      min_value = product;
    }
  }
  
  freq = (gdouble)data->rate / 2 / data->spect_bands * (min_index + 0.5);
  //g_print("%d, %f, %f\n", min_index, min_value, freq);
  g_free(original_mag);
  g_free(compress2_mag);
  g_free(compress3_mag);

  return freq;
}

/* This function is called evertime spectrum element fininsh a FFT */
static void handle_spectrum(const GstStructure *s, CustomData *data)
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
  gfloat        freq, bias;
  gint          octave;
  gchar        *note;
  magnitudes = gst_structure_get_value (s, "magnitude");  

  freq = hps(magnitudes, data);
  standard_notes_nearest_note(freq, &octave, &note, &bias);

  g_print("\rOctave: %d, Note: %s, Bias: %f", octave, note, bias);
}

static void pipeline_element_cb(GstBus *bus, GstMessage *msg, CustomData *data) {
    const GstStructure *s = gst_message_get_structure (msg);
    const gchar *name = gst_structure_get_name (s);

    if (strcmp (name, "spectrum") == 0) {
      handle_spectrum(s, data);
    }
}

static void pad_added_handler (GstElement *src, GstPad *new_pad, CustomData *data) {
  GstPad *sink_pad = gst_element_get_static_pad (data->tee, "sink");
  GstPadLinkReturn ret;
  GstCaps *new_pad_caps = NULL;
  GstStructure *new_pad_struct = NULL;
  const gchar *new_pad_type = NULL;

  g_print ("Received new pad '%s' from '%s':\n", GST_PAD_NAME (new_pad), GST_ELEMENT_NAME (src));
  /* If our converter is already linked, we have nothing to do here */
  if (gst_pad_is_linked (sink_pad)) {
    g_print ("We are already linked. Ignoring.\n");
    goto exit;
  }

  /* Check the new pad's type */
  new_pad_caps = gst_pad_get_current_caps (new_pad);
  new_pad_struct = gst_caps_get_structure (new_pad_caps, 0);
  new_pad_type = gst_structure_get_name (new_pad_struct);
  if (!g_str_has_prefix (new_pad_type, "audio/x-raw")) {
    g_print ("It has type '%s' which is not raw audio. Ignoring.\n", new_pad_type);
    goto exit;
  }

  /* Attempt the link */
  ret = gst_pad_link (new_pad, sink_pad);
  if (GST_PAD_LINK_FAILED (ret)) {
    g_print ("Type is '%s' but link failed.\n", new_pad_type);
  } else {
    g_print ("Link succeeded (type '%s').\n", new_pad_type);
  }

exit:
  /* Unreference the new pad's caps, if we got them */
  if (new_pad_caps != NULL)
    gst_caps_unref (new_pad_caps);

  /* Unreference the sink pad */
  gst_object_unref (sink_pad);
}

int main(int argc, char *argv[]) {
  CustomData data;
  GstPad  *tee_audio_pad, *tee_video_pad, *tee_app_pad;
  GstPad  *queue_audio_pad, *queue_video_pad, *queue_app_pad;
  GstBus  *bus;
  GstCaps *caps;

  /* Initialize cumstom data structure */
  memset (&data, 0, sizeof (data));
  data.spect_bands = 4500;
  data.rate = 3000;
  data.threshold = -80;

  /* Initialize standard note frequency table */
  standard_notes_init();

  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  /* Create the elements */
  // start
  // !!!!!!!! testsrc
  // data.pulsesrc = gst_element_factory_make ("audiotestsrc", "audio_source");
  // g_object_set (G_OBJECT (data.pulsesrc), "wave", 0, "freq", 6000.0, NULL);

  //!!!!!!! record
  data.pulsesrc = gst_element_factory_make ("pulsesrc", "audio_source");

  //!!!!!!! file
  //data.pulsesrc = gst_element_factory_make ("uridecodebin", "audio_source");
  //g_object_set (G_OBJECT (data.pulsesrc), "uri", "file:///home/lawrence/Projects/GuitarTuner/Guitar_Standard_Tuning.ogg", NULL);
  //g_signal_connect (data.pulsesrc, "pad-added", G_CALLBACK (pad_added_handler), &data);
  // end

  data.tee = gst_element_factory_make ("tee", "tee");
  data.audio_queue = gst_element_factory_make ("queue", "audio_queue");
  data.audio_convert1 = gst_element_factory_make ("audioconvert", "audio_convert1");
  data.audio_resample1 = gst_element_factory_make ("audioresample", "audio_resample1");
  data.audio_sink = gst_element_factory_make ("autoaudiosink", "audio_sink");

  data.video_queue = gst_element_factory_make ("queue", "video_queue");
  data.audio_convert2 = gst_element_factory_make ("audioconvert", "audio_convert2");
  data.visual = gst_element_factory_make ("wavescope", "visual");
  data.video_convert = gst_element_factory_make ("videoconvert", "video_convert");
  data.video_sink = gst_element_factory_make ("autovideosink", "video_sink");

  data.app_queue = gst_element_factory_make ("queue", "app_queue");
  data.audio_convert3 = gst_element_factory_make ("audioconvert", "audio_convert3");
  data.audio_resample2 = gst_element_factory_make ("audioresample", "audio_resample2");
  data.capsfilter = gst_element_factory_make ("capsfilter", "filter");
  data.spectrum = gst_element_factory_make ("spectrum", "spectrum");
  data.fake_sink = gst_element_factory_make ("fakesink", "fake_sink");

  /* Create the empty pipeline */
  data.pipeline = gst_pipeline_new ("test-pipeline");


  if (!data.pipeline || !data.pulsesrc || !data.tee || !data.audio_queue || !data.audio_convert1 ||
      !data.audio_resample1 || !data.audio_sink || !data.video_queue || !data.audio_convert2 || !data.visual ||
      !data.video_convert || !data.video_sink || !data.app_queue || !data.audio_convert3 || !data.audio_resample2 || 
      !data.capsfilter || !data.spectrum || !data.fake_sink) {
    g_printerr ("Not all elements could be created.\n");
    return -1;
  }

  /* Configure wavescope */
  g_object_set (data.visual, "shader", 0, "style", 0, NULL);

  /* Configure resample */
  caps = gst_caps_new_simple ("audio/x-raw", "rate", G_TYPE_INT, data.rate, NULL);
  g_object_set (data.capsfilter, "caps", caps, NULL);

  /* Configure spectrum */
  g_object_set (G_OBJECT (data.spectrum), "bands", data.spect_bands, "interval", 100000000,"threshold", data.threshold, "post-messages", TRUE, "message-phase", TRUE, NULL);

  gst_bin_add_many (GST_BIN (data.pipeline), data.pulsesrc, data.tee, data.audio_queue, data.audio_convert1, data.audio_resample1,
      data.audio_sink, data.video_queue, data.audio_convert2, data.visual, data.video_convert, data.video_sink, data.app_queue,
      data.audio_convert3, data.audio_resample2, data.capsfilter, data.spectrum, data.fake_sink, NULL);
  /* Link all elements that can be automatically linked because they have "Always" pads */
  if (!gst_element_link_many (data.pulsesrc, data.tee, NULL) ||
      !gst_element_link_many (data.audio_queue, data.audio_convert1, data.audio_resample1, data.audio_sink, NULL) ||
      !gst_element_link_many (data.video_queue, data.audio_convert2, data.visual, data.video_convert, data.video_sink, NULL) ||
      !gst_element_link_many (data.app_queue, data.audio_convert3,  data.audio_resample2, data.capsfilter, data.spectrum, data.fake_sink, NULL)) {
    g_printerr ("Elements could not be linked.\n");
    gst_object_unref (data.pipeline);
    return -1;
  }

  /* Manually link the Tee, which has "Request" pads */
  tee_audio_pad = gst_element_get_request_pad (data.tee, "src_%u");
  //g_print ("Obtained request pad %s for audio branch.\n", gst_pad_get_name (tee_audio_pad));
  queue_audio_pad = gst_element_get_static_pad (data.audio_queue, "sink");
  tee_video_pad = gst_element_get_request_pad (data.tee, "src_%u");
  //g_print ("Obtained request pad %s for video branch.\n", gst_pad_get_name (tee_video_pad));
  queue_video_pad = gst_element_get_static_pad (data.video_queue, "sink");
  tee_app_pad = gst_element_get_request_pad (data.tee, "src_%u");
  //g_print ("Obtained request pad %s for app branch.\n", gst_pad_get_name (tee_app_pad));
  queue_app_pad = gst_element_get_static_pad (data.app_queue, "sink");
  if (gst_pad_link (tee_audio_pad, queue_audio_pad) != GST_PAD_LINK_OK ||
      gst_pad_link (tee_video_pad, queue_video_pad) != GST_PAD_LINK_OK ||
      gst_pad_link (tee_app_pad, queue_app_pad) != GST_PAD_LINK_OK) {
    g_printerr ("Tee could not be linked\n");
    gst_object_unref (data.pipeline);
    return -1;
  }
  gst_object_unref (queue_audio_pad);
  gst_object_unref (queue_video_pad);
  gst_object_unref (queue_app_pad);

  /* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
  bus = gst_element_get_bus (data.pipeline);
  gst_bus_add_signal_watch (bus);
  g_signal_connect (G_OBJECT (bus), "message::error", (GCallback)pipeline_error_cb, &data);
  //g_signal_connect (G_OBJECT (bus), "message::state-changed", (GCallback)pipeline_state_changed_cb, &data);
  g_signal_connect (G_OBJECT (bus), "message::element", (GCallback)pipeline_element_cb, &data);
  gst_object_unref (bus);

  /* Start playing the pipeline */
  gst_element_set_state (data.pipeline, GST_STATE_PLAYING);

  /* Create a GLib Main Loop and set it to run */
  data.main_loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (data.main_loop);

  /* Release the request pads from the Tee, and unref them */
  gst_element_release_request_pad (data.tee, tee_audio_pad);
  gst_element_release_request_pad (data.tee, tee_video_pad);
  gst_element_release_request_pad (data.tee, tee_app_pad);
  gst_object_unref (tee_audio_pad);
  gst_object_unref (tee_video_pad);
  gst_object_unref (tee_app_pad);

  /* Free resources */
  gst_element_set_state (data.pipeline, GST_STATE_NULL);
  gst_object_unref (data.pipeline);
  return 0;
}
