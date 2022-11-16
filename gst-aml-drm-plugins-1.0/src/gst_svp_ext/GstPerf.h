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


#ifndef __GST_PERF_H__
#define __GST_PERF_H__

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>

#include <string>
#include <list>
#include <map>
#include <stack>


#define THREAD_NAMELEN 16
#define PROCESS_NAMELEN 80

typedef struct _TimingStats
{
    std::string         elementName;
    uint64_t            nTotalTime;
    double              nTotalAvg;
    uint64_t            nTotalMax;
    uint64_t            nTotalMin;
    uint64_t            nTotalCount;
    uint64_t            nIntervalTime;
    double              nIntervalAvg;
    uint64_t            nIntervalMax;
    uint64_t            nIntervalMin;
    uint64_t            nIntervalCount;
    uint64_t            nLastDelta;
} TimingStats;

class PerfTree;
class PerfProcess;
class PerfNode
{
    public:
    PerfNode(); // For root node
    PerfNode(PerfNode* pNode);
    PerfNode(std::string elementName);
    ~PerfNode();
    
    PerfNode* AddChild(PerfNode * pNode);
    static uint64_t TimeStamp();

    std::string& GetName() { return m_elementName; };
    void SetTree(PerfTree* pTree) { m_Tree = pTree; };
    void SetThreshold(uint32_t nUS) { m_TheshholdInUS = (int32_t)nUS; };

    void IncrementData(uint64_t deltaTime);
    void ResetInterval();

    void ReportData(uint32_t nLevel, bool bShowOnlyDelta = false);

    private:
    pthread_t               m_idThread;
    std::string             m_elementName;
    TimingStats             m_stats;
    uint64_t                m_startTime;
    PerfNode*               m_nodeInTree;
    PerfTree*               m_Tree;
    int32_t                 m_TheshholdInUS;
    std::map<std::string, PerfNode*>    m_childNodes;
};

class PerfTree
{
    public:
    PerfTree();
    ~PerfTree();

    void AddNode(PerfNode * pNode);
    void CloseActiveNode(PerfNode * pTreeNode);
    void ReportData();

    bool IsInactive() { return m_ActivityCount == m_CountAtLastReport; };
    char * GetName() { return m_ThreadName; };
    std::stack<PerfNode*> GetStack() { return m_activeNode; }
    pthread_t GetThreadID() { return m_idThread; };

    private:
    pthread_t               m_idThread;
    PerfNode*               m_rootNode;
    char                    m_ThreadName[THREAD_NAMELEN];
    uint64_t                m_ActivityCount;
    uint64_t                m_CountAtLastReport;
    std::stack<PerfNode*>   m_activeNode;
};

class PerfProcess
{
    public:
    PerfProcess(pid_t pID);
    ~PerfProcess();

    PerfTree* GetTree(pthread_t tID);
    PerfTree* NewTree(pthread_t tID);
    void ShowTrees();
    void ShowTree(PerfTree* pTree);
    void ReportData();
    void GetProcessName();
    bool CloseInactiveThreads();
    bool RemoveTree(pthread_t tID);

    private:
    pid_t                           m_idProcess;
    char                            m_ProcessName[PROCESS_NAMELEN];
    std::map<pthread_t, PerfTree*>  m_mapThreads;
};

class GstPerf
{
public:
    GstPerf(const char* szName, uint32_t nThresholdInUS);
    GstPerf(const char* szName);
    ~GstPerf();

    void SetThreshhold(uint32_t nThresholdInUS);

private:
    PerfNode        m_node;
};

void GstPerf_ReportProcess(pid_t pID);
void GstPerf_ReportThread(pthread_t tID);
void GstPerf_CloseThread(pthread_t tID);

#endif // __GST_PERF_H__
