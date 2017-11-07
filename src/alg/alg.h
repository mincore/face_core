/* =====================================================================
 * Copyright (C) 2017 chenshangping All Right Reserved.
 *      Author: chenshuangping(mincore@163.com)
 *    Filename: alg/alg.h
 * Description:
 * =====================================================================
 */
#ifndef _ALG_ALG_H
#define _ALG_ALG_H

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <atomic>
#include "../common/common.h"
#include "../msg/pb/fastore.pb.h"
#include "taskq/taskq.h"
#include "faapi.h"

using namespace fastore;

typedef FAImg Img;
typedef FARect Rect;
typedef std::vector<float> Feature;

struct DetectResults {
    std::vector<DetectResult> data;
    uint64_t detectID;
};

typedef std::vector<ExtractResult> ExtractResults;
typedef std::vector<SearchResult> SearchResults;

FABuffer FABufferAlloc(int size);
bool FABufferFree(FABuffer &buf);
FABuffer& FABufferRef(FABuffer &buf);

class Tracker;
class ExtractorProxy;
class Alg: public singleton<Alg> {
public:
    bool Init();
    bool Release();

public:
    bool Detect(const FADetectParam &param, DetectResults &results);
    bool Extract(const Img &img, const FABuffer &faceInfo, ExtractResult &result);
    float Compare(const FABuffer &f1, const FABuffer &f2);
    bool Search(const FASearchParam *param, SearchResults &results);
    int GetDim();

public:
    int CreateDetectChannel();
    int DestroyDetectChannel(int channel);

private:
    Tracker* GetTracker(int channel);
    bool DoDetect(const FADetectParam &param, DetectResults &results);
    bool DoTrack(const FADetectParam &param, DetectResults &results, Tracker *tracker);

private:
    std::shared_ptr<TaskQueue> m_detect_taskq;
    std::shared_ptr<TaskQueue> m_cmp_taskq;

    std::mutex m_trackerMapMutex;
    std::unordered_map<int, Tracker*> m_trackerMap;

    std::atomic<int> m_dim;
    std::shared_ptr<ExtractorProxy> m_extracter;
};

#endif
