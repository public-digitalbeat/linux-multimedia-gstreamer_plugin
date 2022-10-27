/*
 * gst_ge2d.c
 *
 *  Created on: 2020年11月3日
 *      Author: tao
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gst_ge2d_flip.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
    gboolean ret = FALSE;
    ret |= gst_element_register(plugin, "ge2d_flip",
            GST_RANK_PRIMARY,
            GST_TYPE_GE2D_FLIP);
    return ret;
}

#define PACKAGE "gst-aml-drm-plugins"
#define GST_PACKAGE_ORIGIN "http://amlogic.com"

GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    ge2d,
    "Amlogic plugin for ge2d",
    plugin_init,
    VERSION,
    "LGPL",
    PACKAGE_NAME,
    GST_PACKAGE_ORIGIN
)
