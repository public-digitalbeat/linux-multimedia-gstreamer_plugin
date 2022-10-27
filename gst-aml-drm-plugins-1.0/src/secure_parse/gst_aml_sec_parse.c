#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gsth264_sec_parse.h"
#include "gsth265_sec_parse.h"
#include "gstvp9_sec_trans.h"
#include "gstav1_sec_trans.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
    gboolean ret = FALSE;
    ret |= gst_element_register(plugin, "h264secparse",
            GST_RANK_PRIMARY,
            GST_TYPE_H264_SEC_PARSE);
    ret |= gst_element_register(plugin, "h265secparse",
            GST_RANK_PRIMARY,
            GST_TYPE_H265_SEC_PARSE);
    ret |= gst_element_register(plugin, "vp9_sec_trans",
            GST_RANK_PRIMARY,
            GST_TYPE_VP9_SEC_TRANS);
    ret |= gst_element_register(plugin, "av1_sec_trans",
            GST_RANK_PRIMARY,
            GST_TYPE_AV1_SEC_TRANS);
    return ret;
}

#define PACKAGE "gst-aml-drm-plugins"
#define GST_PACKAGE_ORIGIN "http://amlogic.com"

GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    amlsecparse,
    "Amlogic plugin for secure streams",
    plugin_init,
    VERSION,
    "LGPL",
    PACKAGE_NAME,
    GST_PACKAGE_ORIGIN
)
