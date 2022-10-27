/*
 * gstdummydrm.h
 *
 *  Created on: 2020年4月1日
 *      Author: tao
 */

#ifndef _DUMMY_GSTDUMMYDRM_H_
#define _DUMMY_GSTDUMMYDRM_H_

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS


#define GST_TYPE_DUMMYDRM \
  (gst_dummydrm_get_type())
#define GST_DUMMYDRM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DUMMYDRM,GstDummyDrm))
#define GST_DUMMYDRM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DUMMYDRM,GstDummyDrmClass))
  #define GST_DUMMYDRM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_DUMMYDRM,GstDummyDrmClass))
#define GST_IS_DUMMYDRM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DUMMYDRM))
#define GST_IS_DUMMYDRM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DUMMYDRM))


typedef struct _GstDummyDrm      GstDummyDrm;
typedef struct _GstDummyDrmClass GstDummyDrmClass;


struct _GstDummyDrm
{
    GstBaseTransform        element;

    GstCaps                *outcaps;
    GstAllocator           *allocator;


};

struct _GstDummyDrmClass {
    GstBaseTransformClass   parent_class;
};


GType gst_dummydrm_get_type (void);

G_END_DECLS



#endif /* _DUMMY_GSTDUMMYDRM_H_ */
