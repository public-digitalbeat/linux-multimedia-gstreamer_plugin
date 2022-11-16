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
#ifndef __GST_BUFFER_SVP_H__
#define __GST_BUFFER_SVP_H__

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <gst/gst.h>
#include <gst/base/gstbytereader.h>

#include "gst_svp_performance.h"

// #include "sec_security.h"       // Sec_OpaqueBufferHandle
// #include "sec_security_datatype.h" 

G_BEGIN_DECLS

typedef struct enc_data_chunk enc_chunk_data_t;
struct enc_data_chunk {
    guint32 clear_data_size;
    guint32 enc_data_size;
};

typedef struct svp_meta_data {
    guint32 secure_memory_ptr;
    guint32 num_chunks;
    enc_chunk_data_t *info;
} svp_meta_data_t;

typedef enum context_type_e {
    Server = 0,
    Client,
    InProcess
} context_type;

typedef enum TokenType_e {
    InPlace = 0,
    Handle  = 1
} TokenType;

typedef enum media_type_e
{
    Unknown = 0,
    Video,
    Audio,
    Data
}  media_type;

// typedef struct gst_svp_token_s
// {
//     TokenType eType;
//     union data {
//         Sec_OpaqueBufferHandle  handle;
//         uint8_t                 buffer[1];
//     };
// } gst_svp_token;


/*
 *  GstBuffer          *buffer          - gstreamer buffer that svp meta data will be appended to
 *  svp_meta_data_t    *svp_metadata    - svp meta data
 */
gboolean gst_svp_ext_get_context(void ** ppContext, context_type type, unsigned int rpcID);
gboolean gst_svp_ext_context_set_caps(void * pContext, GstCaps *caps);
gboolean gst_svp_ext_free_context(void * pContext);
gboolean gst_svp_ext_transform_caps(GstCaps **caps, gboolean bEncrypted);
gboolean gst_buffer_svp_transform_from_cleardata(void * pContext, GstBuffer* buffer, media_type mediaType);
gboolean gst_buffer_append_svp_transform(void * pContext, GstBuffer* buffer, GstBuffer* subSampleBuffer, const guint32 subSampleCount, guint8* encryptedData);
gboolean gst_buffer_append_svp_metadata(GstBuffer * buffer,  svp_meta_data_t * svp_metadata);
gboolean svp_buffer_to_token(void * pContext, void* svp_handle, void* svp_token);
gboolean svp_buffer_from_token(void * pContext, void* svp_token, void* svp_handle);
guint32  svp_token_size(void);
gboolean svp_buffer_alloc_token(void **token);
gboolean svp_buffer_free_token(void *token);
gboolean svp_pipeline_buffers_available(void * pContext, media_type mediaType);
gboolean gst_buffer_append_init_metadata(GstBuffer * buffer);

G_END_DECLS
#endif /* __GST_BUFFER_SVP_H__ */
