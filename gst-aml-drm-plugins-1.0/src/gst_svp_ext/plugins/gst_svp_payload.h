/*
 * Copyright (C) 2019 RDK Management
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef GST_SVP_PAYLOAD_H
#define GST_SVP_PAYLOAD_H

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define SVP_PAYLOAD_TYPE          (svp_payload_get_type())
#define SVP_PAYLOAD(obj)          (G_TYPE_CHECK_INSTANCE_CAST((obj), SVP_PAYLOAD_TYPE, SvpPayload))
#define SVP_PAYLOAD_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST((klass), SVP_PAYLOAD_TYPE, SvpPayloadClass))

typedef struct _SvpPayload           SvpPayload;
typedef struct _SvpPayloadClass      SvpPayloadClass;
typedef struct _SvpPayloadPrivate    SvpPayloadPrivate;

GType svp_payload_get_type(void);

struct _SvpPayload {
    GstBaseTransform parent;
    SvpPayloadPrivate *priv;
};

struct _SvpPayloadClass {
    GstBaseTransformClass parentClass;
};

G_END_DECLS


#endif /* GST_SVP_PAYLOAD_H */
