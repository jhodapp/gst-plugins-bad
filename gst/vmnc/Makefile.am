plugin_LTLIBRARIES = libgstvmnc.la

libgstvmnc_la_SOURCES = vmncdec.c
libgstvmnc_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgstvmnc_la_LIBADD = $(GST_BASE_LIBS) $(GST_LIBS)
libgstvmnc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstvmnc_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

Android.mk: Makefile.am $(BUILT_SOURCES)
	androgenizer \
	-:PROJECT libgstvmnc -:SHARED libgstvmnc \
	 -:TAGS eng debug \
         -:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
	 -:SOURCES $(libgstvmnc_la_SOURCES) \
	 -:CFLAGS $(DEFS) $(DEFAULT_INCLUDES) $(libgstvmnc_la_CFLAGS) \
	 -:LDFLAGS $(libgstvmnc_la_LDFLAGS) \
	           $(libgstvmnc_la_LIBADD) \
	           -ldl \
	 -:PASSTHROUGH LOCAL_ARM_MODE:=arm \
		       LOCAL_MODULE_PATH:='$$(TARGET_OUT)/lib/gstreamer-0.10' \
	> $@