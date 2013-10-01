plugin_LTLIBRARIES = libgstmirsink.la

libgstmirsink_la_SOURCES = gstmirsink.c mirpool.c
libgstmirsink_la_CFLAGS = $(GST_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) \
                               $(MIR_CFLAGS) \
                          -I../../gst-libs/
libgstmirsink_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) \
                               -lgstvideo-$(GST_API_VERSION) \
                               -lubuntu_application_api \
                               $(top_builddir)/gst-libs/gst/mir/libgstmiralloc-$(GST_API_VERSION).la \
                               -lmedia \
                               $(EGL_LIBS) $(EGLGLES_LIBS) \
                               $(MIR_LIBS)
libgstmirsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstmirsink_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)
include_HEADERS = mirpool.h gstmirsink.h
noinst_HEADERS = 
