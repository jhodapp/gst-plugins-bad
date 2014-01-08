/*
 * Unit and functional tests for gstamcvideodec
 *
 * Copyright (C) 2013, Canonical Ltd.
 *   Author: Jim Hodapp <jim.hodapp@canonical.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#include <gst/check/gstcheck.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <unistd.h>

static GstElement *pipeline;
static GstBus *bus;
static GMainLoop *loop;
static guint messages = 0;
static guint num_tags = 0;

extern void setup_egl (void);

static void
element_message_cb (GstBus * bus, GstMessage * message, gpointer user_data)
{
  gchar *s = gst_structure_to_string (gst_message_get_structure (message));
  GST_DEBUG ("Received message: %s", s);
  g_free (s);

  ++messages;
}

static void
eos_message_cb (GstBus * bus, GstMessage * message, gpointer user_data)
{
  GST_DEBUG ("Received EOS");
  g_main_loop_quit (loop);
}

static GstPadProbeReturn
pad_event_probe_cb (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      GST_INFO ("GST_EVENT_FLUSH_START");
      break;
    case GST_EVENT_FLUSH_STOP:
      GST_INFO ("GST_EVENT_FLUSH_STOP");
      break;
    case GST_EVENT_STREAM_START:
      GST_INFO ("GST_EVENT_STREAM_START");
      break;
    case GST_EVENT_CAPS:
      GST_INFO ("GST_EVENT_CAPS");
      break;
    case GST_EVENT_SEGMENT:
      GST_INFO ("GST_EVENT_SEGMENT");
      break;
    case GST_EVENT_TAG:
      GST_INFO ("GST_EVENT_TAG");
      ++num_tags;
      break;
    case GST_EVENT_BUFFERSIZE:
      GST_INFO ("GST_EVENT_BUFFERSIZE");
      break;
    case GST_EVENT_SINK_MESSAGE:
      GST_INFO ("GST_EVENT_SINK_MESSAGE");
      break;
    case GST_EVENT_EOS:
      GST_INFO ("GST_EVENT_EOS");
      break;
    case GST_EVENT_TOC:
      GST_INFO ("GST_EVENT_TOC");
      break;
    case GST_EVENT_SEGMENT_DONE:
      GST_INFO ("GST_EVENT_SEGMENT_DONE");
      break;
    case GST_EVENT_GAP:
      GST_INFO ("GST_EVENT_GAP");
      break;
    case GST_EVENT_QOS:
      GST_INFO ("GST_EVENT_QOS");
      break;
    case GST_EVENT_SEEK:
      GST_INFO ("GST_EVENT_SEEK");
      break;
    case GST_EVENT_NAVIGATION:
      GST_INFO ("GST_EVENT_NAVIGATION");
      break;
    case GST_EVENT_LATENCY:
      GST_INFO ("GST_EVENT_LATENCY");
      break;
    case GST_EVENT_STEP:
      GST_INFO ("GST_EVENT_STEP");
      break;
    case GST_EVENT_RECONFIGURE:
      GST_INFO ("GST_EVENT_RECONFIGURE");
      break;
    case GST_EVENT_TOC_SELECT:
      GST_INFO ("GST_EVENT_TOC_SELECT");
      break;
    case GST_EVENT_CUSTOM_UPSTREAM:
      GST_INFO ("GST_EVENT_CUSTOM_UPSTREAM");
      break;
    case GST_EVENT_CUSTOM_DOWNSTREAM:
      GST_INFO ("GST_EVENT_CUSTOM_DOWNSTREAM");
      break;
    case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:
      GST_INFO ("GST_EVENT_CUSTOM_DOWNSTREAM_OOB");
      break;
    case GST_EVENT_CUSTOM_DOWNSTREAM_STICKY:
      GST_INFO ("GST_EVENT_CUSTOM_DOWNSTREAM_STICKY");
      break;
    case GST_EVENT_CUSTOM_BOTH:
      GST_INFO ("GST_EVENT_CUSTOM_BOTH");
      break;
    case GST_EVENT_CUSTOM_BOTH_OOB:
      GST_INFO ("GST_EVENT_CUSTOM_BOTH_OOB");
      break;
    default:
      GST_INFO ("Unknown GstEventType: %d", event->type);
      break;
  }

  return GST_PAD_PROBE_OK;
}

static GstElement *
setup_filesink (void)
{
  GstElement *sink;

  GST_DEBUG ("Setting up filesink");

  sink = gst_element_factory_make ("filesink", "video-sink");

  return sink;
}

static GstElement *
setup_fakesink (void)
{
  GstElement *sink;

  GST_DEBUG ("Setting up fakesink");

  sink = gst_element_factory_make ("fakesink", "video-sink");

  return sink;
}

static gboolean
remove_tmp_file (const char *filename)
{
  gint ret = 0;
  g_return_val_if_fail (filename != NULL, FALSE);

  GST_INFO ("g_access: %d", g_access (filename, R_OK | W_OK));
  /* Check to make sure we can remove the temp file */
  ret = g_access (filename, R_OK | W_OK);
  if (ret != 0)
    return FALSE;

  ret = g_remove (filename);
  if (ret != 0)
    return FALSE;

  return TRUE;
}

static void
setup_pipeline (const char *pipe_str, GstElement * video_sink,
    GstElement * audio_sink)
{
  GError *error = NULL;
  GstPad *pad = NULL;

  fail_if (pipe_str == NULL);

  pipeline = gst_parse_launch (pipe_str, &error);
  fail_unless (pipeline != NULL, "Error parsing pipeline: %s",
      error ? error->message : "(invalid error)");

  if (video_sink)
    g_object_set (G_OBJECT (pipeline), "video-sink", video_sink, NULL);
  if (audio_sink)
    g_object_set (G_OBJECT (pipeline), "audio-sink", audio_sink, NULL);

  bus = gst_element_get_bus (pipeline);
  fail_if (bus == NULL);
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message::element", (GCallback) element_message_cb,
      NULL);
  g_signal_connect (bus, "message::eos", (GCallback) eos_message_cb, NULL);

  messages = 0;
  num_tags = 0;

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* Run the test pipeline until we hit the EOS */
  loop = g_main_loop_new (NULL, FALSE);

  if (video_sink) {
    GST_INFO ("Getting video sink pad on video-sink");
    /* Get the video src pad which should contain raw decoded frames */
    pad = gst_element_get_static_pad (video_sink, "sink");
    fail_if (pad == NULL);
    gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_DATA_BOTH,
        pad_event_probe_cb, NULL, NULL);
  }
}

static void
teardown_pipeline (GstElement * video_sink, GstElement * audio_sink)
{
  g_main_loop_unref (loop);
  fail_unless (gst_element_set_state (pipeline,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS);

  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);

  if (video_sink)
    video_sink = NULL;

  fail_if (messages > 0, "Received too many decoding errors");
  pipeline = NULL;
}

GST_START_TEST (test_hardware_decode)
{
  gchar *pipe_str =
      g_strdup_printf ("playbin video-sink=fakesink audio-sink=fakesink"
      " uri=file:///home/phablet/Videos/sintel_trailer-480p.mp4");

  setup_pipeline (pipe_str, NULL, NULL);
  g_free (pipe_str);
  g_main_loop_run (loop);
  teardown_pipeline (NULL, NULL);
}

GST_END_TEST
GST_START_TEST (test_hardware_decode_verify_height_width)
{
  //GstElement *decodebin = NULL;
  GstElement *video_sink;
  //GstPad *pad = NULL;
  gchar *location;
  gchar *pipe_str;

  g_random_int_range (0, INT_MAX);
  location = g_strdup_printf ("/tmp/gstamc_%d.raw", g_random_int ());
  pipe_str = g_strdup_printf ("playbin audio-sink=fakesink"
      " uri=file:///home/phablet/Videos/sintel_trailer-480p.mp4");

  /* Send the raw video frames to a temp file */
  video_sink = setup_filesink ();
  fail_if (video_sink == NULL);
  g_object_set (G_OBJECT (video_sink), "location", location, NULL);

  setup_pipeline (pipe_str, video_sink, NULL);
  g_free (pipe_str);
#if 0
  decodebin = gst_bin_get_by_name (GST_BIN (pipeline), "decodebin0");
  fail_if (decodebin == NULL);
#endif

  g_main_loop_run (loop);

  teardown_pipeline (video_sink, NULL);
  GST_INFO ("num_tags: %d", num_tags);
  fail_if (num_tags < 25);

  GST_INFO ("Removing tmp file: %s", location);
  fail_if (remove_tmp_file (location) != TRUE);
  g_free (location);
}

GST_END_TEST
#if 0
GST_START_TEST (test_hardware_decode_and_render)
{
  gchar *pipe_str =
      g_strdup_printf ("playbin video-sink=mirsink audio-sink=fakesink"
      " uri=file:///home/phablet/Videos/sintel_trailer-480p.mp4");

  setup_pipeline (pipe_str, NULL, NULL);
  g_free (pipe_str);
  setup_egl ();
  g_main_loop_run (loop);
  teardown_pipeline (NULL, NULL);
}

GST_END_TEST
#endif
GST_START_TEST (test_h264_decode_avi_container)
{
  GstElement *video_sink;
  gchar *pipe_str = g_strdup_printf ("playbin audio-sink=fakesink"
      " uri=file:///home/phablet/Videos/h264.avi");

  /* Send the raw video frames to a temp file */
  video_sink = setup_fakesink ();
  fail_if (video_sink == NULL);

  setup_pipeline (pipe_str, video_sink, NULL);
  g_free (pipe_str);
  g_main_loop_run (loop);
  teardown_pipeline (NULL, NULL);
  GST_INFO ("num_tags: %d", num_tags);
  fail_if (num_tags < 98);
}

GST_END_TEST
GST_START_TEST (test_h264_decode_flv_container)
{
  GstElement *video_sink;
  gchar *pipe_str = g_strdup_printf ("playbin audio-sink=fakesink"
      " uri=file:///home/phablet/Videos/h264.flv");

  /* Send the raw video frames to a temp file */
  video_sink = setup_fakesink ();
  fail_if (video_sink == NULL);

  setup_pipeline (pipe_str, video_sink, NULL);
  g_free (pipe_str);
  g_main_loop_run (loop);
  teardown_pipeline (NULL, NULL);
  GST_INFO ("num_tags: %d", num_tags);
  fail_if (num_tags < 98);
}

GST_END_TEST
GST_START_TEST (test_h264_decode_mov_container)
{
  GstElement *video_sink;
  gchar *pipe_str = g_strdup_printf ("playbin audio-sink=fakesink"
      " uri=file:///home/phablet/Videos/h264.mov");

  /* Send the raw video frames to a temp file */
  video_sink = setup_fakesink ();
  fail_if (video_sink == NULL);

  setup_pipeline (pipe_str, video_sink, NULL);
  g_free (pipe_str);
  g_main_loop_run (loop);
  teardown_pipeline (NULL, NULL);
  GST_INFO ("num_tags: %d", num_tags);
  fail_if (num_tags < 98);
}

GST_END_TEST
GST_START_TEST (test_h264_decode_mp4_container)
{
  GstElement *video_sink;
  gchar *pipe_str = g_strdup_printf ("playbin audio-sink=fakesink"
      " uri=file:///home/phablet/Videos/h264.mp4");

  /* Send the raw video frames to a temp file */
  video_sink = setup_fakesink ();
  fail_if (video_sink == NULL);

  setup_pipeline (pipe_str, video_sink, NULL);
  g_free (pipe_str);
  g_main_loop_run (loop);
  teardown_pipeline (NULL, NULL);
  GST_INFO ("num_tags: %d", num_tags);
  fail_if (num_tags < 98);
}

GST_END_TEST
GST_START_TEST (test_mpeg4_decode_avi_container)
{
  GstElement *video_sink;
  gchar *pipe_str = g_strdup_printf ("playbin audio-sink=fakesink"
      " uri=file:///home/phablet/Videos/mpeg4.avi");

  /* Send the raw video frames to a temp file */
  video_sink = setup_fakesink ();
  fail_if (video_sink == NULL);

  setup_pipeline (pipe_str, video_sink, NULL);
  g_free (pipe_str);
  g_main_loop_run (loop);
  teardown_pipeline (video_sink, NULL);
  GST_INFO ("num_tags: %d", num_tags);
  fail_if (num_tags < 59);
}

GST_END_TEST
GST_START_TEST (test_mpeg4_decode_mov_container)
{
  GstElement *video_sink;
  gchar *pipe_str = g_strdup_printf ("playbin audio-sink=fakesink"
      " uri=file:///home/phablet/Videos/mpeg4.mov");

  /* Send the raw video frames to a temp file */
  video_sink = setup_fakesink ();
  fail_if (video_sink == NULL);

  setup_pipeline (pipe_str, video_sink, NULL);
  g_free (pipe_str);
  g_main_loop_run (loop);
  teardown_pipeline (NULL, NULL);
  GST_INFO ("num_tags: %d", num_tags);
  fail_if (num_tags < 25);
}

GST_END_TEST
GST_START_TEST (test_mpeg4_decode_mp4_container)
{
  GstElement *video_sink;
  gchar *pipe_str = g_strdup_printf ("playbin audio-sink=fakesink"
      " uri=file:///home/phablet/Videos/mpeg4.mp4");

  /* Send the raw video frames to a temp file */
  video_sink = setup_fakesink ();
  fail_if (video_sink == NULL);

  setup_pipeline (pipe_str, video_sink, NULL);
  g_free (pipe_str);
  g_main_loop_run (loop);
  teardown_pipeline (NULL, NULL);
  GST_INFO ("num_tags: %d", num_tags);
  fail_if (num_tags < 25);
}

GST_END_TEST
GST_START_TEST (test_xvid_decode_mp4_container)
{
  GstElement *video_sink;
  gchar *pipe_str = g_strdup_printf ("playbin audio-sink=fakesink"
      " uri=file:///home/phablet/Videos/xvid.mp4");

  /* Send the raw video frames to a temp file */
  video_sink = setup_fakesink ();
  fail_if (video_sink == NULL);

  setup_pipeline (pipe_str, video_sink, NULL);
  g_free (pipe_str);
  g_main_loop_run (loop);
  teardown_pipeline (NULL, NULL);
  GST_INFO ("num_tags: %d", num_tags);
  fail_if (num_tags < 25);
}

GST_END_TEST
GST_START_TEST (test_vorbis_decode_ogg_container)
{
  GstElement *video_sink;
  gchar *pipe_str = g_strdup_printf ("playbin audio-sink=fakesink"
      " uri=file:///home/phablet/Videos/vorbis.ogg");

  /* Send the raw video frames to a temp file */
  video_sink = setup_fakesink ();
  fail_if (video_sink == NULL);

  setup_pipeline (pipe_str, video_sink, NULL);
  g_free (pipe_str);
  g_main_loop_run (loop);
  teardown_pipeline (NULL, NULL);
  GST_INFO ("num_tags: %d", num_tags);
  fail_if (num_tags < 25);
}

GST_END_TEST
GST_START_TEST (test_vpx_decode_webm_container)
{
  GstElement *video_sink;
  gchar *pipe_str = g_strdup_printf ("playbin audio-sink=fakesink"
      " uri=file:///home/phablet/Videos/vpx.webm");

  /* Send the raw video frames to a temp file */
  video_sink = setup_fakesink ();
  fail_if (video_sink == NULL);

  setup_pipeline (pipe_str, video_sink, NULL);
  g_free (pipe_str);
  g_main_loop_run (loop);
  teardown_pipeline (NULL, NULL);
  GST_INFO ("num_tags: %d", num_tags);
  fail_if (num_tags < 25);
}

GST_END_TEST static Suite *
gstamcvideodec_suite (void)
{
  Suite *s = suite_create ("gstamcvideodec");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  //tcase_add_checked_fixture (tc_chain, setup_pipeline, teardown_pipeline);
  tcase_set_timeout (tc_chain, 60);
  tcase_add_test (tc_chain, test_hardware_decode_verify_height_width);
  tcase_add_test (tc_chain, test_hardware_decode);
  //tcase_add_test (tc_chain, test_hardware_decode_and_render);
  tcase_add_test (tc_chain, test_h264_decode_avi_container);
  tcase_add_test (tc_chain, test_h264_decode_flv_container);
  tcase_add_test (tc_chain, test_h264_decode_mov_container);
  tcase_add_test (tc_chain, test_h264_decode_mp4_container);
  tcase_add_test (tc_chain, test_mpeg4_decode_avi_container);
  /* Doesn't work correctly yet: */
  //tcase_add_test (tc_chain, test_mpeg4_decode_mov_container);
  //tcase_add_test (tc_chain, test_mpeg4_decode_mp4_container);
  //tcase_add_test (tc_chain, test_xvid_decode_mp4_container);
  //tcase_add_test (tc_chain, test_vorbis_decode_ogg_container);
  //tcase_add_test (tc_chain, test_vpx_decode_webm_container);

  return s;
}

GST_CHECK_MAIN (gstamcvideodec);
