/*
 * gst_ge2d_flip.h
 *
 *  Created on: 2020年11月3日
 *      Author: tao
 */

#ifndef _GST_GE2D_FLIP_H_
#define _GST_GE2D_FLIP_H_

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>
#include <aml_ge2d.h>


G_BEGIN_DECLS

#define GST_TYPE_GE2D_FLIP \
  (gst_ge2d_flip_get_type())
#define GST_GE2D_FLIP(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GE2D_FLIP,GstGe2dFlip))
#define GST_GE2D_FLIP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GE2D_FLIP,GstGe2dFlipClass))
  #define GST_GE2D_FLIP_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_GE2D_FLIP,GstGe2dFlipClass))
#define GST_IS_GE2D_FLIP(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GE2D_FLIP))
#define GST_IS_GE2D_FLIP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GE2D_FLIP))


typedef struct _GstGe2dFlip      GstGe2dFlip;
typedef struct _GstGe2dFlipClass GstGe2dFlipClass;


struct _GstGe2dFlip
{
    GstBaseTransform            videofilter;

    GstVideoOrientationMethod   method;
    gboolean                    secure;
    gint                        width_set;
    gint                        height_set;
    gint                        width_src;
    gint                        height_src;
    GstVideoInfo                in_info;
    GstVideoInfo                out_info;

    int                         ret;
    aml_ge2d_info_t             *pge2dinfo;
    aml_ge2d_t                  amlge2d;
};

struct _GstGe2dFlipClass {
    GstBaseTransformClass       parent_class;
};


GType gst_ge2d_flip_get_type (void);

G_END_DECLS

#endif /* _GST_GE2D_FLIP_H_ */
