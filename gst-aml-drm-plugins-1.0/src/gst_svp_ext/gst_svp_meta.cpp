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
#include "gst_svp_meta.h"
#include "gst_svp_meta_private.h"

gboolean gst_svp_ext_get_context(void ** ppContext, context_type type, unsigned int rpcID)
{
    GstPerf perf(__FUNCTION__);
    return gst_svp_ext_get_context_impl(ppContext, type, rpcID);
}

gboolean gst_svp_ext_context_set_caps(void * pContext, GstCaps *caps)
{
    GstPerf perf(__FUNCTION__);
    return gst_svp_ext_context_set_caps_impl(pContext, caps);
}

gboolean gst_svp_ext_free_context(void * pContext)
{
    GstPerf perf(__FUNCTION__);
    return gst_svp_ext_free_context_impl(pContext);
}

gboolean gst_buffer_svp_transform_from_cleardata(void * pContext, GstBuffer* buffer, media_type mediaType)
{
    GstPerf perf(__FUNCTION__);
    return gst_buffer_svp_transform_from_cleardata_impl(pContext, buffer, mediaType);
}

gboolean gst_svp_ext_transform_caps(GstCaps **caps, gboolean bEncrypted)
{
    GstPerf perf(__FUNCTION__);
    return gst_svp_ext_transform_caps_impl(caps, bEncrypted);
}

gboolean gst_buffer_append_svp_transform(void * pContext, GstBuffer* buffer, GstBuffer* subSampleBuffer, const guint32 subSampleCount, guint8* encryptedData)
{
    GstPerf perf(__FUNCTION__);
    return gst_buffer_append_svp_transform_impl(pContext, buffer, subSampleBuffer, subSampleCount, encryptedData);
}

gboolean gst_buffer_append_svp_metadata(GstBuffer* buffer,  svp_meta_data_t* svp_metadata)
{
    GstPerf perf(__FUNCTION__);
    return gst_buffer_append_svp_metadata_impl(buffer,svp_metadata);
}

gboolean svp_buffer_to_token(void * pContext, void* svp_handle, void* svp_token)
{
    GstPerf perf(__FUNCTION__);
    return svp_buffer_to_token_impl(pContext, svp_handle, svp_token);
}

gboolean svp_buffer_from_token(void * pContext, void* svp_token, void* svp_handle)
{
    GstPerf perf(__FUNCTION__);
    return svp_buffer_from_token_impl(pContext, svp_token, svp_handle);
}

guint32 svp_token_size(void)
{
    GstPerf perf(__FUNCTION__);
    return svp_token_size_impl();
}

gboolean svp_buffer_alloc_token(void **token)
{
    GstPerf perf(__FUNCTION__);
    return svp_buffer_alloc_token_impl(token);
}

gboolean svp_buffer_free_token(void *token)
{
    GstPerf perf(__FUNCTION__);
    return  svp_buffer_free_token_impl(token);
}

gboolean svp_pipeline_buffers_available(void * pContext, media_type mediaType)
{
    GstPerf perf(__FUNCTION__);
    return  svp_pipeline_buffers_available_impl(pContext, mediaType);
}

gboolean gst_buffer_append_init_metadata(GstBuffer * buffer)
{
    GstPerf perf(__FUNCTION__);
    return  gst_buffer_append_init_metadata_impl(buffer);
}

