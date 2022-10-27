/*
 * gstamlhdcp.h
 *
 *  Created on: Feb 5, 2020
 *      Author: tao
 */

#ifndef __GST_AMLHDCP_H_
#define __GST_AMLHDCP_H_

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include "hdcp_module.h"

G_BEGIN_DECLS


#define GST_TYPE_AMLHDCP \
  (gst_aml_hdcp_get_type())
#define GST_AMLHDCP(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AMLHDCP,GstAmlHDCP))
#define GST_AMLHDCP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AMLHDCP,GstAmlHDCPClass))
  #define GST_AMLHDCP_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_AMLHDCP,GstAmlHDCPClass))
#define GST_IS_AMLHDCP(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AMLHDCP))
#define GST_IS_AMLHDCP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AMLHDCP))


typedef struct _GstAmlHDCP      GstAmlHDCP;
typedef struct _GstAmlHDCPClass GstAmlHDCPClass;

enum
{
    PROP_0,
    PROP_DRM_STATIC_PIPELINE,
    PROP_DRM_HDCP_CONTEXT
};

struct _GstAmlHDCP
{
    GstBaseTransform        element;

    GstCaps                *outcaps;
    GstAllocator           *allocator;
    amlWfdHdcpHandle       *hdcp_context;
    amlWfdHdcpHandle        hdcp_handle;
    int                     hdcp_init_done;
    gboolean                static_pipeline;
};

struct _GstAmlHDCPClass {
    GstBaseTransformClass   parent_class;
};


GType gst_aml_hdcp_get_type (void);

G_END_DECLS


#endif /* __GST_AMLHDCP_H_ */
