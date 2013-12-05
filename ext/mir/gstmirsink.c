/*
 * GStreamer Mir video sink
 * Copyright (C) 2013 Canonical Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */

/**
 * SECTION:element-mirsink
 *
 *  The mirsink creates its own window and renders the decoded video frames there.
 *  Setup the Mir environment as described in
 *  <ulink url="http://mir.freedesktop.org/building.html">Mir</ulink> home page.
 *
 * <refsect2>
 * <title>Example pipeline</title>
 * |[
 * gst-launch -v filesrc ! qtdemux ! h264parse ! queue ! amcviddec-omxtiducati1videodecoder ! mirsink
 * ]| test the video rendering with mirsink
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstmirsink.h"
#include "mirpool.h"

#include <gst/mir/mirallocator.h>
#include <gst/mir/gstmircontext.h>

#include <hybris/media/surface_texture_client_hybris.h>

#include <gst/egl/egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

/* signals */
enum
{
  SIGNAL_0,
  LAST_SIGNAL
};

/* Properties */
enum
{
  PROP_0,
  PROP_MIR_TEXTURE_ID
};

GST_DEBUG_CATEGORY (gstmir_debug);
#define GST_CAT_DEFAULT gstmir_debug

#if 0
static const char gst_mirsink_sink_caps_str[] =
GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_MIR_IMAGE,
    GST_VIDEO_FORMATS_ALL) ";"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_META_GST_VIDEO_GL_TEXTURE_UPLOAD_META,
    GST_VIDEO_FORMATS_ALL);

     static GstStaticPadTemplate gst_mirsink_sink_caps_template =
         GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (gst_mirsink_sink_caps_str));
#else
static GstStaticPadTemplate gst_mirsink_sink_caps_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, "
        "format=(string) {RGBA, BGRA, ARGB, ABGR, RGBx, BGRx, xRGB, xBGR, "
        "AYUV, Y444, I420, YV12, NV12, NV21, Y42B, Y41B, RGB, BGR, RGB16}, "
        "width=(int)[ 1, MAX ], " "height=(int)[ 1, MAX ], "
        "framerate=(fraction)[ 0, MAX ] ")
    );
#endif

static guint frame_ready_signal = 0;
static guint surface_texture_client_set_signal = 0;

/* Fixme: Add more interfaces */
#define gst_mir_sink_parent_class parent_class
G_DEFINE_TYPE (GstMirSink, gst_mir_sink, GST_TYPE_VIDEO_SINK);

static void gst_mir_sink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_mir_sink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_mir_sink_finalize (GObject * object);
static void gst_mir_sink_set_context (GstElement * element,
    GstContext * context);
static GstCaps *gst_mir_sink_get_caps (GstBaseSink * bsink, GstCaps * filter);
static gboolean gst_mir_sink_set_caps (GstBaseSink * bsink, GstCaps * caps);
static gboolean gst_mir_sink_start (GstBaseSink * bsink);
static gboolean gst_mir_sink_stop (GstBaseSink * bsink);
static gboolean gst_mir_sink_preroll (GstBaseSink * bsink, GstBuffer * buffer);
static gboolean
gst_mir_sink_propose_allocation (GstBaseSink * bsink, GstQuery * query);
static gboolean gst_mir_sink_render (GstBaseSink * bsink, GstBuffer * buffer);
static gboolean gst_mir_sink_query (GstBaseSink * bsink, GstQuery * query);

static void
gst_mir_sink_class_init (GstMirSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;

  gobject_class->set_property = gst_mir_sink_set_property;
  gobject_class->get_property = gst_mir_sink_get_property;
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_mir_sink_finalize);
  gstelement_class->set_context = gst_mir_sink_set_context;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_mirsink_sink_caps_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "Mir video sink", "Sink/Video",
      "Output to Mir surface", "Jim Hodapp <jim.hodapp@canonical.com>");

  gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_mir_sink_get_caps);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_mir_sink_set_caps);
  gstbasesink_class->start = GST_DEBUG_FUNCPTR (gst_mir_sink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_mir_sink_stop);
  gstbasesink_class->preroll = GST_DEBUG_FUNCPTR (gst_mir_sink_preroll);
  gstbasesink_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_mir_sink_propose_allocation);
  gstbasesink_class->render = GST_DEBUG_FUNCPTR (gst_mir_sink_render);
  gstbasesink_class->query = GST_DEBUG_FUNCPTR (gst_mir_sink_query);

  /* This signal is for being notified when a frame is ready to be rendered. This
   * is useful for anything outside of the sink that needs to know when each frame
   * is ready. */
  frame_ready_signal =
      g_signal_new ("frame-ready", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstMirSinkClass, frame_ready_changed), NULL, NULL,
      g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 0);

  surface_texture_client_set_signal =
      g_signal_new ("surface-texture-client", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstMirSinkClass, surface_texture_client_changed), NULL,
      NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);

  g_object_class_install_property (gobject_class, PROP_MIR_TEXTURE_ID,
      g_param_spec_uint ("texture-id", "Texture ID",
          "Texture ID to render video to, created by the application", 0,
          UINT_MAX, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_mir_sink_init (GstMirSink * sink)
{
  GST_DEBUG_OBJECT (sink, "Initializing mirsink");
  sink->pool = NULL;

  g_mutex_init (&sink->mir_lock);

  sink->context = NULL;

}

static void
gst_mir_sink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstMirSink *sink = GST_MIR_SINK (object);

  switch (prop_id) {
    case PROP_MIR_TEXTURE_ID:
      g_value_set_uint (value, sink->texture_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mir_sink_create_surface_texture (GObject * object)
{
  GstMirSink *sink = GST_MIR_SINK (object);

  /* Create a new SurfaceTextureClientHybris instance from a texture ID */
  sink->surface_texture_client =
      surface_texture_client_create_by_id (sink->texture_id);
  GST_DEBUG_OBJECT (sink, "Created new SurfaceTextureClientHybris instance: %p",
      sink->surface_texture_client);
  /* Because mirsink is being loaded, we are definitely doing
   * hardware rendering.
   */
  surface_texture_client_set_hardware_rendering (sink->surface_texture_client,
      TRUE);

  /* Signal the client API that a surface_texture_client instance has been set */
  g_signal_emit (G_OBJECT (sink), surface_texture_client_set_signal, 0,
      (gpointer) sink->surface_texture_client);
}

static void
gst_mir_sink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstMirSink *sink = GST_MIR_SINK (object);

  switch (prop_id) {
    case PROP_MIR_TEXTURE_ID:
      sink->texture_id = g_value_get_uint (value);
      GST_WARNING_OBJECT (object, "texture_id: %d", sink->texture_id);
      gst_mir_sink_create_surface_texture (object);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

#if 0
static void
destroy_display (struct display *display)
{
  free (display);
}

static void
destroy_session (struct session *session)
{
  if (session->app_options)
    u_application_options_destroy (session->app_options);

  if (session->app_description)
    u_application_description_destroy (session->app_description);

  free (session);
}

static void
destroy_window (struct window *window)
{
  if (window->properties)
    ua_ui_window_properties_destroy (window->properties);

  if (window->window)
    ua_ui_window_destroy (window->window);

  free (window);
}
#endif

static void
gst_mir_sink_finalize (GObject * object)
{
  GstMirSink *sink = GST_MIR_SINK (object);

  GST_DEBUG_OBJECT (sink, "Finalizing the sink..");

#if 0
  if (sink->window)
    destroy_window (sink->window);
  if (sink->display)
    destroy_display (sink->display);
  if (sink->session)
    destroy_session (sink->session);
#endif

  /* Make sure that the SurfaceTextureClientHybris instance gets cleaned
   * up when the pipeline is being torn down.
   */
  if (sink->surface_texture_client)
    surface_texture_client_unref (sink->surface_texture_client);

  g_mutex_clear (&sink->mir_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_mir_sink_set_context (GstElement * element, GstContext * context)
{
  GST_DEBUG_OBJECT (element, "%s", __PRETTY_FUNCTION__);
}

static gboolean
gst_mir_sink_query (GstBaseSink * bsink, GstQuery * query)
{
  GstMirSink *sink = GST_MIR_SINK (bsink);

  GST_INFO_OBJECT (sink, "query type %s", GST_QUERY_TYPE_NAME (query));

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:{
      const gchar *context_type;
      GstMirContext *gst_mir_context;

      if (gst_query_parse_context_type (query, &context_type) &&
          strcmp (context_type, GST_MIR_CONTEXT_TYPE) == 0 &&
          sink->surface_texture_client) {
        GST_DEBUG_OBJECT (sink, "GST_MIR_CONTEXT_TYPE");

        gst_mir_context = gst_mir_context_new (GST_ELEMENT_CAST (sink),
            sink->surface_texture_client);
        sink->context = gst_mir_context_new_with_stc (gst_mir_context);
        GST_INFO_OBJECT (sink, "Setting context on the sink");
        gst_query_set_context (query, sink->context);
        gst_context_unref (sink->context);

        return TRUE;
      } else {
        goto base_class;
      }
      break;
    }
    default:
      goto base_class;
      break;
  }

base_class:
  return GST_BASE_SINK_CLASS (parent_class)->query (bsink, query);
}

#if 0
static struct display *
create_display (void)
{
  struct display *display;
  display = malloc (sizeof *display);

  display->display = ua_ui_display_new_with_index (0);
  if (display->display == NULL) {
    free (display);
    return NULL;
  }

  display->height = ua_ui_display_query_vertical_res (display->display);
  display->width = ua_ui_display_query_horizontal_res (display->display);

  GST_DEBUG ("Display resolution: (%d,%d)\n", display->height, display->width);

  return display;
}

static struct session *
create_session (void)
{
  struct session *session;
  char argv[1][1];
  session = malloc (sizeof *session);

  session->properties = ua_ui_session_properties_new ();
  ua_ui_session_properties_set_type (session->properties, U_SYSTEM_SESSION);
  session->session = ua_ui_session_new_with_properties (session->properties);
  if (!session->session)
    GST_WARNING ("Failed to start new Ubuntu Application API session");

  session->app_description = u_application_description_new ();
  session->app_lifecycle_delegate = u_application_lifecycle_delegate_new ();
  /* No context data to pass to the lifecycle delegate for now */
  u_application_lifecycle_delegate_set_context
      (session->app_lifecycle_delegate, NULL);
  u_application_description_set_application_lifecycle_delegate
      (session->app_description, session->app_lifecycle_delegate);

  /* The UA requires a command line option set, so give it a fake argv array */
  argv[0][0] = '\n';
  session->app_options =
      u_application_options_new_from_cmd_line (1, (char **) argv);
  session->app_instance =
      u_application_instance_new_from_description_with_options
      (session->app_description, session->app_options);
  if (!session->app_instance)
    GST_WARNING ("Failed to start a new Ubuntu Application API instance");

  return session;
}

static void
create_window (GstMirSink * sink, struct display *display, int width,
    int height)
{
  struct window *window;

  /* No need to create a window a second time */
  if (sink->window)
    return;

  g_mutex_lock (&sink->mir_lock);

  window = malloc (sizeof *window);
  window->display = display;
  window->width = width;
  window->height = height;

  window->properties = ua_ui_window_properties_new_for_normal_window ();
  ua_ui_window_properties_set_titlen (window->properties, "MirSinkWindow", 13);

  ua_ui_window_properties_set_role (window->properties, 1);
  ua_ui_window_properties_set_input_cb_and_ctx (window->properties, NULL, NULL);
  GST_DEBUG ("Creating new UA window");
  window->window =
      ua_ui_window_new_for_application_with_properties (sink->session->
      app_instance, window->properties);
  GST_DEBUG ("Setting window geometry");
  window->width = window->display->width;
  window->height = window->display->height;
  GST_DEBUG_OBJECT (sink, "width: %d, height: %d", window->width,
      window->height);

  if (height != 0 || width != 0)
    ua_ui_window_resize (window->window, window->width, window->height);


  window->egl_native_window = ua_ui_window_get_native_type (window->window);
  sink->window = window;

  g_mutex_unlock (&sink->mir_lock);
}
#endif

static GstCaps *
gst_mir_sink_get_caps (GstBaseSink * bsink, GstCaps * filter)
{
  GstMirSink *sink;
  GstCaps *caps;

  sink = GST_MIR_SINK (bsink);

  GST_DEBUG_OBJECT (sink, "%s", __PRETTY_FUNCTION__);

  caps = gst_pad_get_pad_template_caps (GST_VIDEO_SINK_PAD (sink));
  if (filter) {
    GstCaps *intersection;

    intersection =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = intersection;
  }
  return caps;
}

static gboolean
gst_mir_sink_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
  GstMirSink *sink = GST_MIR_SINK (bsink);
  GstBufferPool *newpool, *oldpool;
  GstMirBufferPool *m_pool;
  GstVideoInfo info;
  GstStructure *config;
  static GstAllocationParams params = {
    0, 0, 0, 15,
  };
  guint size;

  sink = GST_MIR_SINK (bsink);

  GST_DEBUG_OBJECT (sink, "set caps %" GST_PTR_FORMAT, caps);

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_format;

  sink->video_width = info.width;
  sink->video_height = info.height;
  size = info.size;

  GST_DEBUG_OBJECT (sink, "Creating new GstMirBufferPool");
  /* Create a new pool for the new configuration */
  newpool = gst_mir_buffer_pool_new (sink);

  if (!newpool) {
    GST_ERROR_OBJECT (sink, "Failed to create new pool");
    return FALSE;
  }

  GST_DEBUG_OBJECT (sink,
      "Setting SurfaceTextureClientHybris instance in m_pool");
  /* Add the SurfaceTextureClientHybris instance to the pool for later use */
  gst_mir_buffer_pool_set_surface_texture_client (newpool,
      sink->surface_texture_client);
  GST_WARNING_OBJECT (sink, "SurfaceTextureClientHybris: %p",
      sink->surface_texture_client);

  m_pool = GST_MIR_BUFFER_POOL_CAST (newpool);
  GST_WARNING_OBJECT (sink, "m_pool SurfaceTextureClientHybris: %p",
      m_pool->surface_texture_client);
  m_pool->width = sink->video_width;
  m_pool->height = sink->video_height;

  config = gst_buffer_pool_get_config (newpool);
  gst_buffer_pool_config_set_params (config, caps, size, 2, 0);
  gst_buffer_pool_config_set_allocator (config, NULL, &params);
  if (!gst_buffer_pool_set_config (newpool, config))
    goto config_failed;

  GST_OBJECT_LOCK (sink);
  oldpool = sink->pool;
  sink->pool = newpool;
  GST_OBJECT_UNLOCK (sink);

  GST_DEBUG_OBJECT (sink, "Finishing up set_caps");

  if (oldpool)
    gst_object_unref (oldpool);

  return TRUE;

invalid_format:
  {
    GST_DEBUG_OBJECT (sink,
        "Could not locate image format from caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }
config_failed:
  {
    GST_DEBUG_OBJECT (bsink, "failed setting config");
    return FALSE;
  }
}

static gboolean
gst_mir_sink_start (GstBaseSink * bsink)
{
  GstMirSink *sink = (GstMirSink *) bsink;

  GST_DEBUG_OBJECT (sink, "start");

  /* If we start playback again after an EOS, make sure we have a new valid
   * SurfaceTextureClientHybris instance to use and pass to the decoder.
   */
  if (!sink->surface_texture_client) {
    GST_DEBUG_OBJECT (sink, "Creating new SurfaceTextureClientHybris instance");
    gst_mir_sink_create_surface_texture (G_OBJECT (bsink));
  }
#if 0
  /* If we are using a texture_id, there's no need to use the Ubuntu
   * Platform API to create an EGLNativeWindowType */
  if (sink->texture_id)
    return TRUE;

  /* Create a new Ubuntu Application API session */
  if (sink->session == NULL)
    sink->session = create_session ();

  if (sink->session == NULL) {
    GST_ELEMENT_ERROR (bsink, RESOURCE, OPEN_READ_WRITE,
        ("Could not initialize Mir output"),
        ("Could not start a Mir app session"));
    return FALSE;
  }

  if (sink->display == NULL)
    sink->display = create_display ();

  if (sink->display == NULL) {
    GST_ELEMENT_ERROR (bsink, RESOURCE, OPEN_READ_WRITE,
        ("Could not initialize Mir output"),
        ("Could not create a Mir display"));
    return FALSE;
  }

  /* Create an EGLNativeWindowType instance so that a pure playbin
   * scenario will render video */
  if (sink->window == NULL) {
    sink->video_width = sink->display->width;
    sink->video_height = sink->display->height;
    GST_DEBUG_OBJECT (sink, "video_width: %d, video_height: %d",
        sink->video_width, sink->video_height);
    create_window (sink, sink->display, sink->video_width, sink->video_height);
    sink->surface_texture_client =
        surface_texture_client_create (sink->window->egl_native_window);
  }
#endif

  return TRUE;
}

static gboolean
gst_mir_sink_stop (GstBaseSink * bsink)
{
  GstMirSink *sink = (GstMirSink *) bsink;

  GST_DEBUG_OBJECT (sink, "stop");

  /* MediaCodec (hybris layer) will destroy the underlying
   * SurfaceTextureClientHybris instance, * so set it to NULL here so that
   * we don't point to an invalid instance.
   */
  sink->surface_texture_client = NULL;

  return TRUE;
}

static gboolean
gst_mir_sink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  GstMirSink *sink = GST_MIR_SINK (bsink);
  GstBufferPool *pool;
  GstStructure *config;
  GstCaps *caps;
  guint size = 0;
  gboolean need_pool;
  GstAllocator *allocator;
  GstAllocationParams params;

  GST_DEBUG_OBJECT (sink, "Proposing ALLOCATION params");

  gst_allocation_params_init (&params);

  gst_query_parse_allocation (query, &caps, &need_pool);
  if (!caps)
    goto no_caps;

  GST_OBJECT_LOCK (sink);
  pool = sink->pool ? gst_object_ref (sink->pool) : NULL;
  GST_OBJECT_UNLOCK (sink);

  GST_DEBUG_OBJECT (sink, "pool: %p, need_pool: %d", pool, need_pool);

  if (pool) {
    GstCaps *pcaps;
    GST_WARNING_OBJECT (sink, "already have a pool");

    /* We had a pool, check caps */
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_get_params (config, &pcaps, &size, NULL, NULL);

    if (!gst_caps_is_equal (caps, pcaps)) {
      /* Different caps, we can't use this pool */
      gst_object_unref (pool);
      pool = NULL;
    }
    gst_structure_free (config);
  }

  if (pool == NULL && need_pool) {
    GstVideoInfo info;
    info.size = 0;

    if (!gst_video_info_from_caps (&info, caps))
      goto invalid_caps;

    GST_DEBUG_OBJECT (sink, "size: %d", size);
    GST_DEBUG_OBJECT (sink, "caps %" GST_PTR_FORMAT, caps);
    GST_DEBUG_OBJECT (sink, "create new pool");
    pool = gst_mir_buffer_pool_new (sink);

    /* The normal size of a frame */
    size = (info.size == 0) ? info.height * info.width : info.size;

    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (config, caps, size, 2, 0);
    if (!gst_buffer_pool_set_config (pool, config))
      goto config_failed;
  }

  if (pool) {
    /* We are doing hardware rendering */
    gst_mir_buffer_pool_set_hardware_rendering (pool, TRUE);

    // FIXME: How many buffers min do we need? It's 2 right now.
    GST_DEBUG_OBJECT (sink, "size: %d", size);
    GST_DEBUG_OBJECT (sink, "adding allocation pool");
    gst_query_add_allocation_pool (query, pool, size, 2, 0);
    gst_object_unref (pool);
  }

  /* First the default allocator */
  if (!gst_mir_image_memory_is_mappable ()) {
    allocator = gst_allocator_find (NULL);
    gst_query_add_allocation_param (query, allocator, &params);
    gst_object_unref (allocator);
  }

  allocator = gst_mir_image_allocator_obtain ();
  if (!gst_mir_image_memory_is_mappable ())
    params.flags |= GST_MEMORY_FLAG_NOT_MAPPABLE;
  gst_query_add_allocation_param (query, allocator, &params);
  gst_object_unref (allocator);

  //gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  return TRUE;

  /* ERRORS */
no_caps:
  {
    GST_DEBUG_OBJECT (bsink, "no caps specified");
    return FALSE;
  }
invalid_caps:
  {
    GST_DEBUG_OBJECT (bsink, "invalid caps specified");
    return FALSE;
  }
config_failed:
  {
    GST_DEBUG_OBJECT (bsink, "failed setting config");
    gst_object_unref (pool);
    return FALSE;
  }
}

static GstFlowReturn
gst_mir_sink_preroll (GstBaseSink * bsink, GstBuffer * buffer)
{
  GST_DEBUG_OBJECT (bsink, "preroll buffer %p", buffer);
  return gst_mir_sink_render (bsink, buffer);
}

static GstFlowReturn
gst_mir_sink_render (GstBaseSink * bsink, GstBuffer * buffer)
{
  GstMirSink *sink = GST_MIR_SINK (bsink);
  //GstVideoRectangle src, dst, res;
  GstBuffer *to_render;
  GstMirMeta *meta;
  //GstFlowReturn ret;

  GST_DEBUG_OBJECT (sink, "render buffer %p", buffer);

  meta = gst_buffer_get_mir_meta (buffer);

  if (meta && meta->sink == sink) {
    GST_LOG_OBJECT (sink, "buffer %p from our pool, writing directly", buffer);
    to_render = buffer;
  } else {
    //GstMapInfo src;
    GST_LOG_OBJECT (sink, "buffer %p not from our pool, copying", buffer);
    to_render = buffer;

#if 0
    if (!sink->pool)
      goto no_pool;

    if (!gst_buffer_pool_set_active (sink->pool, TRUE))
      goto activate_failed;

    ret = gst_buffer_pool_acquire_buffer (sink->pool, &to_render, NULL);
    if (ret != GST_FLOW_OK)
      goto no_buffer;

    gst_buffer_map (buffer, &src, GST_MAP_READ);
    gst_buffer_fill (to_render, 0, src.data, src.size);
    gst_buffer_unmap (buffer, &src);

    meta = gst_buffer_get_mir_meta (to_render);
#endif
  }

  g_signal_emit (G_OBJECT (bsink), frame_ready_signal, 0);

#if 0
  src.w = sink->video_width;
  src.h = sink->video_height;
  dst.w = sink->window->width;
  dst.h = sink->window->height;

  gst_video_sink_center_rect (src, dst, &res, FALSE);
#endif

  if (buffer != to_render)
    gst_buffer_unref (to_render);
  return GST_FLOW_OK;

#if 0
no_buffer:
  {
    GST_WARNING_OBJECT (sink, "could not create image");
    return ret;
  }
no_pool:
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, WRITE,
        ("Internal error: can't allocate images"),
        ("We don't have a bufferpool negotiated"));
    return GST_FLOW_ERROR;
  }
activate_failed:
  {
    GST_ERROR_OBJECT (sink, "failed to activate bufferpool.");
    ret = GST_FLOW_ERROR;
    return ret;
  }
#endif
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gstmir_debug, "mirsink", 0, " mir video sink");

  return gst_element_register (plugin, "mirsink", GST_RANK_MARGINAL,
      GST_TYPE_MIR_SINK);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    mirsink,
    "Mir Video Sink", plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
