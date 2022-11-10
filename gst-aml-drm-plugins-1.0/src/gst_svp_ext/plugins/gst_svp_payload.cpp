#include "gst_svp_payload.h"
#include <gst_svp_meta.h>
#include "gstsecmemallocator.h"

#define GST_CAPS_FEATURE_MEMORY_SECMEM_MEMORY "memory:SecMem"
#define GST_CAPS_FEATURE_MEMORY_DMABUF "memory:DMABuf"

struct _SvpPayloadPrivate {
  void* svpCtx;
};

GST_DEBUG_CATEGORY_STATIC(svp_payload_debug_category);
#define GST_CAT_DEFAULT svp_payload_debug_category

#define svp_payload_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE(SvpPayload, svp_payload, GST_TYPE_BASE_TRANSFORM);

static GstStaticPadTemplate sinkTemplate =
  GST_STATIC_PAD_TEMPLATE(
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(
        "video/x-h264; "
        "video/x-h265; "
        "video/x-vp9; "
        "video/x-av1; "));

static GstStaticPadTemplate srcTemplate =
  GST_STATIC_PAD_TEMPLATE(
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(
        "video/x-h264(" GST_CAPS_FEATURE_MEMORY_SECMEM_MEMORY "); "
        "video/x-h264(" GST_CAPS_FEATURE_MEMORY_DMABUF "); "
        "video/x-h265(" GST_CAPS_FEATURE_MEMORY_SECMEM_MEMORY "); "
        "video/x-h265(" GST_CAPS_FEATURE_MEMORY_DMABUF "); "
        "video/x-vp9(" GST_CAPS_FEATURE_MEMORY_SECMEM_MEMORY "); "
        "video/x-av1(" GST_CAPS_FEATURE_MEMORY_SECMEM_MEMORY "); "));

static GstFlowReturn svpPayloadTransform(GstBaseTransform* base, GstBuffer* in, GstBuffer* out);
static GstFlowReturn svpPayloadTransformInPlace(GstBaseTransform* base, GstBuffer* buffer);
static GstFlowReturn svpPayloadPrepareOutputBuffer (GstBaseTransform * trans, GstBuffer * inbuf, GstBuffer ** outbuf);
static GstCaps* svpPayloadTransformCaps(GstBaseTransform*, GstPadDirection, GstCaps*, GstCaps*);
static GstStateChangeReturn svpPayloadChangeState(GstElement* element, GstStateChange transition);

static void svp_payload_class_init(SvpPayloadClass* klass)
{
    GstElementClass* elementClass = GST_ELEMENT_CLASS(klass);
    elementClass->change_state = GST_DEBUG_FUNCPTR(svpPayloadChangeState);
    gst_element_class_add_pad_template(elementClass, gst_static_pad_template_get(&sinkTemplate));
    gst_element_class_add_pad_template(elementClass, gst_static_pad_template_get(&srcTemplate));

    gst_element_class_set_static_metadata(
        elementClass,
        "SVP payloader.",
        GST_ELEMENT_FACTORY_KLASS_PAYLOADER,
        "Transfers media into SVP memory.",
        "Comcast");

    GST_DEBUG_CATEGORY_INIT(svp_payload_debug_category,
        "svppay", 0, "SVP payloader");

    GstBaseTransformClass* baseTransformClass = GST_BASE_TRANSFORM_CLASS(klass);
    baseTransformClass->transform = GST_DEBUG_FUNCPTR(svpPayloadTransform);
    baseTransformClass->transform_ip = GST_DEBUG_FUNCPTR(svpPayloadTransformInPlace);
    baseTransformClass->prepare_output_buffer = GST_DEBUG_FUNCPTR(svpPayloadPrepareOutputBuffer);
    baseTransformClass->transform_caps = GST_DEBUG_FUNCPTR(svpPayloadTransformCaps);
    baseTransformClass->transform_ip_on_passthrough = FALSE;
}

static void svp_payload_init(SvpPayload* self)
{
    SvpPayloadPrivate* priv = (SvpPayloadPrivate*)svp_payload_get_instance_private(self);
    priv->svpCtx = NULL;
    self->priv = priv;

    GstBaseTransform* base = GST_BASE_TRANSFORM(self);
    gst_base_transform_set_in_place(base, FALSE);
    gst_base_transform_set_passthrough(base, FALSE);
    gst_base_transform_set_gap_aware(base, FALSE);
}

static GstFlowReturn svpPayloadTransform(GstBaseTransform* base, GstBuffer* inbuf, GstBuffer* outbuf)
{
    GST_DEBUG_OBJECT(base, "Transform inbuf=(%" GST_PTR_FORMAT "), outbuf=(%" GST_PTR_FORMAT ")", inbuf, outbuf);

    SvpPayload* self = SVP_PAYLOAD(base);
    SvpPayloadPrivate* priv = (SvpPayloadPrivate*)svp_payload_get_instance_private(self);

    GstMemory *mem = gst_buffer_peek_memory(inbuf, 0);
    if ( gst_is_secmem_memory(mem) ) {
       GST_DEBUG_OBJECT(base, "Already in secmem, passthrough");
       g_return_val_if_fail((inbuf == outbuf), GST_FLOW_ERROR);
       return GST_FLOW_OK;
    }

    if (!gst_buffer_svp_transform_from_cleardata(priv->svpCtx, outbuf, Video))
      return GST_FLOW_ERROR;

    return GST_FLOW_OK;
}

static GstFlowReturn svpPayloadTransformInPlace(GstBaseTransform* base, GstBuffer* buffer)
{
    GST_DEBUG_OBJECT(base, "Transform in place buf=(%" GST_PTR_FORMAT ")", buffer);

    SvpPayload* self = SVP_PAYLOAD(base);
    SvpPayloadPrivate* priv = (SvpPayloadPrivate*)svp_payload_get_instance_private(self);

    GstMemory *mem = gst_buffer_peek_memory(buffer, 0);
    if ( gst_is_secmem_memory(mem) ) {
       GST_DEBUG_OBJECT(base, "Already in secmem, passthrough");
       return GST_FLOW_OK;
    }

    if (!gst_buffer_svp_transform_from_cleardata(priv->svpCtx, buffer, Video))
        return GST_FLOW_ERROR;

    return GST_FLOW_OK;
}

static GstFlowReturn svpPayloadPrepareOutputBuffer (GstBaseTransform * base, GstBuffer * inbuf, GstBuffer ** outbuf)
{
    GstMemory *mem = gst_buffer_peek_memory(inbuf, 0);
    if ( gst_is_secmem_memory(mem) ) {
       *outbuf = inbuf;
       return GST_FLOW_OK;
    }

    if ( gst_base_transform_is_in_place(base) == TRUE ) {
       GST_DEBUG_OBJECT(base, "Prepare buffer, call parent func");
       return GST_BASE_TRANSFORM_CLASS(parent_class)->prepare_output_buffer(base, inbuf, outbuf);
    } else {
       GST_DEBUG_OBJECT(base, "Prepare buffer, copy");
       *outbuf = gst_buffer_copy (inbuf);
       return GST_FLOW_OK;
    }
}

static GstCaps* svpPayloadTransformCaps(GstBaseTransform* base, GstPadDirection direction, GstCaps* caps, GstCaps* filter)
{
    if (direction == GST_PAD_UNKNOWN)
        return NULL;

    SvpPayload* self = SVP_PAYLOAD(base);

    GST_DEBUG_OBJECT(self, "Transform in direction: %s, caps %" GST_PTR_FORMAT ", filter %" GST_PTR_FORMAT,
                     direction == GST_PAD_SINK ? "GST_PAD_SINK" : "GST_PAD_SRC", caps, filter);

    GstCaps *ret = NULL;

    if (direction == GST_PAD_SINK) {
      ret = gst_caps_ref (caps);
      if(!gst_svp_ext_transform_caps(&ret, FALSE)) {
        gst_caps_unref(caps);
        GST_ERROR_OBJECT(self, "Caps transform failed");
        return NULL;
      }
    } else {
      ret = gst_caps_copy(caps);
      unsigned int size = gst_caps_get_size(ret);
      for (unsigned i = 0; i < size; ++i) {
        GstCapsFeatures *feat =  gst_caps_get_features(ret, i);
        gst_caps_features_remove(feat, GST_CAPS_FEATURE_MEMORY_SECMEM_MEMORY);
        gst_caps_features_remove(feat, GST_CAPS_FEATURE_MEMORY_DMABUF);
      }
    }

    if (filter) {
      GstCaps* intersection;
      intersection = gst_caps_intersect_full (ret, filter, GST_CAPS_INTERSECT_FIRST);
      gst_caps_unref(ret);
      ret = intersection;
    }

    GST_DEBUG_OBJECT(self, "return caps %" GST_PTR_FORMAT, ret);

    return ret;
}

static GstStateChangeReturn svpPayloadChangeState(GstElement* element, GstStateChange transition)
{
    SvpPayload* self = SVP_PAYLOAD(element);
    SvpPayloadPrivate* priv = (SvpPayloadPrivate*)svp_payload_get_instance_private(self);

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        GST_DEBUG_OBJECT(self, "NULL->READY");
        if (priv->svpCtx == NULL)
          gst_svp_ext_get_context(&priv->svpCtx, Server, 0);
        break;
    default:
        break;
    }

    GstStateChangeReturn result = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);

    switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
        GST_DEBUG_OBJECT(self, "READY->NULL");
        gst_svp_ext_free_context(priv->svpCtx);
        priv->svpCtx = NULL;
        break;
    default:
        break;
    }

    return result;
}

static gboolean svp_payload_plugin_init (GstPlugin * plugin)
{
   return gst_element_register (plugin,
                                "svppay",
                                GST_RANK_MARGINAL,
                                svp_payload_get_type());
}

#define PACKAGE_ORIGIN "http://amlogic.com/"
#define PACKAGE "gst_aml_svp_ext"

GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    amlsvppayload,
    "Transfers media into SVP memory",
    svp_payload_plugin_init,
    "0.1",
    "LGPL",
    PACKAGE,
    PACKAGE_ORIGIN )
