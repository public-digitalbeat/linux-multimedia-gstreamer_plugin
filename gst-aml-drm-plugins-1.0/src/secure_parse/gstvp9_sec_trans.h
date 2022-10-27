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

#ifndef _GST_VP9_SEC_TRANS_H_
#define _GST_VP9_SEC_TRANS_H_

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_VP9_SEC_TRANS \
  (gst_vp9_sec_trans_get_type())
#define GST_VP9_SEC_TRANS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VP9_SEC_TRANS,GstVp9SecTrans))
#define GST_VP9_SEC_TRANS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VP9_SEC_TRANS,GstVp9SecTransClass))
  #define GST_VP9_SEC_TRANS_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_VP9_SEC_TRANS,GstVp9SecTransClass))
#define GST_IS_VP9_SEC_TRANS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VP9_SEC_TRANS))
#define GST_IS_VP9_SEC_TRANS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VP9_SEC_TRANS))


typedef struct _GstVp9SecTrans      GstVp9SecTrans;
typedef struct _GstVp9SecTransClass GstVp9SecTransClass;


struct _GstVp9SecTrans
{
    GstBaseTransform        element;
};

struct _GstVp9SecTransClass {
    GstBaseTransformClass   parent_class;
};


GType gst_vp9_sec_trans_get_type (void);

G_END_DECLS
#endif /* _GST_VP9_SEC_TRANS_H_ */
