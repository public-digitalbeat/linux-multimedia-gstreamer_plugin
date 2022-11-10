/**
 * Copyright 2014 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
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
#ifndef SEC_SECURITY_DATATYPE_H_
#define SEC_SECURITY_DATATYPE_H_

#include <stdint.h>
#include <sys/param.h>
#ifdef __cplusplus

extern "C"
{
#endif

/*****************************************************************************
 * EXPORTED TYPES
 *****************************************************************************/

typedef uint8_t SEC_BYTE;
typedef uint8_t SEC_BOOL;
typedef uint32_t SEC_SIZE;
typedef uint64_t SEC_OBJECTID;

typedef uint32_t secmem_handle_t;

struct Sec_OpaqueBufferHandle_struct
{
    secmem_handle_t secmem_handle;
    SEC_SIZE dataBufSize;
};

typedef enum
{
    SEC_RESULT_SUCCESS = 0,
    SEC_RESULT_FAILURE,
    SEC_RESULT_INVALID_PARAMETERS,
    SEC_RESULT_NO_SUCH_ITEM,
    SEC_RESULT_BUFFER_TOO_SMALL,
    SEC_RESULT_INVALID_INPUT_SIZE,
    SEC_RESULT_INVALID_HANDLE,
    SEC_RESULT_INVALID_PADDING,
    SEC_RESULT_UNIMPLEMENTED_FEATURE,
    SEC_RESULT_ITEM_ALREADY_PROVISIONED,
    SEC_RESULT_ITEM_NON_REMOVABLE,
    SEC_RESULT_VERIFICATION_FAILED,
    SEC_RESULT_NUM
} Sec_Result;


#define SEC_TRUE 1
#define SEC_FALSE 0


/**
 * @brief Opaque buffer handle
 */
typedef struct Sec_OpaqueBufferHandle_struct Sec_OpaqueBufferHandle;

typedef void Sec_ProtectedMemHandle;

#ifdef __cplusplus
}
#endif
#endif /* SEC_SECURITY_DATATTYPE_H_ */