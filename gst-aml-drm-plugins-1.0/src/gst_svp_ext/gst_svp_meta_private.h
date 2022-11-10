/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2019 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
#ifndef __GST_BUFFER_SVP_PRIVATE_H__
#define __GST_BUFFER_SVP_PRIVATE_H__

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <gst/gst.h>
#include <gst/base/gstbytereader.h>

G_BEGIN_DECLS

/* Vendor specific implementations */
gboolean gst_svp_ext_get_context_impl(void ** ppContext, context_type type, unsigned int rpcID);
gboolean gst_svp_ext_context_set_caps_impl(void * pContext, GstCaps *caps);
gboolean gst_svp_ext_free_context_impl(void * pContext);
gboolean gst_svp_ext_transform_caps_impl(GstCaps **caps, gboolean bEncrypted);
gboolean gst_buffer_svp_transform_from_cleardata_impl(void * pContext, GstBuffer* buffer, media_type mediaType);
gboolean gst_buffer_append_svp_transform_impl(void * pContext, GstBuffer* buffer, GstBuffer* subSampleBuffer, const guint32 subSampleCount, guint8* encryptedData);
gboolean gst_buffer_append_svp_metadata_impl(GstBuffer * buffer,  svp_meta_data_t * svp_metadata);
gboolean svp_buffer_to_token_impl(void * pContext, void* svp_handle, void* svp_token);
gboolean svp_buffer_from_token_impl(void * pContext, void* svp_token, void* svp_handle);
guint32  svp_token_size_impl(void);
gboolean svp_buffer_alloc_token_impl(void **token);
gboolean svp_buffer_free_token_impl(void *token);
gboolean svp_pipeline_buffers_available_impl(void * pContext, media_type mediaType);
gboolean gst_buffer_append_init_metadata_impl(GstBuffer * buffer);

G_END_DECLS
#endif /* __GST_BUFFER_SVP_PRIVATE_H__ */
