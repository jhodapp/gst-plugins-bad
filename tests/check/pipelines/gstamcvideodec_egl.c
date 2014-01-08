/*
 * EGL functionality for gstamcvideodec
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

#include <gst/egl/egl.h>
#include <gst/check/gstcheck.h>

void
setup_egl (void)
{
  EGLDisplay egl_display;
  EGLConfig egl_config;
  EGLContext egl_context;
  EGLint major = 0, minor = 0;
  EGLint attribs[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE
  };
  EGLint n = 0;
  EGLint context_attribs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE
  };

  GST_INFO ("Initializing EGL");

  egl_display = eglGetDisplay (EGL_DEFAULT_DISPLAY);
  fail_if (egl_display == EGL_NO_DISPLAY);

  fail_if (eglInitialize (egl_display, &major, &minor) == EGL_FALSE);

  fail_if (eglChooseConfig (egl_display, attribs, &egl_config, 1,
          &n) == EGL_FALSE);

  egl_context =
      eglCreateContext (egl_display, egl_config, EGL_NO_CONTEXT,
      context_attribs);
  fail_if (egl_context == EGL_NO_CONTEXT);
}
