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


#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#include <string>
#include <map>

#include <gst/gst.h>

extern "C" {
#include <sec_security_datatype.h>
// #include <sec_security.h>
}

#include "gst_svp_logging.h"
#include "gst_svp_scopedlock.h"
#include "GstPerf.h"

#define REPORTING_INITIAL_COUNT 5000
#define REPORTING_INTERVAL_COUNT 20000
#define TIMER_INTERVAL_SECONDS 10
#define MAX_DELAY 60
#define MAX_BUF_SIZE 2048
#define INITIAL_MIN_VALUE 1000000000

std::map<pid_t, PerfProcess*> s_ProcessMap;

static void __attribute__((constructor)) PerfModuleInit();
static void __attribute__((destructor)) PerfModuleTerminate();

static guint s_timerID = 0;

static gboolean TimerCallback (gpointer pData)
{
    gboolean bTimerContinue = true;
    static uint32_t nDelay = 0;
    static uint32_t nCount = 0;
    //std::map<pid_t, PerfProcess*>* pMap = (std::map<pid_t, PerfProcess*>*)pData;

    // Validate that threads in process are still active
    auto it = s_ProcessMap.find(getpid());
    if(it != s_ProcessMap.end()) {
        // Found a process in the list
        if(nCount > nDelay) {
            GstPerf_ReportProcess(getpid());
            nCount = 0;
            if(nDelay < MAX_DELAY) {
                nDelay+=5;
            }
            LOG(eWarning, "Next performance log in %d seconds\n", nDelay * TIMER_INTERVAL_SECONDS);
        }
        nCount++;
    }
    else {
        LOG(eTrace, "Could not find Process ID %X for reporting\n", (uint32_t)getpid());
    }

    // Print report for process.
    return bTimerContinue;
}
// This function is assigned to execute as a library init
//  using __attribute__((constructor))
static void PerfModuleInit()
{
    char cmd[80] = { 0 };
    char strProcessName[PROCESS_NAMELEN];

    sprintf(cmd, "/proc/%d/cmdline", getpid());
    FILE* fp = fopen(cmd,"r");
    if(fp != NULL) {
        size_t size = fread(strProcessName, sizeof(char), PROCESS_NAMELEN - 1, fp);
        if(size <= 0) {
            LOG(eError, "Could not read process name\n");
        }
        fclose(fp);
    }

    LOG(eWarning, "GST performance process initialize %X named %s\n", getpid(), strProcessName);
    if(s_timerID == 0) {
        s_timerID = g_timeout_add_seconds(TIMER_INTERVAL_SECONDS, (GSourceFunc)TimerCallback, (gpointer)&s_ProcessMap);
        LOG(eTrace, "Created new timer (%u) with the context %p\n", s_timerID, NULL);
    }
}
  
// This function is assigned to execute as library unload
// using __attribute__((destructor))
static void PerfModuleTerminate()
{
    pid_t pID = getpid();

    LOG(eWarning, "GST performance process terminate %X\n", pID);
    GstPerf_ReportProcess(pID);

    // Find thread in process map
    auto it = s_ProcessMap.find(pID);
    if(it == s_ProcessMap.end()) {
        LOG(eError, "Could not find Process ID %X for reporting\n", (uint32_t)pID);
        return;
    }
    else {
        delete it->second;
        s_ProcessMap.erase(it);
    }

    // Stop the timer
    g_source_remove(s_timerID);
    s_timerID = 0;

}

PerfTree::PerfTree()
:m_idThread(0), m_rootNode(NULL), m_ActivityCount(0), m_CountAtLastReport(0)
{
    memset(m_ThreadName, 0, THREAD_NAMELEN);
    return;
}
PerfTree::~PerfTree()
{
    delete m_rootNode;

    return;
}

void PerfTree::AddNode(PerfNode * pNode)
{
    PerfNode* pTreeNode = NULL;
    PerfNode* pTop      = NULL;

    //Get last opended node
    if(m_activeNode.size() > 0) {
        pTop = m_activeNode.top();
    }
    else {
        // New Tree
        m_rootNode = new PerfNode();    // root node special constructor
        m_activeNode.push(m_rootNode);
        m_idThread = pthread_self();
        pthread_getname_np(m_idThread, m_ThreadName, THREAD_NAMELEN);
        pTop = m_activeNode.top();
        LOG(eWarning, "Creating new Tree stack size = %d for node %s, thread name %s\n", 
            m_activeNode.size(), pNode->GetName().c_str(), m_ThreadName);
    }
    pTreeNode = pTop->AddChild(pNode);
    m_activeNode.push(pTreeNode);
    m_ActivityCount++;

    pNode->SetTree(this);

    return;
}

void PerfTree::CloseActiveNode(PerfNode * pTreeNode)
{
    //Get last opended node
    PerfNode* pTop = m_activeNode.top();
    if(pTop != NULL) {
        // There is an active node
        if(pTreeNode != pTop) {
            // Error
            LOG(eError, "Not closeing the active node(%s != %s)\n", 
                pTop->GetName().c_str(), pTreeNode->GetName().c_str());
        }
        else {
            // LOG(eTrace, "Closeing the active node %s\n", 
            //             pTop->GetName().c_str());
            m_activeNode.pop();
        }
    }

    return;
}

void PerfTree::ReportData()
{
    // Get the root node and walk down the tree
    LOG(eWarning, "Printing report on %X thread named %s\n", (uint32_t)m_idThread, m_ThreadName);
    m_rootNode->ReportData(0);
    
    // Update the activity monitor
    m_CountAtLastReport = m_ActivityCount;

    return;
}

PerfProcess::PerfProcess(pid_t pID)
: m_idProcess(pID)
{
    memset(m_ProcessName, 0, PROCESS_NAMELEN);
    GetProcessName();
    LOG(eWarning, "Creating new process with named <%.*s>\n",
                  sizeof(m_ProcessName) <= PROCESS_NAMELEN ? (int)sizeof(m_ProcessName) : (int)PROCESS_NAMELEN, 
                  m_ProcessName);
    return;
}
PerfProcess::~PerfProcess()
{
    auto it = m_mapThreads.begin();
    while(it != m_mapThreads.end()) {
        delete it->second;
        it++;
    }
    return;
}
bool PerfProcess::CloseInactiveThreads()
{
    bool retVal = false;

    auto it = m_mapThreads.begin();
    while(it != m_mapThreads.end()) {
        PerfTree* pTree = it->second;
        if(pTree->IsInactive()) {
            LOG(eWarning, "Thread %s is inactive, removing from process %.*s\n", 
                           pTree->GetName(),
                           sizeof(m_ProcessName) <= PROCESS_NAMELEN ? (int)sizeof(m_ProcessName) : (int)PROCESS_NAMELEN, 
                           m_ProcessName);
            // Remove inactive thread from tree
            delete it->second;
            it = m_mapThreads.erase(it);
            retVal = true;
        }
        else {
            it++;
        }
    }
    return retVal;
}

bool PerfProcess::RemoveTree(pthread_t tID)
{
    bool retVal = false;
    // Remove tree from process list.
    auto it = m_mapThreads.find(tID);
    if(it != m_mapThreads.end()) {
        // Remove from list
        delete it->second;
        it = m_mapThreads.erase(it);
        retVal = true;
    }

    return retVal;
}

void PerfProcess::GetProcessName()
{
    char cmd[80] = { 0 };
    sprintf(cmd, "/proc/%d/cmdline", m_idProcess);
    FILE* fp = fopen(cmd,"r");
    if(fp != NULL) {
        size_t size = fread(m_ProcessName, sizeof(char), PROCESS_NAMELEN - 1, fp);
        if(size <= 0) {
            LOG(eError, "Could not read process name\n");
        }
        fclose(fp);
    }
    return;
}

PerfTree* PerfProcess::GetTree(pthread_t tID)
{
    PerfTree* pTree = NULL;

    auto it = m_mapThreads.find(tID);
    if(it != m_mapThreads.end()) {
        pTree = it->second;
    }
    else {
        LOG(eWarning, "Could not find %X in thread map Possible matches are:\n", tID);
        auto it2 = m_mapThreads.begin();
        while(it2 != m_mapThreads.end()) {
            LOG(eWarning, "\tKey %X, ID %X\n", it2->first, it2->second->GetThreadID());
            it2++;
        }
    }

    return pTree;
}

PerfTree* PerfProcess::NewTree(pthread_t tID)
{
    PerfTree* pTree = NULL;

    auto it = m_mapThreads.find(tID);
    if(it == m_mapThreads.end()) {
        // Cound not find thread in list
        pTree = new PerfTree();
        m_mapThreads[tID] = pTree;
    }
    else {
        LOG(eError, "Trying to add a thread %d to a map where it already exists name = %s\n", 
                    tID, it->second->GetName());
    }

    return pTree;
}

void PerfProcess::ShowTrees()
{
    LOG(eWarning, "Displaying %d threads in thread map for process %s\n", m_mapThreads.size(), m_ProcessName);
    auto it = m_mapThreads.begin();
    while(it != m_mapThreads.end()) {
        LOG(eWarning, "Tree = %p\n", it->second);
        if(it->second != NULL) {
            ShowTree(it->second);
        }
        it++;
    }
}

void PerfProcess::ShowTree(PerfTree* pTree)
{
    // Describe Thread
    LOG(eWarning, "Found Thread %X in tree named %s with stack size %d\n", 
        pTree->GetThreadID(),
        pTree->GetName(),
        pTree->GetStack().size());
    // Show Stack
    LOG(eWarning, "Top Node = %p name %s\n", pTree->GetStack().top(), pTree->GetStack().top()->GetName().c_str());

    return;
}

void PerfProcess::ReportData()
{
    // Print reports for all the trees in this process.
    LOG(eWarning, "Found %d threads in this process %X named <%.*s>\n", 
                  m_mapThreads.size(), (uint32_t)m_idProcess, 
                  sizeof(m_ProcessName) <= PROCESS_NAMELEN ? (int)sizeof(m_ProcessName) : (int)PROCESS_NAMELEN, 
                  m_ProcessName);
    auto it = m_mapThreads.begin();
    while(it != m_mapThreads.end()) {
        it->second->ReportData();
        it++;
    }    
    
    return;
}

PerfNode::PerfNode()
: m_elementName("root_node"), m_nodeInTree(NULL), m_Tree(NULL), m_TheshholdInUS(-1)
{
#ifndef DISABLE_METRICS
    SCOPED_LOCK();

    m_startTime     = TimeStamp();
    m_idThread      = pthread_self();

    memset(&m_stats, 0, sizeof(TimingStats));
    m_stats.nTotalMin     = INITIAL_MIN_VALUE;      // Preset Min values to pickup the inital value
    m_stats.nIntervalMin  = INITIAL_MIN_VALUE;
    m_stats.elementName   = m_elementName;

    LOG(eWarning, "Creating node for element %s\n", m_elementName.c_str());
#endif // !DISABLE_METRICS
    return;
}

PerfNode::PerfNode(PerfNode* pNode)
: m_nodeInTree(NULL), m_Tree(NULL), m_TheshholdInUS(-1)
{
#ifndef DISABLE_METRICS
    SCOPED_LOCK();

    m_idThread      = pNode->m_idThread;
    m_elementName   = pNode->m_elementName;
    m_startTime     = pNode->m_startTime;
    m_childNodes    = pNode->m_childNodes;

    memset(&m_stats, 0, sizeof(TimingStats));
    m_stats.nTotalMin     = INITIAL_MIN_VALUE;      // Preset Min values to pickup the inital value
    m_stats.nIntervalMin  = INITIAL_MIN_VALUE;
    m_stats.elementName   = m_elementName;
#endif // !DISABLE_METRICS
    return;
}

PerfNode::PerfNode(std::string elementName)
: m_elementName(elementName), m_nodeInTree(NULL), m_Tree(NULL), m_TheshholdInUS(-1)
{
#ifndef DISABLE_METRICS
    pid_t           pID = getpid();
    PerfProcess*    pProcess = NULL;

    SCOPED_LOCK();

    //LOG(eWarning, "Creating node for element %s\n", m_elementName.c_str());
    // Find thread in process map
    auto it = s_ProcessMap.find(pID);
    if(it == s_ProcessMap.end()) {
        // no existing PID in map
        pProcess = new PerfProcess(pID);
        s_ProcessMap[pID] = pProcess;

        LOG(eWarning, "Creating new process element %X\n", pProcess);
    }
    else {
        pProcess = it->second;
    }

    m_startTime = TimeStamp();
    m_idThread = pthread_self();

    // Found PID get element tree for current thread.
    PerfTree* pTree = pProcess->GetTree(m_idThread);
    if(pTree == NULL) {
        pTree = pProcess->NewTree(m_idThread);
    }
    
    if(pTree) {
        pTree->AddNode(this);
    }
#endif // !DISABLE_METRICS
    return;
}

PerfNode::~PerfNode()
{
#ifndef DISABLE_METRICS
    SCOPED_LOCK();

    if(m_nodeInTree != NULL) {
        // This is a node in the code
        uint64_t deltaTime = TimeStamp() - m_startTime;
        m_nodeInTree->IncrementData(deltaTime);
        m_Tree->CloseActiveNode(m_nodeInTree);
        if(m_TheshholdInUS > 0 && deltaTime > (uint64_t)m_TheshholdInUS) {
            LOG(eWarning, "%s Threshold %ld exceeded, elapsed time = %0.3lf ms Avg time = %0.3lf (interval %0.3lf) ms\n", 
                          GetName().c_str(), 
                          m_TheshholdInUS / 1000,
                          ((double)deltaTime) / 1000.0,
                          ((double)m_nodeInTree->m_stats.nTotalTime / (double)m_nodeInTree->m_stats.nTotalCount) / 1000.0,
                          ((double)m_nodeInTree->m_stats.nIntervalTime / (double)m_nodeInTree->m_stats.nIntervalCount) / 1000.0);
            m_nodeInTree->ReportData(0, true);
        }
    }
    else {
        // This is a node in the tree and should be cleaned up
        auto it = m_childNodes.begin();
        while(it != m_childNodes.end()) {
            delete it->second;
            it++;
        }
    }
#endif // !DISABLE_METRICS
    return;
}
PerfNode* PerfNode::AddChild(PerfNode * pNode)
{
#ifndef DISABLE_METRICS
    SCOPED_LOCK();

    // Does this node exist in the list of children
    auto it = m_childNodes.find(pNode->GetName());
    if(it == m_childNodes.end()) {
        // new child
        // Create copy of Node for storage in the tree
        PerfNode* copyNode = new PerfNode(pNode);
        m_childNodes[pNode->GetName()] = copyNode;
        pNode->m_nodeInTree = copyNode;
    }
    else {
        pNode->m_nodeInTree = it->second;
    }

    return pNode->m_nodeInTree;
#else 
    return NULL;
#endif // !DISABLE_METRICS

}

void PerfNode::IncrementData(uint64_t deltaTime)
{
#ifndef DISABLE_METRICS
   SCOPED_LOCK();

    // Increment totals
    m_stats.nLastDelta = deltaTime;
    m_stats.nTotalTime += deltaTime;
    m_stats.nTotalCount++;
    if(m_stats.nTotalMin > deltaTime) {
        m_stats.nTotalMin = deltaTime;
    }
    if(m_stats.nTotalMax < deltaTime) {
        m_stats.nTotalMax = deltaTime;
    }
    m_stats.nTotalAvg = (double)m_stats.nTotalTime / (double)m_stats.nTotalCount;

    // Increment intervals
    m_stats.nIntervalTime += deltaTime;
    m_stats.nIntervalCount++;
    if(m_stats.nIntervalMin > deltaTime) {
        m_stats.nIntervalMin = deltaTime;
    }
    if(m_stats.nIntervalMax < deltaTime) {
        m_stats.nIntervalMax = deltaTime;
    }
    m_stats.nIntervalAvg = (double)m_stats.nIntervalTime / (double)m_stats.nIntervalCount;
#endif // !DISABLE_METRICS
    return;
}

void PerfNode::ResetInterval()
{
#ifndef DISABLE_METRICS
    SCOPED_LOCK();

    m_stats.nIntervalTime     = 0;
    m_stats.nIntervalAvg      = 0;
    m_stats.nIntervalMax      = 0;
    m_stats.nIntervalMin      = INITIAL_MIN_VALUE;      // Need a large number here to get new min value
    m_stats.nIntervalCount    = 0;
#endif // !DISABLE_METRICS
    return;
}

void PerfNode::ReportData(uint32_t nLevel, bool bShowOnlyDelta)
{
#ifndef DISABLE_METRICS
    SCOPED_LOCK();

    char buffer[MAX_BUF_SIZE] = { 0 };
    char* ptr = &buffer[0];
    // Print the indent 
    for(uint32_t nIdx = 0; nIdx < nLevel; nIdx++) {
        snprintf(ptr, MAX_BUF_SIZE, "--");
        ptr += 2;
    }

    if(bShowOnlyDelta) {
        // Print only the current delta time data 
        snprintf(ptr, MAX_BUF_SIZE - strlen(buffer), "| %s elapsed time %0.3lf\n",
                m_stats.elementName.c_str(),
                (double)m_stats.nLastDelta / 1000.0);
    }
    else {
        // Print data for this node
        snprintf(ptr, MAX_BUF_SIZE - strlen(buffer), "| %s (Count, Max, Min, Avg) Total %lld, %0.3lf, %0.3lf, %0.3lf Interval %lld, %0.3lf, %0.3lf, %0.3lf\n",
                m_stats.elementName.c_str(),
                m_stats.nTotalCount, ((double)m_stats.nTotalMax) / 1000.0, ((double)m_stats.nTotalMin) / 1000.0, m_stats.nTotalAvg / 1000.0,
                m_stats.nIntervalCount, ((double)m_stats.nIntervalMax) / 1000.0, ((double)m_stats.nIntervalMin) / 1000.0, m_stats.nIntervalAvg / 1000.0);
    }
    LOG(eWarning, "%s\n", buffer);
    
    // Print data for all the children
    auto it = m_childNodes.begin();
    while(it != m_childNodes.end()) {
        it->second->ReportData(nLevel + 1, bShowOnlyDelta);
        it++;
    }

    if(!bShowOnlyDelta) {
        ResetInterval();
    }
#endif // !DISABLE_METRICS
    return;
}

// No SCOPED_LOCK on this method as it is
// thread safe
uint64_t PerfNode::TimeStamp() 
{
    struct timeval  timeStamp;
    uint64_t        retVal = 0;

    gettimeofday(&timeStamp, NULL);

    // Convert timestamp to Micro Seconds
    retVal = (uint64_t)(((uint64_t)timeStamp.tv_sec * 1000000) + timeStamp.tv_usec);

    return retVal;
}

GstPerf::GstPerf(const char* szName) 
: m_node(szName)
{
    return;
}
GstPerf::GstPerf(const char* szName, uint32_t nThresholdInUS)
: m_node(szName)
{
    m_node.SetThreshold(nThresholdInUS);
    return;
}

void GstPerf::SetThreshhold(uint32_t nThresholdInUS)
{
    m_node.SetThreshold(nThresholdInUS); 
}

GstPerf::~GstPerf()
{
    return;
}


void GstPerf_ReportProcess(pid_t pID)
{
#ifndef DISABLE_METRICS
    // Find Process ID in List
    PerfProcess*    pProcess = NULL;

    SCOPED_TRY_LOCK(100, void());

    // Find thread in process map
    auto it = s_ProcessMap.find(pID);
    if(it == s_ProcessMap.end()) {
        LOG(eError, "Could not find Process ID %X for reporting\n", (uint32_t)pID);
        return;
    }
    else {
        pProcess = it->second;
    }

    if(pProcess != NULL) {
        pProcess->ShowTrees();
        // Close threads that have no activity since the last report
        pProcess->CloseInactiveThreads();
        LOG(eWarning, "Printing process report for Process ID %X\n", (uint32_t)pID);
        pProcess->ReportData();
    }
#endif // !DISABLE_METRICS
    return;
}
void GstPerf_ReportThread(pthread_t tID)
{
#ifndef DISABLE_METRICS
    // Find Process ID in List
    PerfProcess*    pProcess = NULL;

    SCOPED_TRY_LOCK(100, void());

    // Find thread in process map
    auto it = s_ProcessMap.find(getpid());
    if(it == s_ProcessMap.end()) {
        LOG(eError, "Could not find Process ID %X for reporting\n", (uint32_t)getpid());
        return;
    }
    else {
        pProcess = it->second;
    }

    PerfTree* pTree = pProcess->GetTree(tID);
    if(pTree != NULL) {
        LOG(eWarning, "Printing tree report for Task ID %X\n", (uint32_t)tID);
        pTree->ReportData();
    }
#endif // !DISABLE_METRICS
    return;

}

void GstPerf_CloseThread(pthread_t tID)
{
#ifndef DISABLE_METRICS
    // Find Process ID in List
    PerfProcess*    pProcess = NULL;

    SCOPED_TRY_LOCK(100, void());

    // Find thread in process map
    auto it = s_ProcessMap.find(getpid());
    if(it == s_ProcessMap.end()) {
        LOG(eError, "Could not find Process ID %X for reporting\n", (uint32_t)getpid());
        return;
    }
    else {
        pProcess = it->second;
    }

    if(pProcess != NULL) {
        pProcess->RemoveTree(tID);
    }
#endif // !DISABLE_METRICS
    return;
}

#if 0
static bool load_library(const char * szLibraryName, void** ppHandle)
{
    dlerror();  // clear error

    void * library_handle = dlopen(szLibraryName, RTLD_LAZY);
    if(library_handle == NULL) {
        char* error = dlerror();
        if (error != NULL) {
            LOG(eError, "Could not open library <%s> error = %s\n", szLibraryName, error);
        }
        LOG(eError, "ERROR: could not open library %s\n", szLibraryName);
        *ppHandle = NULL;

        return false;
    }

    // Success
    *ppHandle = library_handle;
    return true;
}

static bool link_function(void* pLibrary, void** ppFunc, const char* szFuncName)
{
    *ppFunc = dlsym(pLibrary, szFuncName);
    if (!*ppFunc) {
        /* no such symbol */
        LOG(eError, "Error for %s : %s\n", szFuncName, dlerror());
        return false;
    }
    return true;
}
#endif

Sec_Result GstPerf_SecOpaqueBuffer_Malloc(SEC_SIZE bufLength, Sec_OpaqueBufferHandle **handle)
{
    // GstPerf perf("SecOpaqueBuffer_Malloc");
    // return SecOpaqueBuffer_Malloc(bufLength, handle);
    return SEC_RESULT_SUCCESS;
}

Sec_Result GstPerf_SecOpaqueBuffer_Write(Sec_OpaqueBufferHandle *handle, SEC_SIZE offset, SEC_BYTE *data, SEC_SIZE length)
{
    // GstPerf perf("SecOpaqueBuffer_Write");
    // return SecOpaqueBuffer_Write(handle, offset, data, length);
    return SEC_RESULT_SUCCESS;
}

Sec_Result GstPerf_SecOpaqueBuffer_Free(Sec_OpaqueBufferHandle *handle)
{
    // GstPerf perf("SecOpaqueBuffer_Free");
    // return SecOpaqueBuffer_Free(handle);
    return SEC_RESULT_SUCCESS;
}

Sec_Result GstPerf_SecOpaqueBuffer_Release(Sec_OpaqueBufferHandle *handle, Sec_ProtectedMemHandle **svpHandle)
{
    // GstPerf perf("SecOpaqueBuffer_Release");
    // return SecOpaqueBuffer_Release(handle, svpHandle);
    return SEC_RESULT_SUCCESS;
}

Sec_Result GstPerf_SecOpaqueBuffer_Copy(Sec_OpaqueBufferHandle *out, SEC_SIZE out_offset, Sec_OpaqueBufferHandle *in, SEC_SIZE in_offset, SEC_SIZE num_to_copy)
{
    // GstPerf perf("SecOpaqueBuffer_Copy");
    // return SecOpaqueBuffer_Copy(out, out_offset, in, in_offset, num_to_copy);
    return SEC_RESULT_SUCCESS;
}

#ifdef ENABLE_OCDM_PROFILING
#error ENABLE_OCDM_PROFILING
// Forward declarations
typedef uint32_t OpenCDMError;
struct OpenCDMSession;
#ifndef EXTERNAL
    #ifdef _MSVC_LANG
        #ifdef OCDM_EXPORTS
        #define EXTERNAL __declspec(dllexport)
        #else
        #define EXTERNAL __declspec(dllimport)
        #endif
    #else
        #define EXTERNAL __attribute__ ((visibility ("default")))
    #endif
#endif

EXTERNAL OpenCDMError opencdm_session_decrypt(struct OpenCDMSession* session,
                                    uint8_t encrypted[],
                                    const uint32_t encryptedLength,
                                    const uint8_t* IV, uint16_t IVLength,
                                    const uint8_t* keyId, const uint16_t keyIdLength,
                                    uint32_t initWithLast15,
                                    uint8_t* streamInfo,
                                    uint16_t streamInfoLength);

uint32_t (*lcl_opencdm_session_decrypt)(struct OpenCDMSession* session,
                                    uint8_t encrypted[],
                                    const uint32_t encryptedLength,
                                    const uint8_t* IV, uint16_t IVLength,
                                    const uint8_t* keyId, const uint16_t keyIdLength,
                                    uint32_t initWithLast15,
                                    uint8_t* streamInfo,
                                    uint16_t streamInfoLength);

OpenCDMError GstPerf_opencdm_session_decrypt(struct OpenCDMSession* session,
                                                    uint8_t encrypted[],
                                                    const uint32_t encryptedLength,
                                                    const uint8_t* IV, uint16_t IVLength,
                                                    const uint8_t* keyId, const uint16_t keyIdLength,
                                                    uint32_t initWithLast15,
                                                    uint8_t* streamInfo,
                                                    uint16_t streamInfoLength)
{
    static void *library_handle = NULL;
    static bool bInitLibrary = false;
    static const char* szLibraryName = "libocdm.so";
    
    if(bInitLibrary == false) {
        if(library_handle == NULL) {
            load_library(szLibraryName, &library_handle);
            LOG(eWarning, "library_handle = %p\n", library_handle);
        }
        if(library_handle) {
            // Library loaded, link symbol
            if(link_function(library_handle, (void**)&lcl_opencdm_session_decrypt, "opencdm_session_decrypt")) {
                // Success
                bInitLibrary = true;
            }
        }
        else {
            LOG(eError, "Invalid Library handle for %s\n", szLibraryName);
        }
    }

    GstPerf perf("opencdm_session_decrypt");

    if(lcl_opencdm_session_decrypt) {
        return lcl_opencdm_session_decrypt(session, encrypted, encryptedLength, IV, IVLength, 
                                    keyId, keyIdLength, initWithLast15, 
                                    streamInfo, streamInfoLength);
    }
    else {
        LOG(eError, "Dynamic function not linked\n");
        return 0x80004005;      // ERROR_FAIL
    }
}
#endif // ENABLE_OCDM_PROFILING
