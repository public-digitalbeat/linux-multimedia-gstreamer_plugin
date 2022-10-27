/*
 * gstdummydrm.c
 *
 *  Created on: 2020年4月1日
 *      Author: tao
 */


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <stdbool.h>
#include "gstdummydrm.h"
#include "gstsecmemallocator.h"



GST_DEBUG_CATEGORY_STATIC (gst_dummydrm_debug);
#define GST_CAT_DEFAULT gst_dummydrm_debug

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);


#define gst_dummydrm_parent_class parent_class
G_DEFINE_TYPE(GstDummyDrm, gst_dummydrm, GST_TYPE_BASE_TRANSFORM);

static void             gst_dummydrm_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void             gst_dummydrm_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static gboolean         gst_dummydrm_start(GstBaseTransform *trans);
static gboolean         gst_dummydrm_stop(GstBaseTransform *trans);
static GstCaps*         gst_dummydrm_transform_caps(GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps, GstCaps *filter);
static GstFlowReturn    gst_dummydrm_prepare_output_buffer(GstBaseTransform * trans, GstBuffer *input, GstBuffer **outbuf);
static GstFlowReturn    gst_dummydrm_transform(GstBaseTransform *trans, GstBuffer *inbuf, GstBuffer *outbuf);


static void
gst_dummydrm_class_init (GstDummyDrmClass * klass)
{
    GObjectClass *gobject_class = (GObjectClass *) klass;
    GstElementClass *element_class = (GstElementClass *) klass;
    GstBaseTransformClass *base_class = GST_BASE_TRANSFORM_CLASS(klass);

    gobject_class->set_property = gst_dummydrm_set_property;
    gobject_class->get_property = gst_dummydrm_get_property;
    base_class->start = GST_DEBUG_FUNCPTR(gst_dummydrm_start);
    base_class->stop = GST_DEBUG_FUNCPTR(gst_dummydrm_stop);
    base_class->transform_caps = GST_DEBUG_FUNCPTR(gst_dummydrm_transform_caps);
    base_class->transform = GST_DEBUG_FUNCPTR(gst_dummydrm_transform);
    base_class->prepare_output_buffer = GST_DEBUG_FUNCPTR(gst_dummydrm_prepare_output_buffer);
    base_class->passthrough_on_same_caps = FALSE;
    base_class->transform_ip_on_passthrough = FALSE;

    gst_element_class_add_pad_template (element_class,
            gst_static_pad_template_get (&sinktemplate));
    gst_element_class_add_pad_template (element_class,
            gst_static_pad_template_get (&srctemplate));

    gst_element_class_set_details_simple(element_class,
            "Amlogic Dummy DRM Plugin",
            "Filter/DRM/Dummy",
            "DRM Decryption Plugin",
            "mm@amlogic.com");
}

static void
gst_dummydrm_init(GstDummyDrm * plugin)
{
    GstBaseTransform *base = GST_BASE_TRANSFORM (plugin);
    gst_base_transform_set_passthrough (base, FALSE);

    plugin->allocator = NULL;
    plugin->outcaps = NULL;

}

void
gst_dummydrm_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
    //TODO
}

void
gst_dummydrm_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
    //TODO
}

gboolean
gst_dummydrm_start(GstBaseTransform *trans)
{
    GstDummyDrm *plugin = GST_DUMMYDRM(trans);


    return TRUE;
}
gboolean
gst_dummydrm_stop(GstBaseTransform *trans)
{
    GstDummyDrm *plugin = GST_DUMMYDRM(trans);

    if (plugin->allocator) {
        gst_object_unref(plugin->allocator);
    }
    if (plugin->outcaps) {
        gst_caps_unref(plugin->outcaps);
    }
    return TRUE;
}

GstCaps*
gst_dummydrm_transform_caps(GstBaseTransform *trans, GstPadDirection direction,
        GstCaps *caps, GstCaps *filter)
{
    GstDummyDrm *plugin = GST_DUMMYDRM(trans);
    GstCaps *srccaps, *sinkcaps;
    GstCaps *ret = NULL;

    GST_DEBUG_OBJECT (plugin, "transform_caps direction:%d caps:%" GST_PTR_FORMAT, direction, caps);

    srccaps = gst_pad_get_pad_template_caps(GST_BASE_TRANSFORM_SRC_PAD(trans));
    sinkcaps = gst_pad_get_pad_template_caps(GST_BASE_TRANSFORM_SINK_PAD(trans));

    switch (direction) {
    case GST_PAD_SINK:
    {
        if (gst_caps_can_intersect(caps, sinkcaps)) {
            gboolean find = false;
            unsigned size;

            ret = gst_caps_copy(caps);
            size = gst_caps_get_size(ret);
            for (unsigned i = 0; i < size; ++i) {
                GstStructure *structure;
                structure = gst_caps_get_structure(caps, i);
                if (g_str_has_prefix (gst_structure_get_name (structure), "video/")) {
                    find = true;
                    gst_caps_set_features(ret, i,
                            gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_SECMEM_MEMORY));
                }
            }
            if (find) {
                if (!plugin->allocator) {
                    plugin->allocator = gst_secmem_allocator_new(false, false);
                }
                if (plugin->outcaps) {
                    gst_caps_unref(plugin->outcaps);
                }

                plugin->outcaps = gst_caps_ref(ret);
            } else {
                gst_caps_unref(ret);
                ret = gst_caps_copy(sinkcaps);
            }
        } else {
            ret = gst_caps_copy(sinkcaps);
        }
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
        intersection = gst_caps_intersect_full(filter, ret, GST_CAPS_INTERSECT_FIRST);
        gst_caps_unref(ret);
        ret = intersection;
    }

    GST_DEBUG_OBJECT (plugin, "transform_caps result:%" GST_PTR_FORMAT, ret);

    gst_caps_unref(srccaps);
    gst_caps_unref(sinkcaps);
    return ret;
}

GstFlowReturn
gst_dummydrm_prepare_output_buffer(GstBaseTransform * trans, GstBuffer *inbuf, GstBuffer **outbuf)
{
    GstDummyDrm *plugin = GST_DUMMYDRM(trans);
    GstFlowReturn ret = GST_FLOW_OK;

    g_return_val_if_fail(plugin->allocator != NULL, GST_FLOW_ERROR);

    *outbuf = gst_buffer_new_allocate(plugin->allocator, gst_buffer_get_size(inbuf), NULL);

    g_return_val_if_fail(*outbuf != NULL, GST_FLOW_ERROR);
    GST_BASE_TRANSFORM_CLASS(parent_class)->copy_metadata (trans, inbuf, *outbuf);
    return ret;
}

static GstFlowReturn
gst_dummydrm_transform(GstBaseTransform *trans, GstBuffer *inbuf, GstBuffer *outbuf)
{
    GstFlowReturn ret = GST_FLOW_ERROR;
    GstMapInfo map;
    GstMemory *mem;
    gsize size;

    mem = gst_buffer_peek_memory(outbuf, 0);
    g_return_val_if_fail(gst_is_secmem_memory(mem), ret);

    gst_memory_get_sizes(mem, NULL, &size);
    gst_buffer_map(inbuf, &map, GST_MAP_READ);
    if (map.size > size) {
        GST_ERROR("buffer size not match");
        goto beach;
    }
    gst_secmem_fill(mem, 0, map.data, map.size);
    ret = GST_FLOW_OK;
beach:
    gst_buffer_unmap(inbuf, &map);
    return ret;
}

#ifndef PACKAGE
#define PACKAGE "gst-aml-drm-plugins"
#endif

static gboolean
dummydrm_init (GstPlugin * dummydrm)
{
    GST_DEBUG_CATEGORY_INIT(gst_dummydrm_debug, "dummydrm", 0, "Amlogic Dummy DRM Plugin");

    return gst_element_register(dummydrm, "dummydrm", GST_RANK_PRIMARY, GST_TYPE_DUMMYDRM);
}


GST_PLUGIN_DEFINE(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    dummydrm,
    "Gstreamer Dummy Drm plugin",
    dummydrm_init,
    VERSION,
    "LGPL",
    "gst-aml-drm-plugins",
    "http://amlogic.com/"
)
