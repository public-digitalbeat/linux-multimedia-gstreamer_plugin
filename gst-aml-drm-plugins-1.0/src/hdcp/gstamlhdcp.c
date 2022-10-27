/*
 * gstamlhdcp.c
 *
 *  Created on: Feb 5, 2020
 *      Author: tao
 */



#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <stdbool.h>
#include "gstamlhdcp.h"
#include "secmem_ca.h"
#include "gstsecmemallocator.h"

GST_DEBUG_CATEGORY_STATIC (gst_aml_hdcp_debug);
#define GST_CAT_DEFAULT gst_aml_hdcp_debug

#define GST_STATIC_CAPS_SINK GST_STATIC_CAPS("ANY")
static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-hdcp, original-media-type=(string)video/x-h264; "
                     "application/x-hdcp, original-media-type=(string)audio/mpeg"));

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);


#define gst_aml_hdcp_parent_class parent_class
G_DEFINE_TYPE(GstAmlHDCP, gst_aml_hdcp, GST_TYPE_BASE_TRANSFORM);

static void             gst_aml_hdcp_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void             gst_aml_hdcp_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static gboolean         gst_aml_hdcp_start(GstBaseTransform *trans);
static gboolean         gst_aml_hdcp_stop(GstBaseTransform *trans);
static GstCaps*         gst_aml_hdcp_transform_caps(GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps, GstCaps *filter);
static gboolean         gst_aml_hdcp_propose_allocation(GstBaseTransform *trans, GstQuery *decide_query, GstQuery *query);
static GstFlowReturn    gst_aml_prepare_output_buffer(GstBaseTransform * trans, GstBuffer *input, GstBuffer **outbuf);
static GstFlowReturn    gst_aml_hdcp_transform(GstBaseTransform *trans, GstBuffer *inbuf, GstBuffer *outbuf);
static GstFlowReturn    drm_hdcp_decrypt_all(GstAmlHDCP *plugin, GstBuffer *inbuf, GstBuffer *outbuf, unsigned char *iv, unsigned int iv_len);


static void
gst_aml_hdcp_class_init (GstAmlHDCPClass * klass)
{
    GObjectClass *gobject_class = (GObjectClass *) klass;
    GstElementClass *element_class = (GstElementClass *) klass;
    GstBaseTransformClass *base_class = GST_BASE_TRANSFORM_CLASS(klass);

    gobject_class->set_property = gst_aml_hdcp_set_property;
    gobject_class->get_property = gst_aml_hdcp_get_property;
    base_class->start = GST_DEBUG_FUNCPTR(gst_aml_hdcp_start);
    base_class->stop = GST_DEBUG_FUNCPTR(gst_aml_hdcp_stop);
    base_class->transform_caps = GST_DEBUG_FUNCPTR(gst_aml_hdcp_transform_caps);
    base_class->transform = GST_DEBUG_FUNCPTR(gst_aml_hdcp_transform);
    base_class->propose_allocation = GST_DEBUG_FUNCPTR(gst_aml_hdcp_propose_allocation);
    base_class->prepare_output_buffer = GST_DEBUG_FUNCPTR(gst_aml_prepare_output_buffer);
    base_class->passthrough_on_same_caps = FALSE;
    base_class->transform_ip_on_passthrough = FALSE;

    gst_element_class_add_pad_template (element_class,
            gst_static_pad_template_get (&sinktemplate));
    gst_element_class_add_pad_template (element_class,
            gst_static_pad_template_get (&srctemplate));

    g_object_class_install_property(gobject_class, PROP_DRM_STATIC_PIPELINE,
                                    g_param_spec_boolean("static-pipeline", "static pipeline",
                                                         "whether static pipeline", FALSE, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_DRM_HDCP_CONTEXT,
                                    g_param_spec_pointer("set-hdcp-context", "set-hdcp-context",
                                                      "set the hdcp-context", G_PARAM_READWRITE));

    gst_element_class_set_details_simple(element_class,
            "Amlogic HDCP Plugin",
            "Decryptor/Converter/DRM/HDCP",
            "HDCP Decryption Plugin",
            "mm@amlogic.com");
}

static void
gst_aml_hdcp_init(GstAmlHDCP * plugin)
{
    GstBaseTransform *base = GST_BASE_TRANSFORM (plugin);
    gst_base_transform_set_passthrough (base, FALSE);

    plugin->allocator = NULL;
    plugin->outcaps = NULL;
    plugin->static_pipeline = FALSE;
    plugin->hdcp_init_done = -1;
    plugin->hdcp_context = NULL;
    plugin->hdcp_handle = NULL;
}

void
gst_aml_hdcp_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
    GstAmlHDCP *plugin = GST_AMLHDCP(object);
    GST_DEBUG_OBJECT (object, "gst_aml_hdcp_set_property property:%d \n", prop_id);

    switch(prop_id) {
    case PROP_DRM_STATIC_PIPELINE:
        plugin->static_pipeline = g_value_get_boolean(value);
        break;
    case PROP_DRM_HDCP_CONTEXT:
        plugin->hdcp_context = (amlWfdHdcpHandle )g_value_get_pointer(value);
        plugin->hdcp_init_done = 0;
        GST_DEBUG_OBJECT(object, "gst_aml_hdcp_set_property set done:%p \n", plugin->hdcp_context);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

void
gst_aml_hdcp_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
    GstAmlHDCP *plugin = GST_AMLHDCP(object);

    switch (prop_id) {
    case PROP_DRM_STATIC_PIPELINE:
    break;
    case PROP_DRM_HDCP_CONTEXT:
    break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

gboolean
gst_aml_hdcp_start(GstBaseTransform *trans)
{
    return TRUE;
}

gboolean
gst_aml_hdcp_stop(GstBaseTransform *trans)
{
    GstAmlHDCP *plugin = GST_AMLHDCP(trans);

    if (plugin->allocator) {
        gst_object_unref(plugin->allocator);
    }
    if (plugin->outcaps) {
        gst_caps_unref(plugin->outcaps);
    }
    plugin->hdcp_init_done = -1;
    return TRUE;
}

/*
  Append new_structure to dest, but only if it does not already exist in res.
  This function takes ownership of new_structure.
*/
static bool AppendIfNotDuplicate(GstCaps* destination, GstStructure* structure)
{
    bool duplicate = false;
    unsigned size = gst_caps_get_size(destination);
    for (unsigned index = 0; !duplicate && index < size; ++index) {
        GstStructure* s = gst_caps_get_structure(destination, index);
        if (gst_structure_is_equal(s, structure))
            duplicate = true;
    }

    if (!duplicate)
        gst_caps_append_structure(destination, structure);
    else
        gst_structure_free(structure);

    return duplicate;
}

GstCaps*
gst_aml_hdcp_transform_caps(GstBaseTransform *trans, GstPadDirection direction,
        GstCaps *caps, GstCaps *filter)
{
    GstAmlHDCP *plugin = GST_AMLHDCP(trans);
    GstCaps *transformed_caps = gst_caps_new_empty();
    guint incoming_caps_size;
    guint i;
    guint index;
    gboolean isvideo = TRUE;

    if (direction == GST_PAD_UNKNOWN)
        return NULL;

    GST_DEBUG_OBJECT (plugin,
        "direction: %s, caps: %" GST_PTR_FORMAT " filter: %" GST_PTR_FORMAT,
        (direction == GST_PAD_SRC) ? "src" : "sink", caps, filter);

    incoming_caps_size = gst_caps_get_size (caps);

    if (direction == GST_PAD_SINK) {
        for (i = 0; i < incoming_caps_size; ++i) {
            GstStructure *incoming_structure = gst_caps_get_structure (caps, i);
            GstStructure *outgoing_structure = NULL;

            if (!gst_structure_has_field (incoming_structure, "original-media-type"))
                continue;

            const gchar *media_type = gst_structure_get_string (incoming_structure, "original-media-type");
            isvideo = g_str_has_prefix (media_type, "video");

            outgoing_structure = gst_structure_copy (incoming_structure);
            gst_structure_set_name (outgoing_structure, gst_structure_get_string (outgoing_structure, "original-media-type"));
            GST_DEBUG_OBJECT(plugin, "transform_caps direction:%d caps:%" GST_PTR_FORMAT, direction, outgoing_structure);

            gint n_fields = gst_structure_n_fields (incoming_structure);

            /* filter out the DRM related fields from the down-stream caps */
            for (gint j = 0; j < n_fields; ++j) {
                const gchar *field_name;
                field_name = gst_structure_nth_field_name (incoming_structure, j);

                if (g_str_has_prefix(field_name, "protection-system")
                    || g_str_has_prefix(field_name, "original-media-type")
                    || g_str_has_prefix(field_name, "pixel-aspect-ratio"))
                    gst_structure_remove_field(outgoing_structure, field_name);
            }

            AppendIfNotDuplicate(transformed_caps, outgoing_structure);
            GST_DEBUG_OBJECT(plugin, "transform_caps direction:%d caps:%" GST_PTR_FORMAT, direction, transformed_caps);

            if (isvideo) //for drm sec
                gst_caps_set_features (transformed_caps, i, gst_caps_features_from_string(GST_CAPS_FEATURE_MEMORY_SECMEM_MEMORY));
        }
    } else {
        if (!gst_caps_is_empty(caps) && !gst_caps_is_any(caps)) {
            for (int i = 0; i < incoming_caps_size; i++) {
                GstStructure* in = gst_caps_get_structure(caps, i);
                GstStructure* out = NULL;
                GstStructure* tmp = gst_structure_copy(in);

                /* filter out the video/audio related fields from the up-stream caps */
                for (int index = gst_structure_n_fields(tmp) - 1; index >= 0; --index) {
                    const gchar* fieldName = gst_structure_nth_field_name(tmp, index);
                    if (!g_strcmp0(fieldName, "base-profile")
                        || !g_strcmp0(fieldName, "codec_data")
                        || !g_strcmp0(fieldName, "mpegversion")
                        || !g_strcmp0(fieldName, "stream-format")
                        || !g_strcmp0(fieldName, "framed")
                        || !g_strcmp0(fieldName, "height")
                        || !g_strcmp0(fieldName, "framerate")
                        || !g_strcmp0(fieldName, "level")
                        || !g_strcmp0(fieldName, "pixel-aspect-ratio")
                        || !g_strcmp0(fieldName, "channels")
                        || !g_strcmp0(fieldName, "profile")
                        || !g_strcmp0(fieldName, "rate")
                        || !g_strcmp0(fieldName, "width")) {
                        gst_structure_remove_field(tmp, fieldName);
                    }
                }
                out = gst_structure_copy(tmp);
                gst_structure_set(out, "original-media-type", G_TYPE_STRING, gst_structure_get_name(in), NULL);
                gst_structure_set_name(out, "application/x-hdcp");
                AppendIfNotDuplicate(transformed_caps, out);
                gst_structure_free(tmp);
            }
        } else {
            transformed_caps = gst_caps_new_empty_simple("application/x-hdcp");
            gst_caps_set_simple(transformed_caps, "original-media-type", G_TYPE_STRING, "video/x-h264", NULL);
            gst_caps_append_structure(transformed_caps, gst_structure_new ("application/x-hdcp",
                                    "original-media-type", G_TYPE_STRING, "audio/mpeg", NULL));
        }
    }

    if (filter) {
        GstCaps *intersection;
        GST_DEBUG_OBJECT (plugin, "Using filter caps %" GST_PTR_FORMAT, filter);
        intersection =
            gst_caps_intersect_full (transformed_caps, filter,
            GST_CAPS_INTERSECT_FIRST);
        gst_caps_replace (&transformed_caps, intersection);
    }
    GST_DEBUG_OBJECT (plugin, "amlhdcp returning %" GST_PTR_FORMAT, transformed_caps);
    return transformed_caps;
}

gboolean
gst_aml_hdcp_propose_allocation(GstBaseTransform *trans, GstQuery *decide_query, GstQuery *query)
{
    return GST_BASE_TRANSFORM_CLASS (parent_class)->propose_allocation(trans, decide_query, query);
}


GstFlowReturn
gst_aml_prepare_output_buffer(GstBaseTransform * trans, GstBuffer *inbuf, GstBuffer **outbuf)
{
    GstAmlHDCP *plugin = GST_AMLHDCP(trans);
    GstFlowReturn ret = GST_FLOW_OK;
    gboolean isvideo = FALSE;
    gboolean secure = FALSE;
    GstProtectionMeta *meta = gst_buffer_get_protection_meta(inbuf);
    if (meta) {
        GST_DEBUG_OBJECT(plugin, "gst_aml_prepare_output_buffer meta:%" GST_PTR_FORMAT, meta->info);
        gst_structure_get_boolean(meta->info, "isvideo", &isvideo);
        gst_structure_get_boolean(meta->info, "secure", &secure);
    }

    if (!isvideo || !secure) {
        *outbuf = gst_buffer_new_allocate(NULL, gst_buffer_get_size(inbuf), NULL);
    } else {
        if (!plugin->allocator)
            plugin->allocator = gst_secmem_allocator_new(true, false);

        g_return_val_if_fail(plugin->allocator != NULL, GST_FLOW_ERROR);
        *outbuf = gst_buffer_new_allocate(plugin->allocator, gst_buffer_get_size(inbuf), NULL);
    }

    GST_BASE_TRANSFORM_CLASS(parent_class)->copy_metadata(trans, inbuf, *outbuf);
    return ret;
}

GstFlowReturn
gst_aml_hdcp_transform(GstBaseTransform *trans, GstBuffer *inbuf, GstBuffer *outbuf)
{
    GstAmlHDCP *plugin = GST_AMLHDCP(trans);
    GstFlowReturn ret = GST_FLOW_OK;
    gboolean secure = FALSE;
    gboolean isvideo = FALSE;
    GstBuffer *iv_buffer;
    GValue *value;
    GstMapInfo map;

    GstProtectionMeta *meta = gst_buffer_get_protection_meta(inbuf);
    if (meta) {
        GstStructure *drm_info = meta->info;
        gst_structure_get_boolean(meta->info, "isvideo", &isvideo);
        if (!gst_structure_get(drm_info, "secure", G_TYPE_BOOLEAN,
                &secure, NULL)) {
            GST_INFO_OBJECT(plugin, "No drm_info, take it as clean!!!");
        } else {
            GST_DEBUG_OBJECT(plugin, "Found drm_info in ProtectionMeta.");
            value = gst_structure_get_value(drm_info, "iv");
            iv_buffer = gst_value_get_buffer (value);

            if (iv_buffer == NULL) {
                GST_ERROR_OBJECT(plugin, "Get iv from drm_info fail!!!");
                return GST_FLOW_ERROR;
            }

            GST_DEBUG_OBJECT(plugin, "Get iv from drm_info successfully");

            GST_INFO_OBJECT(plugin, "IV Length: %d", gst_buffer_get_size(iv_buffer));
        }
    } else {
        GST_INFO_OBJECT(plugin, "No ProtectionMeta, take it as clean data.");
    }

    if (secure) {
        gst_buffer_map(iv_buffer, &map, GST_MAP_READ);
        if (drm_hdcp_decrypt_all(plugin, inbuf, outbuf, map.data,
                map.size) == GST_FLOW_OK) {
            GST_INFO_OBJECT(plugin, "decrypt and push data seccessfully. ");
            ret = GST_FLOW_OK;
        } else {
            GST_ERROR_OBJECT(plugin, "decryption process error !!!");
            ret = GST_FLOW_ERROR;
        }
        gst_buffer_unmap(iv_buffer, &map);
    } else if(isvideo) { // for have hdcp protocol but no iv
        ret = gst_buffer_copy_to_secmem(outbuf, inbuf) ? GST_FLOW_OK : GST_FLOW_ERROR;
        GST_INFO_OBJECT(plugin, "for have hdcp protocol but no iv secmem ret %d", ret);
    } else {
        GstMapInfo inmap, outmap;
        gst_buffer_map(inbuf, &inmap, GST_MAP_READ);
        gst_buffer_map(outbuf, &outmap, GST_MAP_READWRITE);
        memcpy(outmap.data, inmap.data, inmap.size);

        gst_buffer_unmap(inbuf, &inmap);
        gst_buffer_unmap(outbuf, &outmap);
        ret = GST_FLOW_OK;
    }
    return ret;
}

unsigned long long BitField(unsigned int val, unsigned char start, unsigned char size)
{
    unsigned char start_bit;
    unsigned char bit_count;
    unsigned int mask;
    unsigned int value;

    start_bit=start;
    bit_count= size;
    // generate mask
    if (bit_count == 32)
    {
        mask = 0xffffffff;
    }
    else
    {
        mask = 1;
        mask = mask << bit_count;
        mask = mask -1;
        mask = mask << start_bit;
    }
    value = val;
    unsigned long long rev = (((unsigned long long)(value & mask)) >> start_bit);
    return rev;
}

unsigned int
drm_hdcp_calc_stream_ctr(unsigned char *iv)
{
    unsigned int stream_ctr = 0;
    stream_ctr = (unsigned int)   (BitField(iv[0 + 1], 1, 2) << 30)
                                | (BitField(iv[0 + 2], 0, 8) << 22)
                                | (BitField(iv[0 + 3], 1, 7) << 15)
                                | (BitField(iv[0 + 4], 0, 8) << 7)
                                | (BitField(iv[0 + 5], 1, 7));

    return stream_ctr;
}


unsigned long long
drm_hdcp_calc_input_ctr(unsigned char *iv)
{
    unsigned long long input_ctr = 0;
    input_ctr = /*(unsigned long long)*/  (BitField(iv[0 + 7], 1, 4) << 60)
                                        | (BitField(iv[0 + 8], 0, 8) << 52)
                                        | (BitField(iv[0 + 9], 1, 7) << 45)
                                        | (BitField(iv[0 + 10], 0, 8) << 37)
                                        | (BitField(iv[0 + 11], 1, 7) << 30)
                                        | (BitField(iv[0 + 12], 0, 8) << 22)
                                        | (BitField(iv[0 + 13], 1, 7) << 15)
                                        | (BitField(iv[0 + 14], 0, 8) << 7)
                                        | (BitField(iv[0 + 15], 1, 7));

    return input_ctr;
}

amlWfdHdcpResultType drm_hdcp_init(GstAmlHDCP* drm)
{
    amlWfdHdcpResultType ret = HDCP_RESULT_SUCCESS;
    if (drm->static_pipeline == TRUE) {
        int hdcp_receiver_port=6789;
        ret = amlWfdHdcpInit((const char *)"127.0.0.1", hdcp_receiver_port, &drm->hdcp_handle);
        if (ret) {
            GST_ERROR("amlWfdHdcpInit failed  %d", ret);
            goto beach;
        }
        if (drm->hdcp_init_done == -1) {
            drm->hdcp_init_done = 0;
        }
    } else {
        GST_DEBUG_OBJECT(drm,
                "integrate_hdcp_log: Just print set-hdcp-context : hdcp_context = %p",
                drm->hdcp_context);
    }

beach:
    GST_DEBUG_OBJECT(drm, "drm_hdcp_init Out with ret=%d", ret);
    return ret;
}

static GstFlowReturn drm_hdcp_decrypt_trust_zone(GstAmlHDCP* drm, amlWfdHdcpHandle *hdcp, GstBuffer *inbuf, GstBuffer *outbuf, unsigned int stream_ctr, unsigned long long input_ctr)
{
    GstFlowReturn ret;
    GstMapInfo map;
    GstMapInfo outmap;
    struct amlWfdHdcpDataInfo info = {0};
    GstMemory *mem;
    gsize size;
    gsize outsize;
    amlWfdHdcpResultType err;
    unsigned long paddr;
    gboolean isvideo = FALSE;

    GstProtectionMeta *meta = gst_buffer_get_protection_meta(inbuf);
    if (meta)
        gst_structure_get_boolean(meta->info, "isvideo", &isvideo);

    if (isvideo) {
        mem = gst_buffer_peek_memory(outbuf, 0);
        g_return_val_if_fail(gst_is_secmem_memory(mem), GST_FLOW_ERROR);
        size = gst_memory_get_sizes(mem, NULL, NULL);
        paddr = (unsigned long)gst_secmem_memory_get_handle(mem);
        info.isAudio = 0;
        info.out = (uint8_t *)paddr;
    } else {
        gst_buffer_map(outbuf, &outmap, GST_MAP_READWRITE);
        size = outmap.size;
        info.isAudio = 1;
        info.out = outmap.data;
    }

    gst_buffer_map(inbuf, &map, GST_MAP_READ);
    if (map.size != size) {
        GST_ERROR("buffer size not match");
        goto beach;
    }
    info.in = map.data;
    info.inSize = map.size;
    info.outSize = map.size;
    info.streamCtr = stream_ctr;
    info.inputCtr = input_ctr;
    err = amlWfdHdcpDecrypt(*hdcp, &info);
    if (err) {
        GST_ERROR("amlWfdHdcpDecrypt failed %d", err);
        goto beach;
    }
    ret = GST_FLOW_OK;

beach:
    gst_buffer_unmap(inbuf, &map);
    return ret;
}

GstFlowReturn drm_hdcp_decrypt_all(GstAmlHDCP *plugin, GstBuffer *inbuf, GstBuffer *outbuf, unsigned char *iv, unsigned int iv_len)
{
    int retry = 10;
    GstFlowReturn ret = GST_FLOW_ERROR;
    amlWfdHdcpHandle *hdcp;
    if (plugin->hdcp_init_done != 0) {
        if (plugin->static_pipeline) {
            if (drm_hdcp_init(plugin) != HDCP_RESULT_SUCCESS) {
                GST_ERROR_OBJECT(plugin,
                        "static-pipeline drm_hdcp_init() fail!!!!!!");
                return GST_FLOW_ERROR;
            }
        } else {
            while (retry-- >= 0) {
                sleep(1);
                if (plugin->hdcp_init_done == 0) {
                    break;
                }
            }
            if (plugin->hdcp_init_done != 0) {
                GST_ERROR_OBJECT(plugin,
                        "No hdcp context set by application (WFD), fatal error!!!!!!");
                return GST_FLOW_ERROR;
            }
        }
    }
    hdcp = (plugin->static_pipeline) ? &plugin->hdcp_handle : plugin->hdcp_context;
    unsigned int stream_ctr = drm_hdcp_calc_stream_ctr(iv);
    unsigned long long input_ctr = drm_hdcp_calc_input_ctr(iv);

    ret = drm_hdcp_decrypt_trust_zone(plugin, hdcp, inbuf, outbuf, stream_ctr, input_ctr);

    return ret;
}

#ifndef PACKAGE
#define PACKAGE "gst-plugins-amlogic"
#endif

static gboolean
amlhdcp_init (GstPlugin * amlhdcp)
{
    GST_DEBUG_CATEGORY_INIT(gst_aml_hdcp_debug, "amlhdcp", 0, "Amlogic HDCP Plugin");

    return gst_element_register(amlhdcp, "amlhdcp", GST_RANK_PRIMARY, GST_TYPE_AMLHDCP);
}


GST_PLUGIN_DEFINE(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    amlhdcp,
    "Gstreamer HDCP plugin",
    amlhdcp_init,
    VERSION,
    "LGPL",
    "gst-plugins-drmhdcp",
    "http://amlogic.com/"
)
