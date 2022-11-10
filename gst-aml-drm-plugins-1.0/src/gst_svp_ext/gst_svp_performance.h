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


#ifndef __GST_SVP_PERFORMANCE_H__
#define __GST_SVP_PERFORMANCE_H__

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "GstPerf.h"

extern "C" {
#include <sec_security_datatype.h>
// #include <sec_security.h>
}


// Feature Control
#define GST_SVP_PERF 1

#if !defined(__OCDM_WRAPPER_H_)
    // Only define for OCDM component, 
    //#define ENABLE_OCDM_PROFILING 1
#endif

// class DebugFunction
// {
// public:
//     DebugFunction(const char* strFN) : _strFN(strFN)
//     {
//         LOG(eError, "%s ENTER TID %X\n", _strFN, pthread_self());
//     }
//     ~DebugFunction() 
//     {
//         LOG(eError, "%s EXIT TID %X\n", _strFN, pthread_self());
//     }
// private:
//     const char* _strFN;
// };
// #define DBG_FN()     DebugFunction dbg_fn(__FUNCTION__)
#define PERF_FN()     GstPerf(__FUNCTION__)

#ifdef GST_SVP_PERF
// Sec API Opaque Buffers
#define SecOpaqueBuffer_Malloc(a, b) GstPerf_SecOpaqueBuffer_Malloc(a, b)
#define SecOpaqueBuffer_Write(a, b, c, d) GstPerf_SecOpaqueBuffer_Write(a, b, c, d)
#define SecOpaqueBuffer_Free(a) GstPerf_SecOpaqueBuffer_Free(a)
#define SecOpaqueBuffer_Release(a, b) GstPerf_SecOpaqueBuffer_Release(a, b)
#define SecOpaqueBuffer_Copy(a, b, c, d, e) GstPerf_SecOpaqueBuffer_Copy(a, b, c, d, e)

#ifdef ENABLE_OCDM_PROFILING
// OCDM
#define opencdm_session_decrypt(a, b, c, d, e, f, g, h, i, j) GstPerf_opencdm_session_decrypt(a, b, c, d, e, f, g, h, i, j)
#endif // ENABLE_OCDM_PROFILING
// Netflix
#endif // GST_SVP_PERF

// SecAPI Opaque Buffer Functions
inline Sec_Result GstPerf_SecOpaqueBuffer_Malloc(SEC_SIZE bufLength, Sec_OpaqueBufferHandle **handle);
inline Sec_Result GstPerf_SecOpaqueBuffer_Write(Sec_OpaqueBufferHandle *handle, SEC_SIZE offset, SEC_BYTE *data, SEC_SIZE length);
inline Sec_Result GstPerf_SecOpaqueBuffer_Free(Sec_OpaqueBufferHandle *handle);
inline Sec_Result GstPerf_SecOpaqueBuffer_Release(Sec_OpaqueBufferHandle *handle, Sec_ProtectedMemHandle **svpHandle);
inline Sec_Result GstPerf_SecOpaqueBuffer_Copy(Sec_OpaqueBufferHandle *out, SEC_SIZE out_offset, Sec_OpaqueBufferHandle *in, SEC_SIZE in_offset, SEC_SIZE num_to_copy);

#ifdef ENABLE_OCDM_PROFILING
// OCDM Decrypt
#if !defined(__OCDM_WRAPPER_H_) // Is open_cdm.h in the include path already
// Forward declarations
typedef uint32_t OpenCDMError;
struct OpenCDMSession;
#endif

OpenCDMError GstPerf_opencdm_session_decrypt(struct OpenCDMSession* session,
                                                    uint8_t encrypted[],
                                                    const uint32_t encryptedLength,
                                                    const uint8_t* IV, uint16_t IVLength,
                                                    const uint8_t* keyId, const uint16_t keyIdLength,
                                                    uint32_t initWithLast15,
                                                    uint8_t* streamInfo,
                                                    uint16_t streamInfoLength);
#endif //ENABLE_OCDM_PROFILING

#endif // __GST_SVP_PERFORMANCE_H__
