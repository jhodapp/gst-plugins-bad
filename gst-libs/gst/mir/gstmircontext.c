/*
 * Provides a struct for transfering context between elements
 * Copyright (C) 2013 Collabora Ltd.
 *   @author: Jim Hodapp <jim.hodapp@canonical.com>
 * *
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "gstmircontext.h"

#include <gst/gst.h>
#include <string.h>

#define GST_MIR_TYPE_CONTEXT \
    gst_mir_context_get_type()

GType
gst_mir_stc_get_type (void)
    G_GNUC_CONST;

G_DEFINE_BOXED_TYPE (GstMirContext, gst_mir_context,
    (GBoxedCopyFunc) gst_mir_context_ref,
    (GBoxedFreeFunc) gst_mir_context_unref)

     GstMirContext *gst_mir_context_new (GstElement * element,
    SurfaceTextureClientHybris surface_texture_client)
{
  GstMirContext *ctx = g_new0 (GstMirContext, 1);

  ctx->element = gst_object_ref (element);
  ctx->surface_texture_client = surface_texture_client;

  GST_DEBUG_OBJECT (element, "ctx->surface_texture_client: %p",
      ctx->surface_texture_client);

  return ctx;
}

GstContext *
gst_mir_context_new_with_stc (GstMirContext * gst_mir_context)
{
  GstContext *context;
  GstStructure *structure;

  GST_DEBUG_OBJECT (gst_mir_context,
      "gst_mir_context->surface_texture_client: %p",
      gst_mir_context->surface_texture_client);

  /* Create a new GstContext, not persistent */
  context = gst_context_new (GST_MIR_CONTEXT_TYPE, TRUE);
  structure = gst_context_writable_structure (context);
  gst_structure_set (structure, "gst_mir_context", GST_MIR_TYPE_CONTEXT,
      gst_mir_context, NULL);
  return context;
}

void
gst_mir_context_free (GstMirContext * ctx)
{
  gst_object_unref (ctx->element);
  g_free (ctx);
}

gboolean
gst_is_mir_context (GstContext * ctx)
{
  g_return_val_if_fail (ctx != NULL, FALSE);
  return g_strcmp0 (gst_context_get_context_type (ctx),
      GST_MIR_CONTEXT_TYPE) == 0;
}

GstMirContext *
gst_mir_context_ref (GstMirContext * context)
{
  g_return_val_if_fail (context != NULL, NULL);

  g_atomic_int_inc (&context->ref_count);
  return context;
}

void
gst_mir_context_unref (GstMirContext * context)
{
  g_return_if_fail (context != NULL);
  g_return_if_fail (context->ref_count > 0);

  if (g_atomic_int_dec_and_test (&context->ref_count))
    gst_mir_context_free (context);
}

static gboolean
context_pad_query (const GValue * item, GValue * value, gpointer data)
{
  GstPad *const pad = g_value_get_object (item);
  GstQuery *const query = data;

  if (gst_pad_peer_query (pad, query)) {
    g_value_set_boolean (value, TRUE);
    return FALSE;
  }

  return TRUE;
}

static gboolean
do_context_query (GstElement * element, GstQuery * query)
{
  GstIteratorFoldFunction const func = context_pad_query;
  GstIterator *it;
  GValue res = { 0 };

  g_value_init (&res, G_TYPE_BOOLEAN);
  g_value_set_boolean (&res, FALSE);

  GST_DEBUG_OBJECT (element, "Querying downstream elements for CONTEXT query");
  /* First check downstream elements for a CONTEXT query reply */
  it = gst_element_iterate_src_pads (element);
  while (gst_iterator_fold (it, func, &res, query) == GST_ITERATOR_RESYNC)
    gst_iterator_resync (it);

  gst_iterator_free (it);

  /* If we got a reply from a downstream element, return success now */
  if (g_value_get_boolean (&res))
    return TRUE;

  GST_DEBUG_OBJECT (element, "Querying upstream elements for CONTEXT query");
  /* Now try upstream elements */
  it = gst_element_iterate_sink_pads (element);
  while (gst_iterator_fold (it, func, &res, query) == GST_ITERATOR_RESYNC)
    gst_iterator_resync (it);

  gst_iterator_free (it);

  return g_value_get_boolean (&res);
}

void
gst_mir_do_context_query (gpointer element)
{
  GstQuery *query;
  GstMessage *msg;

  g_return_if_fail (GST_IS_ELEMENT (element));

  GST_WARNING_OBJECT (element, "Sending new CONTEXT query");
  /* Get a pointer to a SurfaceTextureClientHybris instance to configure MediaCodec with */
  query = gst_query_new_context (GST_MIR_CONTEXT_TYPE);
  if (do_context_query (element, query)) {
    GstContext *context = NULL;

    GST_WARNING_OBJECT (element, "Got a successful reply to the CONTEXT query");
    gst_query_parse_context (query, &context);
    gst_element_set_context (element, context);
  } else {
    /* Second, try to get a context from mirsink via a NEED_CONTEXT bus message */
    GST_WARNING_OBJECT (element, "Sending new NEED_CONTEXT bus message");
    msg = gst_message_new_need_context (GST_OBJECT_CAST (element),
        GST_MIR_CONTEXT_TYPE);
    gst_element_post_message (GST_ELEMENT_CAST (element), msg);
  }
  gst_query_unref (query);
}

gboolean
gst_mir_ensure_surface_texture_client (gpointer element)
{
  g_return_val_if_fail (GST_IS_ELEMENT (element), FALSE);

  GST_DEBUG_OBJECT (element, "Calling gst_mir_do_context_query()");
  /* Otherwise check the neighboring elements and app for a GstContext */
  gst_mir_do_context_query (element);

  return TRUE;
}

SurfaceTextureClientHybris
gst_context_get_surface_texture_client (GstContext * context)   //,
//    SurfaceTextureClientHybris * surface_texture_client)
{
  const GstStructure *s;
  const GstMirContext *gst_mir_context;
  SurfaceTextureClientHybris surface_texture_client;

  GST_WARNING_OBJECT (context, "%s", __PRETTY_FUNCTION__);

  g_return_val_if_fail (GST_IS_CONTEXT (context), FALSE);
  g_return_val_if_fail (strcmp (gst_context_get_context_type (context),
          GST_MIR_CONTEXT_TYPE) == 0, FALSE);

  s = gst_context_get_structure (context);
  if (gst_structure_get (s, "gst_mir_context", GST_MIR_TYPE_CONTEXT,
          &gst_mir_context, NULL)) {
    surface_texture_client = gst_mir_context->surface_texture_client;
    GST_DEBUG_OBJECT (context, "gst_mir_context->surface_texture_client: %p",
        gst_mir_context->surface_texture_client);
    GST_DEBUG_OBJECT (context, "surface_texture_client: %p",
        surface_texture_client);
    return surface_texture_client;
  }

  return NULL;
}
