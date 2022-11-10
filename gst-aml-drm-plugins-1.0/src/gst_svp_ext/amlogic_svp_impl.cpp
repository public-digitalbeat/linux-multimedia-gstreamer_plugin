#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include <string>
#include <vector>
#include <functional>

#include <gst/gst.h>
#include "gst_svp_meta.h"
#include "gst_svp_meta_private.h"
#include "gst_svp_logging.h"
#include "gst_svp_scopedlock.h"
#include "sec_security_datatype.h"

#define SECMEM_HANDLE_THRESHHOLD 10
#define SECMEM_MEMORY_THRESHHOLD (512 * 1024)
static unsigned int s_nSecmem_Handle_Threshold = SECMEM_HANDLE_THRESHHOLD;
static unsigned int s_nSecmem_Memory_Threshold = SECMEM_MEMORY_THRESHHOLD;

// #ifdef RDK_SVP
//   #ifndef DYNAMIC_SVP_DECRYPTION
//     #error AMLOGIC_REQUIRES_DYNAMIC_SVP_DECRYPTION_FOR_SVP
//   #endif
// #endif

// Platform includes
extern "C" {
#include "gstsecmemallocator.h"
}

#ifdef ESSOS_RM
#include "essos-resmgr.h"
EssRMgr*        g_rm = NULL;
int             g_resAssignedId = -1;
#endif

void __attribute__((constructor)) ModuleInit();
void __attribute__((destructor)) ModuleTerminate();

typedef struct gst_svp_context_s
{
    gboolean        bInit;
    gboolean        bInUse;
    context_type    type;
    GstAllocator*   allocator;
    guint           timerID;
    gchar*          caps_str;
    size_t          caps_hash;
} gst_svp_context;

static GstAllocator* x_allocator = NULL;

Sec_Result SecOpaqueBuffer_CopyByIndex(GstMemory *mem, Sec_OpaqueBufferHandle *src, SEC_CopyIndex *copy_array, SEC_SIZE num_indexes)
{
    SEC_SIZE i = 0;
    uint32_t dst_offset[num_indexes];
    uint32_t src_offset[num_indexes];
    uint32_t size[num_indexes];

    if (NULL == mem || NULL == src) {
        LOG(eWarning, "Null pointer arg encountered\n");
        return SEC_RESULT_FAILURE;
    }

    /* overflow check */
    for (i = 0; i < num_indexes; i++) {
        if (copy_array[i].offset_in_target + copy_array[i].bytes_to_copy > mem->size) {
            LOG(eWarning, "attempt to write beyond opaque buffer boundary in %zd\n", i);
            goto error;
        }
    }

    for (i = 0; i < num_indexes; i++) {
        dst_offset[i] = copy_array[i].offset_in_target;
        src_offset[i] = copy_array[i].offset_in_src;
        size[i] = copy_array[i].bytes_to_copy;
    }

    return gst_secmem_copybyhandle(mem, src->secmem_handle, num_indexes, dst_offset, src_offset, size) ? SEC_RESULT_SUCCESS : SEC_RESULT_FAILURE;
error:
    return SEC_RESULT_FAILURE;
}

Sec_Result SecOpaqueBuffer_Free(Sec_OpaqueBufferHandle *handle)
{
    LOG(eWarning, "secopaque buffer free %x\n", handle->secmem_handle);
    gst_secmem_free_handle(x_allocator, handle->secmem_handle);
    return SEC_RESULT_SUCCESS;
}

void DumpOpaqueBufferHandle(void* pBuffer, const char* szName)
{
    Sec_OpaqueBufferHandle *handle = (Sec_OpaqueBufferHandle *)pBuffer;

    LOG(eWarning, "Sec_OpaqueBufferHandle dump struct - (%s)  - %p\n", szName, pBuffer);
    LOG(eWarning, "\tsecure_buffer_handle %x\n", handle->secmem_handle);
    LOG(eWarning, "\tdataBufSize %d\n", handle->dataBufSize);
}

static gboolean ReleaseSecMemAllocator()
{
    gboolean retVal = false;
    gint refCounts = 0;

    SCOPED_LOCK();

    if(x_allocator) {
        refCounts = GST_OBJECT_REFCOUNT_VALUE(x_allocator);
        LOG(eTrace, "secmem allocator (%p) refcount: %d\n", x_allocator, refCounts);
    } else {
        LOG(eTrace, "secmem allocator already released!\n");
        retVal = true;
    }

    if (x_allocator && (refCounts == 1)) {
        LOG(eTrace, "unreffing secmem allocator\n");
        gst_object_unref(x_allocator);

        x_allocator = NULL;

        LOG(eTrace, "released secmem allocator !\n");
#ifdef ESSOS_RM
        if (g_rm) {
            if (g_resAssignedId >= 0) {
                LOG(eTrace, "Essos resMgr: releasing secmem allocator resource id: %d\n", g_resAssignedId);
                EssRMgrReleaseResource (g_rm, EssRMgrResType_svpAllocator, g_resAssignedId);
                g_resAssignedId = -1;
            }
        }
#endif
        retVal = true;
    }

    if(refCounts > 1) {
        LOG(eTrace, "can't release secmem allocator due to refCounts held by gst buffers !\n");
    }

    return retVal;
}
#ifdef ESSOS_RM

// Essos comm. timeout is 6 seconds, so we can't wait more than that
#define SLEEP_TIME 100*1000 // 100ms
#define RELEASESECMEM_RETRIES 59 // 59*100 = 5.9 seconds

static void resMgrNotify(EssRMgr *rm, int event, int type, int id, void* userData)
{
  int retries = RELEASESECMEM_RETRIES;

  switch (type) {
    case EssRMgrResType_svpAllocator:
    {
      switch (event) {
        case EssRMgrEvent_revoked:
        {
            LOG(eWarning, "Essos resMgr:  revoking svp allocator %d\n", id);

            while ( retries-- ){
                if(ReleaseSecMemAllocator()) {
                    LOG(eWarning, "Essos resMgr: secmem allocator released!\n");
                    break;
                }
                LOG(eWarning, "Essos resMgr:  could not release secmem allocator, sleeping 100ms and trying again. retries left: %d\n", retries);
                usleep(SLEEP_TIME);
            }

            LOG(eWarning, "Essos resMgr: done revoking svp allocator %d\n", id);
          break;
        }
        default:
            break;
      }
    }
    default:
        break;
  }
}

static bool essos_rm_request()
{
  bool result=false;
  EssRMgrRequest resReq;

  resReq.type = EssRMgrResType_svpAllocator;
  resReq.usage = EssRMgrSVPAUse_none;
  resReq.priority = 0;
  resReq.asyncEnable = false;
  resReq.notifyCB = resMgrNotify;
  resReq.notifyUserData = NULL; //pContext;
  resReq.assignedId = -1;

  LOG(eWarning, "Essos resMgr: requesting SVP allocator from Essos resMgr\n", resReq.assignedId, resReq.assignedCaps);
  result = EssRMgrRequestResource (g_rm, EssRMgrResType_svpAllocator, &resReq);
  if (result) {
    if (resReq.assignedId >= 0) {
      LOG(eWarning, "Essos resMgr: assigned id %d caps %X\n", resReq.assignedId, resReq.assignedCaps);
      g_resAssignedId = resReq.assignedId;
    }
  }
  return result;
}
#endif
static GstAllocator* GetSecMemAllocator()
{
    SCOPED_LOCK();

    if(x_allocator == NULL) {
#ifdef ESSOS_RM
        if (!g_rm) {
            g_rm = EssRMgrCreate();
            if (!g_rm) {
                LOG(eError, "Essos resMgr: failed to create Essos Resource Manager!\n");
                return NULL;
            }
            g_resAssignedId = -1;
            LOG(eWarning, "Essos resMgr created %p\n", g_rm);
        }
        if(!essos_rm_request()){
            LOG(eError, "failed to get secmem allocator from Essos resMgr!\n");
            return NULL;
        }
#endif
        x_allocator = gst_secmem_allocator_new_ex(SECMEM_DECODER_DEFAULT, 0);
        LOG(eWarning, "Secmem Allocator Init %p\n", x_allocator);
    }

    return x_allocator;
}

// This function is assigned to execute as a library init
//  using __attribute__((constructor))
void ModuleInit()
{
    SCOPED_LOCK();

    LOG(eTrace, "GSTSVPEXT initialize\n");
}

// This function is assigned to execute as library unload
// using __attribute__((destructor))
void ModuleTerminate()
{
    SCOPED_LOCK();

    LOG(eTrace, "GSTSVPEXT terminate\n");

    ReleaseSecMemAllocator();
}

const char* getClientTypeString(context_type type)
{
    return (type == Server ? "Server" : type == Client ? "Client" : "InProcess");
}

static gboolean CheckFlowControl( GstAllocator *allocator, bool bBlock)
{
    // Implement flow control.  Wait here until there is room (handles, memory) to proceed.
    gboolean bSecMemSpaceAvailable = false;
    gint secmemHandles = 0;
    gint secmemSize = 0;

    gboolean retVal = gst_secmem_check_free_buf_and_handles_size(allocator, &secmemSize, &secmemHandles);
    if(!retVal) {
        LOG(eError, "gst_secmem_check_free_buf_and_handles_size returned FAILURE\n");
    }
    while (bSecMemSpaceAvailable == false) {
        if( secmemHandles > s_nSecmem_Handle_Threshold &&
            secmemSize > s_nSecmem_Memory_Threshold) {
            // Space available
            bSecMemSpaceAvailable = true;
            LOG(eTrace, "secmem heap stats: handles = %d, memory = %d\n",
                            secmemHandles, secmemSize);
        } else {
            LOG(eTrace, "secmem heap below threshold: handles = %d, memory = %d\n",
                            secmemHandles, secmemSize);
            if(bBlock) {
                usleep(30 * 1000); // 30ms
            }
        }
        if(!bBlock) {
            // Don't block break out of loop
            break;
        }
    }
    return bSecMemSpaceAvailable;
}
#ifndef ESSOS_RM
#define TIMER_INTERVAL_SECONDS 5
static gboolean timer_callback (gpointer pData)
{
    gboolean bTimerContinue = true;
    gst_svp_context* pContext = (gst_svp_context*)pData;

    SCOPED_LOCK();

    LOG(eTrace, "Context (%p) bInUse = %d\n", pContext, pContext->bInUse);

    if(pContext->bInUse != true) {
        // Allocator idle for timer period
        if(ReleaseSecMemAllocator()) {
            pContext->allocator = NULL;
            bTimerContinue = false;
        }
    }

    // Set the context to idle to see if there is activity by the next callback
    pContext->bInUse = false;

    return bTimerContinue;
}
#endif
static gboolean allocate_secure_memory(void * pContext, GstBuffer** ppBuffer, size_t nSize)
{
    gst_svp_context* pSVPContext = (gst_svp_context*)pContext;

    *ppBuffer = NULL;
    GstAllocator *allocator  = GetSecMemAllocator();
    if(allocator == NULL) {
        LOG(eError, "failed to get secure memory allocator!\n");
        return false;
    }
#ifndef ESSOS_RM
    if(pSVPContext->timerID == 0) {
        pSVPContext->timerID = g_timeout_add_seconds(TIMER_INTERVAL_SECONDS, (GSourceFunc)timer_callback, (gpointer)pContext);
        LOG(eTrace, "attached new timer (%u) to the context %p\n", pSVPContext->timerID, pSVPContext);
    }
#endif
    if(!CheckFlowControl(allocator, false)) {
        // Below low watermarks for handles or memory
        LOG(eTrace, "Secmem buffers or handles below watermark, no memory available\n");
    }

    LOG(eTrace, "Creating a secmem buffer of %d size\n", nSize);
    GstBuffer* buffer_secmem = gst_buffer_new_allocate(allocator, nSize, NULL);
    // Mark the allocator as active
    pSVPContext->bInUse = true;

    if(buffer_secmem != NULL) {
        *ppBuffer = buffer_secmem;
        return true;
    }

    return false;
}
gboolean gst_buffer_svp_transform_from_cleardata_impl(void * pContext, GstBuffer* buffer, media_type mediaType)
{
    if(mediaType != Video) {
        // Only wrap the data in SVP if it's video
        return true;
    }

    GstBuffer* buffer_secmem = NULL;
    if(!allocate_secure_memory(pContext, &buffer_secmem, gst_buffer_get_size(buffer))){
        LOG(eError, "could not allocate secure memory\n");
        // Allocation failure
        return false;
    }

    GstMemory *mem = gst_buffer_peek_memory(buffer_secmem, 0);

    // Copy clear data into output buffer
    GstMapInfo bufferMap;
    if (gst_buffer_map(buffer, &bufferMap, GST_MAP_READ) == false) {
        LOG(eError, "Invalid GST buffer.\n");
        return false;
    }
    uint8_t *networkData = reinterpret_cast<uint8_t* >(bufferMap.data);
    uint32_t networkDataSize = static_cast<uint32_t >(bufferMap.size);
    LOG(eTrace, "networkData %p, networkDataSize %d\n", networkData, networkDataSize);
    gst_secmem_fill(mem, 0, networkData, networkDataSize);

    gst_buffer_unmap(buffer, &bufferMap);

    // Put the new secmem memory into the original gst buffer
    gst_buffer_replace_all_memory(buffer, gst_buffer_get_all_memory(buffer_secmem));

    // LOG(eTrace, "Buffer RefCount buffer %d, secmem %d, mem %d\n",
    //  GST_OBJECT_REFCOUNT_VALUE(buffer), GST_OBJECT_REFCOUNT_VALUE(buffer_secmem), GST_OBJECT_REFCOUNT_VALUE(mem));

    gst_buffer_unref (buffer_secmem);

    return true;
}

static gboolean is_in_place_decrypted_buffer(guint8* encryptedData)
{
    gboolean retVal = false;

    if(encryptedData[0] == TokenType::InPlace) {
        retVal = true;
    }

    return retVal;
}
static guint8* remove_encrypted_buffer_header(guint8* encryptedData)
{
    guint8* retVal = encryptedData + sizeof(uint8_t);
    // LOG(eTrace, "Fixing pointers In = %p, Out = %p\n", encryptedData, retVal);
    return retVal;
}
static gboolean transform_in_place_buffer(void* pContext, GstBuffer* buffer, GstBuffer* subSampleBuffer, const guint32 subSampleCount, guint8* encryptedData)
{
    gboolean retVal = false;

    // LOG(eTrace, "buffer %p, subSampleBuffer %p, subSampleCount %u, encryptedData %p\n", buffer, subSampleBuffer, subSampleCount, encryptedData);

    GstMapInfo dataMap;
    if (gst_buffer_map(buffer, &dataMap, (GstMapFlags) GST_MAP_READWRITE) == false) {
        printf("Invalid buffer.\n");
        return retVal;
    }

    uint8_t *mappedData = reinterpret_cast<uint8_t* >(dataMap.data);

    if (subSampleBuffer != nullptr) {
        GstMapInfo sampleMap;

        if (gst_buffer_map(subSampleBuffer, &sampleMap, GST_MAP_READ) == false) {
            LOG(eError, "Invalid subsample buffer.\n");
            return retVal;
        }
        guint8 *mappedSubSample = reinterpret_cast<uint8_t* >(sampleMap.data);
        guint32 mappedSubSampleSize = static_cast<uint32_t >(sampleMap.size);

        GstByteReader* reader = gst_byte_reader_new(mappedSubSample, mappedSubSampleSize);
        gst_byte_reader_set_pos(reader, 0);

        // Re-build sub-sample data.
        uint32_t index = 0;
        unsigned total = 0;
        uint16_t inClear = 0;
        uint32_t inEncrypted = 0;
        for (uint32_t position = 0; position < subSampleCount; position++) {
            gst_byte_reader_get_uint16_be(reader, &inClear);
            gst_byte_reader_get_uint32_be(reader, &inEncrypted);

            memcpy(mappedData + total + inClear, encryptedData + index, inEncrypted);
            index += inEncrypted;
            total += inClear + inEncrypted;
        }

        retVal = true;

        gst_byte_reader_free(reader);
        gst_buffer_unmap(subSampleBuffer, &sampleMap);
    }
    else {
        // No Subsamples
        memcpy(mappedData, encryptedData, subSampleCount);
        retVal = true;
    }

    gst_buffer_unmap(buffer, &dataMap);
    return retVal;
}

gboolean gst_buffer_append_svp_transform_impl(void* pContext, GstBuffer* buffer, GstBuffer* subSampleBuffer, const guint32 subSampleCount, guint8* encryptedData)
{
    gboolean                    retVal          = false;
    void*                       secBuffer       = NULL;
    Sec_Result                  secResult       = SEC_RESULT_SUCCESS;

    SCOPED_LOCK();

    // LOG(eTrace, "buffer %p, subSampleBuffer %p, subSampleCount %u, encryptedData %p\n", buffer, subSampleBuffer, subSampleCount, encryptedData);

    if(encryptedData == NULL) {
        // clear data buffer
        if (!gst_buffer_svp_transform_from_cleardata_impl(pContext, buffer, Video)){
            LOG(eError, "gst_buffer_svp_transform_from_cleardata_impl() failed!\n");
            return false;
        }
    }
    if(is_in_place_decrypted_buffer(encryptedData)) {
        // Remove the Header that signals InPlace/Handle
        encryptedData = remove_encrypted_buffer_header(encryptedData);
        return transform_in_place_buffer(pContext, buffer, subSampleBuffer, subSampleCount, encryptedData);
    } else {
        // Remove the Header that signals InPlace/Handle
        encryptedData = remove_encrypted_buffer_header(encryptedData);
    }
    // LOG(eTrace, "buffer %p, subSampleBuffer %p, subSampleCount %u, encryptedData %p\n", buffer, subSampleBuffer, subSampleCount, encryptedData);

    if (subSampleBuffer != nullptr) {
        GstMapInfo sampleMap;

        if (gst_buffer_map(subSampleBuffer, &sampleMap, GST_MAP_READ) == false) {
            LOG(eError, "Invalid subsample buffer.\n");
            return retVal;
        }
        guint8 *mappedSubSample = reinterpret_cast<uint8_t* >(sampleMap.data);
        guint32 mappedSubSampleSize = static_cast<uint32_t >(sampleMap.size);

        GstByteReader* reader = gst_byte_reader_new(mappedSubSample, mappedSubSampleSize);
        guint16 inClear = 0;
        guint32 inEncrypted = 0;
        guint32 totalClear = 0;
        guint32 totalEncrypted = 0;
        for (guint32 position = 0; position < subSampleCount; position++) {

            gst_byte_reader_get_uint16_be(reader, &inClear);
            gst_byte_reader_get_uint32_be(reader, &inEncrypted);
            totalEncrypted += inEncrypted;
            totalClear += inClear;
            // LOG(eWarning, "gst_buffer_append_svp_transform_impl - inClear %d, inEncrypted %d \n",
            //               inClear, inEncrypted);
        }
        gst_byte_reader_set_pos(reader, 0);
        LOG(eTrace, "gst_buffer_append_svp_transform_impl - subSampleCount %d totalEncrypted %d, totalClear %d\n",
                    subSampleCount, totalEncrypted, totalClear);

        GstBuffer* buffer_secmem = NULL;
        if(!allocate_secure_memory(pContext, &buffer_secmem, gst_buffer_get_size(buffer)))
        {
            LOG(eError, "allocate_secure_memory() failed!\n");
            gst_byte_reader_free(reader);
            gst_buffer_unmap(subSampleBuffer, &sampleMap);
            return retVal;
        }

        GstMemory *mem = gst_buffer_peek_memory(buffer_secmem, 0);
        secmem_handle_t handle = gst_secmem_memory_get_handle(mem);

        if(totalEncrypted > 0) {
            // Before appending meta data we need to combine both the clear and decrypted data
            // into a single buffer and pass that to decoder

            // Get opaque buffer with decrypted data.
            svp_buffer_alloc_token(&secBuffer);
            Sec_OpaqueBufferHandle* pBufferFrom = NULL;
            if(secBuffer) {
                svp_buffer_from_token(pContext, encryptedData, secBuffer);
                pBufferFrom = (Sec_OpaqueBufferHandle*)secBuffer;
            }
            else {
                LOG(eError, "pBufferFrom allocation Failed!\n");
                return retVal;
            }

            // Copy clear data into output buffer
            GstMapInfo bufferMap;
            if (gst_buffer_map(buffer, &bufferMap, GST_MAP_READ) == false) {
                LOG(eError, "Invalid GST buffer.\n");
                return retVal;
            }
            uint8_t *networkData = reinterpret_cast<uint8_t* >(bufferMap.data);
            uint32_t networkDataSize = static_cast<uint32_t >(bufferMap.size);
            // LOG(eTrace, "SVP_POC gst_buffer_append_svp_transform_impl - networkData %p, networkDataSize %d\n", networkData, networkDataSize);
            mem->size = networkDataSize;
            gst_secmem_fill(mem, 0, networkData, networkDataSize);

            gst_buffer_unmap(buffer, &bufferMap);
            // Copy decrypted data into the opaque output buffer
            uint32_t byteIndex = 0;
            uint32_t encByteIndex = 0;
            std::vector<SEC_CopyIndex> copyArray(subSampleCount, {0, 0, 0});

            for (guint32 position = 0; position < subSampleCount; position++) {
                gst_byte_reader_get_uint16_be(reader, &inClear);
                gst_byte_reader_get_uint32_be(reader, &inEncrypted);
                byteIndex += inClear;

                copyArray[position].offset_in_src = encByteIndex;
                copyArray[position].offset_in_target = byteIndex;
                copyArray[position].bytes_to_copy = inEncrypted;

                encByteIndex  += inEncrypted;
                byteIndex += inEncrypted;
            }

            secResult = SecOpaqueBuffer_CopyByIndex(mem, pBufferFrom, copyArray.data(), subSampleCount);
            if (secResult != SEC_RESULT_SUCCESS) {
                LOG(eError, "ERROR: SecOpaqueBuffer_CopyByIndex failed result = %d\n", secResult);
                LOG(eError, "Transfering data to handle = %x index = %d, from index = %d, length = %d\n", handle, byteIndex, encByteIndex, inEncrypted);
                return false;
            }
            gst_byte_reader_set_pos(reader, 0);

            // Put the new secmem memory into the original gst buffer
            gst_buffer_replace_all_memory(buffer, gst_buffer_get_all_memory(buffer_secmem));
            // Add metadata indicating this is an SVP buffer
            GstStructure *drm_info = gst_structure_new("drm_info","handle", G_TYPE_INT, handle, NULL);
            gst_buffer_add_protection_meta(buffer, drm_info);

            retVal = true;

            // LOG(eTrace, "Buffer RefCount buffer %d, secmem %d, mem %d\n",
            //             GST_OBJECT_REFCOUNT_VALUE(buffer), GST_OBJECT_REFCOUNT_VALUE(buffer_secmem), GST_OBJECT_REFCOUNT_VALUE(mem));

            svp_buffer_free_token(pBufferFrom);
            gst_buffer_unref(buffer_secmem);
        }
        gst_byte_reader_free(reader);
        gst_buffer_unmap(subSampleBuffer, &sampleMap);
    } else {
        // Get opaque buffer with decrypted data.
        svp_buffer_alloc_token(&secBuffer);
        Sec_OpaqueBufferHandle* pBufferFrom = NULL;
        Sec_OpaqueBufferHandle gst_opaque_buf;
        if(secBuffer) {
            svp_buffer_from_token(pContext, encryptedData, secBuffer);
            pBufferFrom = (Sec_OpaqueBufferHandle*)secBuffer;
            // DumpOpaqueBufferHandle(pBufferFrom, "pBufferFrom");

            GstBuffer* buffer_secmem = NULL;
            gsize bufSize = gst_buffer_get_size(buffer);
            if (!allocate_secure_memory(pContext, &buffer_secmem, bufSize)) {
                LOG(eError, "allocate_secure_memory() failed!\n");
                svp_buffer_free_token(secBuffer);
                return false;
            }

            GstMemory *mem = gst_buffer_peek_memory(buffer_secmem, 0);
            secmem_handle_t handle = gst_secmem_memory_get_handle(mem);

            // Copy data into new buffer
            gst_opaque_buf.secmem_handle = handle;
            gst_opaque_buf.dataBufSize = (SEC_SIZE)bufSize;
            mem->size = (SEC_SIZE)bufSize;
            // LOG(eTrace, "Copying %d bytes to secure buffer of size %d\n", gst_opaque_buf.dataBufSize, bufSize);
            SEC_CopyIndex copy_array;

            copy_array.offset_in_src = 0;
            copy_array.offset_in_target = 0;
            copy_array.bytes_to_copy = gst_opaque_buf.dataBufSize;
            secResult = SecOpaqueBuffer_CopyByIndex(mem, pBufferFrom, &copy_array, 1);

            // Put the new secmem memory into the original gst buffer
            gst_buffer_replace_all_memory(buffer, gst_buffer_get_all_memory(buffer_secmem));

            // Add metadata indicating this is an SVP buffer
            GstStructure *drm_info = gst_structure_new("drm_info","handle", G_TYPE_INT, handle, NULL);
            gst_buffer_add_protection_meta(buffer, drm_info);

            retVal = true;

            /* This function will release the pointer and destroy the underlying secmem handle */
            bool ret = svp_buffer_free_token(pBufferFrom);
            if (!ret) {
                LOG(eWarning, "gst_buffer_append_svp_transform_impl - SecOpaqueBuffer_Free returned %d\n", ret);
            }

            // LOG(eWarning, "Buffer RefCount buffer %d, secmem %d, mem %d\n",
            // GST_OBJECT_REFCOUNT_VALUE(buffer), GST_OBJECT_REFCOUNT_VALUE(buffer_secmem), GST_OBJECT_REFCOUNT_VALUE(mem));

            gst_buffer_unref (buffer_secmem);
        } else {
            LOG(eError, "secBuffer allocation Failed!\n");
            return retVal;
        }
    }

    LOG(eTrace, "Done\n");
    return retVal;
}

gboolean gst_buffer_append_svp_metadata_impl(GstBuffer* buffer, svp_meta_data_t* svp_metadata)
{
    bool result = false;
    LOG(eError, "Not Implemented!\n");
    return result;
}

gboolean svp_buffer_to_token_impl(void * pContext, void* svp_handle, void* svp_token)
{
    bool result = false;

    //DumpOpaqueBufferHandle(svp_handle, __FUNCTION__);

    // if ((svp_handle != NULL) && (svp_token != NULL)) {
    //     Sec_Result res;
    //     Sec_ProtectedMemHandle *tmp = NULL;
    //     /* release the svp handle from secapi */
    //     res = ReleaseSecMemAllocator();
    //     if (!res && tmp) {
    //         /* copy the secmem handle to upper layer */
    //         memcpy(svp_token, tmp, sizeof(Sec_OpaqueBufferHandle));
    //         LOG(eTrace, "tmp handle: %x", ((Sec_OpaqueBufferHandle *)svp_token)->secmem_handle);
    //         free(tmp);
    //         result = true;
    //     } else {
    //         LOG(eError, "failed to release handle");
    //         result = false;
    //     }
    // }

    return result;
}

gboolean svp_buffer_from_token_impl(void * pContext, void* svp_token, void* svp_handle)
{
    bool result = false;

    if((svp_handle != NULL) && (svp_token != NULL)) {
        memcpy(svp_handle, svp_token, sizeof(Sec_OpaqueBufferHandle));
        LOG(eTrace, "tmp handle: %x\n", ((Sec_OpaqueBufferHandle *)svp_handle)->secmem_handle);
        //DumpOpaqueBufferHandle(svp_handle, __FUNCTION__);

        result = true;
    }

    return result;
}

guint32 svp_token_size_impl(void)
{
    guint32 nTokenSize = sizeof(Sec_OpaqueBufferHandle);
    nTokenSize += sizeof(uint8_t);
    return nTokenSize;
}

gboolean svp_buffer_alloc_token_impl(void **token)
{
    gboolean retVal = false;
    if(token != NULL) {
        *token = (void *)calloc(svp_token_size_impl(), 1);
        if(*token != NULL) {
            retVal = true;
        }
    }

    return retVal;
}

gboolean svp_buffer_free_token_impl(void *token)
{
    gboolean retVal = false;
    if(token != NULL) {
        free(token);
        retVal = true;
    }

    return retVal;
}

gboolean svp_pipeline_buffers_available_impl(void * pContext, media_type mediaType)
{
    gboolean retVal = true;

    if(mediaType != Video) {
        // Audio streams do not use SVP
        LOG(eTrace, "Non Video frame\n");
        return true;
    }

    // if(buffer == NULL) {
    //     // No buffer to verify secmem levels
    //     LOG(eWarning, "Null buffer\n");
    //     return true;
    // }

    GstAllocator *allocator  = GetSecMemAllocator();
    if(allocator == NULL) {
        LOG(eError, "failed to get secure memory allocator!\n");
        retVal = true;
    } else if(!CheckFlowControl(allocator, false)) {
        // Below low watermarks for handles or memory
        LOG(eTrace, "Secmem buffers or handles below watermark, no memory available\n");
        retVal = false;
    }

    return retVal;
}

gboolean gst_svp_ext_get_context_impl(void ** ppContext, context_type type, unsigned int rpcID)
{
    gst_svp_context* context = (gst_svp_context*)*ppContext;

    SCOPED_LOCK();

    if(context != nullptr && context->bInit == true) {
        LOG(eTrace, "Found existing context for %s (%d)\n", getClientTypeString(type), context->type);
        return true;
    }

    LOG(eWarning, "Got Init for %s\n", getClientTypeString(type));
    if(context == nullptr) {
        LOG(eWarning, "Allocating new Context, ppContext = %p, *ppContext = %p, context = %p\n", ppContext, *ppContext, context);
        context = (gst_svp_context*)calloc(1, sizeof(gst_svp_context));
        if(context == nullptr) {
            LOG(eError, "Failed to allocate a new context\n");
            return false;
        }
        LOG(eWarning, "Allocated new Context, ppContext = %p, *ppContext = %p, context = %p\n", ppContext, *ppContext, context);
    }

    context->type = type;
    context->bInit = true;
    context->bInUse = false;
    context->allocator = NULL;
    context->timerID =0;

    LOG(eWarning, "Context (%p) Init for %s complete\n", context, getClientTypeString(type));

    // Check env variable overrides
    const char *env_memory_threshhold = getenv("GSTSVPEXT_MEMORY_THRESHHOLD");
    if(env_memory_threshhold != NULL) {
        s_nSecmem_Memory_Threshold = atoi(env_memory_threshhold);
        LOG(eWarning, "Overriding s_nSecmem_Memory_Threshold default value to %u\n", s_nSecmem_Memory_Threshold);
    }
    const char *env_handle_threshhold = getenv("GSTSVPEXT_HANDLE_THRESHHOLD");
    if(env_handle_threshhold != NULL) {
        s_nSecmem_Handle_Threshold = atoi(env_handle_threshhold);
        LOG(eWarning, "Overriding s_nSecmem_Handle_Threshold default value to %u\n", s_nSecmem_Handle_Threshold);
    }

    *ppContext = (void*)context;
    return true;
}

gboolean gst_svp_ext_context_set_caps_impl(void * pContext, GstCaps *caps)
{
    gst_svp_context* pSVPContext = (gst_svp_context*)pContext;
    gchar* caps_str = NULL;
    static std::hash<std::string> str_hash;

    SCOPED_LOCK();

    if(!pSVPContext){
        LOG(eError, "invalid SVP context!\n");
        return false;
    }

    if(!caps ) {
        // caps was NULL
        LOG(eError, "Invalid caps value\n");
        return false;
    }

    caps_str = gst_caps_to_string(caps);
    if(!caps_str) {
        LOG(eError, "caps string is empty\n");
        return false;
    }

    if(pSVPContext->caps_hash == 0) {
        // No Caps value set
        pSVPContext->caps_str = caps_str;
        pSVPContext->caps_hash = str_hash(std::string(caps_str));
        LOG(eTrace, "Got Caps (%s) with hash %lu\n", pSVPContext->caps_str, pSVPContext->caps_hash);
        return true;
    }
    // We have an existing hash

    if(pSVPContext->caps_hash != str_hash(std::string(caps_str))) {
        // Hash values differ
        LOG(eTrace, "Caps differ from already set values orig (%s), new (%s)\n", pSVPContext->caps_str, caps_str);
        pSVPContext->caps_str = caps_str;
        pSVPContext->caps_hash = str_hash(std::string(caps_str));
    }
    else {
        g_free(caps_str);
    }

    return true;
}

gboolean gst_svp_ext_free_context_impl(void * pContext)
{
    gst_svp_context* pSVPContext = (gst_svp_context*)pContext;

    SCOPED_LOCK();

    LOG(eWarning, "About to free context %p\n", pSVPContext);
    if(pSVPContext != nullptr) {
        LOG(eWarning, "Freeing context %p for %s\n", pSVPContext, getClientTypeString(pSVPContext->type));

        ReleaseSecMemAllocator();

        pSVPContext->allocator = NULL;

        if(pSVPContext->timerID != 0) {
            g_source_remove(pSVPContext->timerID);
            pSVPContext->timerID = 0;
        }

        if(pSVPContext->caps_str != nullptr) {
            g_free(pSVPContext->caps_str);
            pSVPContext->caps_str = nullptr;
        }

        // reset the context to initial state
        memset(pSVPContext, 0, sizeof(gst_svp_context));
        free(pSVPContext);
    }

    return true;
}

#define GST_CAPS_FEATURE_MEMORY_SECMEM_MEMORY "memory:SecMem"
#define GST_CAPS_FEATURE_MEMORY_DMABUF "memory:DMABuf"

gboolean gst_svp_ext_transform_caps_impl(GstCaps **caps, gboolean bEncrypted)
{
    gboolean find = false;
    GstCaps * ret = gst_caps_copy(*caps);
    unsigned int size = gst_caps_get_size(ret);
    for (unsigned i = 0; i < size; ++i) {
        GstStructure * structure = gst_caps_get_structure(*caps, i);
        LOG(eTrace, "caps structure = %s\n", gst_structure_get_name (structure));
        if (g_str_has_prefix (gst_structure_get_name (structure), "video/")) {
            find = true;
            /*
                Codec	Stream Format	Feature
                H264	avc 			memory:SecMem
                        byte-stream 	memory:DMABuf
                H265	hvc1		    memory:SecMem
                        byte-stream	    memory:DMABuf
                VP9						memory:SecMem
                AV1						memory:SecMem
            */
            if ( gst_structure_has_field(structure, "stream-format") &&
                 !g_strcmp0(gst_structure_get_string(structure, "stream-format"), "byte-stream") &&
                 !g_strcmp0(gst_structure_get_string(structure, "alignment"), "au")) {
                gst_caps_set_features(ret, i, gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_DMABUF));
            } else {
                gst_caps_set_features(ret, i, gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_SECMEM_MEMORY));
            }
        }
    }
    if(find) {
      gst_caps_replace(caps, ret);
      gchar* capsStr = gst_caps_to_string (*caps);
      static std::string capsStrSaved; // Used to prevent redundant consecutive duplicates of a caps string from being logged.
      if(0 != g_strcmp0 (capsStr, capsStrSaved.c_str())) { // Only log caps strings that differ from the preceding.
        LOG(eWarning, "NEW video caps_string = %s\n", capsStr);
      }
      capsStrSaved = capsStr;
      g_free(capsStr);
    }
    gst_caps_unref(ret);

    return true;
}

gboolean gst_buffer_append_init_metadata_impl(GstBuffer * buffer)
{
    //no op
    return false;
}

