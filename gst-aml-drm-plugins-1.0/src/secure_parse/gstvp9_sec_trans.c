/* GStreamer VP9 secure transformer
 *
 * Copyright (C) <2019> Amlogic Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include "gstvp9_sec_trans.h"
#include "gstsecmemallocator.h"

GST_DEBUG_CATEGORY_STATIC (gst_vp9_sec_trans_debug);
#define GST_CAT_DEFAULT gst_vp9_sec_trans_debug

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-vp9(" GST_CAPS_FEATURE_MEMORY_SECMEM_MEMORY ")"));

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-vp9(" GST_CAPS_FEATURE_MEMORY_DMABUF ")"));

#define gst_vp9_sec_trans_parent_class parent_class
G_DEFINE_TYPE(GstVp9SecTrans, gst_vp9_sec_trans, GST_TYPE_BASE_TRANSFORM);

static void             gst_vp9_sec_trans_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void             gst_vp9_sec_trans_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static gboolean         gst_vp9_sec_trans_start(GstBaseTransform *trans);
static gboolean         gst_vp9_sec_trans_stop(GstBaseTransform *trans);
static GstCaps*         gst_vp9_sec_trans_transform_caps(GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps, GstCaps *filter);
static GstFlowReturn    gst_vp9_sec_trans_transform_ip(GstBaseTransform *trans, GstBuffer *buf);


static void
gst_vp9_sec_trans_class_init (GstVp9SecTransClass * klass)
{
    GObjectClass *gobject_class = (GObjectClass *) klass;
    GstElementClass *element_class = (GstElementClass *) klass;
    GstBaseTransformClass *base_class = GST_BASE_TRANSFORM_CLASS(klass);

    GST_DEBUG_CATEGORY_INIT(gst_vp9_sec_trans_debug, "vp9_sec_trans", 0, "Vp9 Secure transformer");
    gobject_class->set_property = gst_vp9_sec_trans_set_property;
    gobject_class->get_property = gst_vp9_sec_trans_get_property;
    base_class->start = GST_DEBUG_FUNCPTR(gst_vp9_sec_trans_start);
    base_class->stop = GST_DEBUG_FUNCPTR(gst_vp9_sec_trans_stop);
    base_class->transform_caps = GST_DEBUG_FUNCPTR(gst_vp9_sec_trans_transform_caps);
    base_class->transform_ip = GST_DEBUG_FUNCPTR(gst_vp9_sec_trans_transform_ip);
    base_class->passthrough_on_same_caps = FALSE;
    base_class->transform_ip_on_passthrough = FALSE;

    gst_element_class_add_pad_template (element_class,
            gst_static_pad_template_get (&sinktemplate));
    gst_element_class_add_pad_template (element_class,
            gst_static_pad_template_get (&srctemplate));

    gst_element_class_set_details_simple(element_class,
            "Secure VP9 convertor",
            "Codec/Parser/Video",
            "Amlogic Secure VP9 Plugin",
            "song.zhao@amlogic.com");
}

static void
gst_vp9_sec_trans_init(GstVp9SecTrans * plugin)
{
    GstBaseTransform *base = GST_BASE_TRANSFORM (plugin);
    gst_base_transform_set_passthrough (base, FALSE);

    gst_base_transform_set_in_place(base, TRUE);
    gst_base_transform_set_passthrough(base, FALSE);
    gst_base_transform_set_gap_aware(base, FALSE);
}

void
gst_vp9_sec_trans_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
    //TODO
}

void
gst_vp9_sec_trans_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
    //TODO
}

gboolean
gst_vp9_sec_trans_start(GstBaseTransform *trans)
{
    //GstVp9SecTrans *plugin = GST_VP9_SEC_TRANS(trans);
    return TRUE;
}

gboolean
gst_vp9_sec_trans_stop(GstBaseTransform *trans)
{
    //GstVp9SecTrans *plugin = GST_VP9_SEC_TRANS(trans);
    return TRUE;
}

GstCaps*
gst_vp9_sec_trans_transform_caps(GstBaseTransform *trans, GstPadDirection direction,
        GstCaps *caps, GstCaps *filter)
{
    GstVp9SecTrans *plugin = GST_VP9_SEC_TRANS(trans);
    GstCaps *srccaps, *sinkcaps;
    GstCaps *ret = NULL;

    GST_DEBUG_OBJECT (plugin, "transform_caps direction:%d caps:%" GST_PTR_FORMAT, direction, caps);

    srccaps = gst_pad_get_pad_template_caps(GST_BASE_TRANSFORM_SRC_PAD(trans));
    sinkcaps = gst_pad_get_pad_template_caps(GST_BASE_TRANSFORM_SINK_PAD(trans));

    switch (direction) {
    case GST_PAD_SINK:
    {
        if (gst_caps_can_intersect(caps, sinkcaps)) {
            gint width, height;
            GstStructure *s = gst_structure_copy(gst_caps_get_structure (caps, 0));

            ret = gst_caps_copy(srccaps);
            s = gst_caps_get_structure (caps, 0);

            if (s) {
                width = 1920;
                if (gst_structure_has_field (s, "width")) {
                    gst_structure_get_int (s, "width", &width);
                }
                gst_caps_set_simple (ret, "width", G_TYPE_INT, width, NULL);
            }
            if (s) {
                height = 1080;
                if (gst_structure_has_field (s, "height")) {
                    gst_structure_get_int (s, "height", &height);
                }
                gst_caps_set_simple (ret, "height", G_TYPE_INT, height, NULL);
            }
        }
        GST_DEBUG_OBJECT (plugin, "ret caps:%" GST_PTR_FORMAT, ret);
        break;
    }
    case GST_PAD_SRC:
        if (gst_caps_can_intersect(caps, srccaps)) {
            ret = gst_caps_copy(caps);
            unsigned size = gst_caps_get_size(ret);
            for (unsigned i = 0; i < size; ++i) {
                gst_caps_set_features(ret, i, gst_caps_features_from_string(GST_CAPS_FEATURE_MEMORY_DMABUF));
            }
        }  else {
            ret = gst_caps_copy(srccaps);
        }
        break;
    default:
        g_assert_not_reached();
    }

    if (filter) {
        GstCaps *intersection;
        GST_DEBUG_OBJECT (plugin, "filter:%" GST_PTR_FORMAT, filter);
        intersection = gst_caps_intersect_full(filter, ret, GST_CAPS_INTERSECT_FIRST);
        gst_caps_unref(ret);
        ret = intersection;
    }

    GST_DEBUG_OBJECT (plugin, "transform_caps result:%" GST_PTR_FORMAT, ret);

    gst_caps_unref(srccaps);
    gst_caps_unref(sinkcaps);
    return ret;
}

static GstFlowReturn
gst_vp9_sec_trans_transform_ip(GstBaseTransform *trans, GstBuffer *buf)
{
    GstFlowReturn ret = GST_FLOW_ERROR;
    GstVp9SecTrans *plugin = GST_VP9_SEC_TRANS(trans);
    GstMemory *mem;
    gboolean res;

    mem = gst_buffer_peek_memory(buf, 0);
    g_return_val_if_fail(gst_is_secmem_memory(mem), ret);

    res = gst_secmem_parse_vp9(mem);
    g_return_val_if_fail(res == TRUE, ret);

    GST_LOG_OBJECT (plugin, "size:%d", gst_buffer_get_size(buf));
    ret = GST_FLOW_OK;
    return ret;
}
