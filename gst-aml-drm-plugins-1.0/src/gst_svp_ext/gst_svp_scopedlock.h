/* Copyright (C) 2021 RDK Management.  All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS. OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
* OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#ifndef __GST_SVP_SCOPEDLOCK_H__
#define __GST_SVP_SCOPEDLOCK_H__

#include <unistd.h>     // For usleep()
#include "gst_svp_logging.h"

#ifdef USE_LIBC_SCOPED_LOCK
#include <mutex>
#else
#include <pthread.h>
#endif

#ifdef USE_LIBC_SCOPED_LOCK
extern std::recursive_mutex _lock;
#define SCOPED_LOCK()     std::lock_guard<std::recursive_mutex> lock(_lock)
#define SCOPED_TRY_LOCK(timeoutMS, failRetVal)  \
    int     nTryCount = 0;                      \
    bool    bLockAcquired = false;              \
    while(nTryCount < timeoutMS) {              \
        bLockAcquired = _lock.try_lock();       \
        if(bLockAcquired == false) {            \
            nTryCount++;                        \
            usleep(1000); /* 1ms */             \
        }                                       \
        else {                                  \
            break;                              \
        }                                       \
    }                                           \
    if(bLockAcquired != true) {                 \
        LOG(eError, "%s failed to acquire mutex after %d ms\n", \
                    __FUNCTION__, timeoutMS);   \
        return failRetVal;                      \
    }                                           \
    SCOPED_LOCK();                              \
    _lock.unlock()
#else
class ScopedMutex
{
public:
    ScopedMutex(const char* strFN);
    ScopedMutex(const char* strFN, uint32_t timeoutMS);
    ~ScopedMutex();

    bool IsMutexLocked() { return _bMutexAcquired; };

private:
    void InitMutex(pthread_mutex_t* pLock);

    const char* _strFN;
    bool                    _bMutexAcquired;

    static bool _bMutexInit;
    static pthread_mutex_t _lock;
};

#define SCOPED_LOCK()     ScopedMutex lock(__FUNCTION__)
#define SCOPED_TRY_LOCK(timeoutMS, failRetVal)                  \
    ScopedMutex lock(__FUNCTION__, timeoutMS);                  \
    if(!lock.IsMutexLocked()) {                                 \
        LOG(eError, "%s failed to acquire mutex after %d ms\n", \
                    __FUNCTION__, timeoutMS);                   \
        return failRetVal;                                      \
    }
#endif

#endif //__GST_SVP_SCOPEDLOCK_H__
