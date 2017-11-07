/* ===================================================
 * Copyright (C) 2017 chenshangping All Right Reserved.
 *      Author: chenshuangping(mincore@163.com)
 *    Filename: alg.cpp
 *     Created: 2017-06-02 19:12
 * Description:
 * ===================================================
 */
#if USE_NUMA
#include <numa.h>
#endif
#include <atomic>
#include <thread>
#include <future>
#include <cstdlib>
#include <unistd.h>
#include "taskq/taskq.h"
#include "taskq/chan.h"
#include "alg.h"
#include "a/alga.h"
#include "tracker.h"
#include "store/store.h"

#define MILLISECONDS(x) std::chrono::milliseconds(x)

#define MAX_DELAY 5

static thread_local AlgA::Detector* D;

static std::vector<int> g_gpu_ids = {0};

static int ALG_DETECT_THREAD_COUNT = 20;
static int ALG_COMPARE_THREAD_COUNT = 20;

static void LoadIDs(const char *key, std::vector<int> &ids)
{
    char *str = std::getenv(key);
    if (str) {
        ids.clear();
        std::vector<std::string> items;
        strsplit(str, ',', items);
        for (auto &r: items) {
            if (!r.empty()) {
                int id = std::atoi(r.c_str());
                ids.push_back(id);
            }
        }
    }
}

class ExtractorProxy {
public:
    void extract(const Img &img, const FABuffer &faceInfo, ExtractResult &result) {
        AlgA::Extractor::Item *item = new AlgA::Extractor::Item{img, faceInfo};
        add(item);
        result.feature = item->result.get_future().get();
    }

    int GetDim() {
        return dim_;
    }

private:
    void add(AlgA::Extractor::Item *item) {
        item->time = now();
        std::unique_lock<std::mutex> lk(mutex_);
        items_.push_back(item);
        cond_.notify_one();
    }

    int size() {
        return items_.size();
    }

    void run(std::unique_lock<std::mutex> &lk, AlgA::Extractor *e, int gpu, int batch) {
        if (quit_ || size() < batch) return;

        std::vector<AlgA::Extractor::Item*> items(items_.begin(), items_.begin() + batch);
        items_.erase(items_.begin(), items_.begin() + batch);

        lk.unlock();

        auto t1 = now();
        e->Extract(items);
        LOG_INFO("extract %d on gpu:%d featrues use %lldms", batch, gpu, now()-t1);

        for (auto p : items) {
            delete p;
        }

        lk.lock();
    }

    bool trigger(int &delay, int batch) {
        if (quit_) return true;
        if (size() == 0) return false;
        if (batch > 1)
            return size() >= batch;

        delay = now() - (*items_.begin())->time;
        if (delay < MAX_DELAY) {
            delay = MAX_DELAY - delay;
            return false;
        }
        delay = MAX_DELAY;
        return true;
    }

    void loop(Chan<int> *c, int gpu, int batch) {
        int delay = batch > 1 ? 0 : MAX_DELAY;
        int id = (batch << 8) | (gpu & 0xff);
        auto e = new AlgA::Extractor(id);
        if (dim_ == 0) dim_ = e->GetDim();
        c->Write(1);

        std::unique_lock<std::mutex> lk(mutex_);
        for (;!quit_;) {
            cond_.wait_for(lk, MILLISECONDS(delay), [&, this]{return trigger(delay, batch);});
            run(lk, e, gpu, batch);
        }
    }

    void init() {
        LoadIDs("ALG_EXTRACT_BATCHS", batchs_);
        std::sort(batchs_.begin(), batchs_.end(), std::greater<int>());
        int n = g_gpu_ids.size() * batchs_.size();
        Chan<int> c(n);
        for (auto gpu : g_gpu_ids) {
            for (auto batch: batchs_) {
                LOG_INFO("creating extract batch:%d on gpu:%d", batch, gpu);
                threads_.push_back(std::thread(&ExtractorProxy::loop, this, &c, gpu, batch));
            }
        }
        c.ReadN(n);
        LOG_INFO("create extractors done");
    }
public:
    ExtractorProxy(): dim_(0), quit_(0) {init();}
    ~ExtractorProxy() {
        quit_ = true;
        cond_.notify_all();
        for (auto &t : threads_)
            t.join();
    }

private:
    std::vector<std::atomic<uint64_t> > threads_stats_;
    std::vector<std::thread> threads_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::vector<AlgA::Extractor::Item*> items_;
    std::atomic<int> dim_;
    std::atomic<int> quit_;
    std::vector<int> batchs_ = {20, 15, 10, 5, 1};
};

static void LoadConfig()
{
    const char *str;

    str = std::getenv("ALG_DETECT_THREAD_COUNT");
    if (str)
        ALG_DETECT_THREAD_COUNT = std::atoi(str);

    str = std::getenv("ALG_COMPARE_THREAD_COUNT");
    if (str)
        ALG_COMPARE_THREAD_COUNT = std::atoi(str);

    LoadIDs("GPU_IDS", g_gpu_ids);
}

bool Alg::Init()
{
    LoadConfig();
    AlgA::Init();

    int n = ALG_DETECT_THREAD_COUNT;
    Chan<int> c(n);

    m_detect_taskq.reset(new TaskQueue(ALG_DETECT_THREAD_COUNT,
        [&] {
            static std::atomic<int> index(0);
            int gpu = g_gpu_ids[(index++)%g_gpu_ids.size()];
            LOG_INFO("creating detector on gpu:%d", gpu);
            D = new AlgA::Detector(gpu);
            c.Write(1);
        }
    ));

    c.ReadN(n);
    LOG_INFO("create detectors done");

    m_extracter.reset(new ExtractorProxy);
    m_cmp_taskq.reset(new TaskQueue(ALG_COMPARE_THREAD_COUNT));


    return true;
}

int Alg::GetDim() {
    return m_extracter->GetDim();
}

bool Alg::Release()
{
    return true;
}

bool Alg::Detect(const FADetectParam &param, DetectResults &results)
{
#ifdef COST_PROFILING
    auto t1 = now();
#endif
    if (!DoDetect(param, results))
        return false;
#ifdef COST_PROFILING
    printf("track step1: detect use %lldms\n", now() - t1);
#endif
    Tracker *tracker = GetTracker(param.channel);
    if (tracker) {
        return DoTrack(param, results, tracker);
    }
    return true;
}

bool Alg::DoDetect(const FADetectParam &param, DetectResults &results)
{
#ifdef COST_PROFILING
    auto t1 = now();
#endif
    Chan<int> c;
    m_detect_taskq->Push([&] {
        D->Detect(param, results);
        c.Write(1);
    });
    c.Read();
#ifdef COST_PROFILING
    printf("---step1 + step2, async Detect use %lldms\n", now() - t1);
#endif

    return !results.data.empty();
}

bool Alg::Extract(const Img &img, const FABuffer &faceInfo, ExtractResult &result)
{
    m_extracter->extract(img, faceInfo, result);
    return true;
}

static inline float ScoreToPercent(float score)
{
    return (1 - pow(score/2, 2)) * 100;
}

float Alg::Compare(const FABuffer &f1, const FABuffer &f2)
{
    cv::Mat feam1 = cv::Mat(1, f1.size/sizeof(float), CV_32F, f1.data);
    cv::Mat feam2 = cv::Mat(1, f2.size/sizeof(float), CV_32F, f2.data);
    float score = cv::norm(feam1, feam2);
    return ScoreToPercent(score);
}

static void SortAndCutSearchReuslts(SearchResults &results, int maxResult)
{
    std::sort(results.begin(), results.end(),
        [](const SearchResult &a, const SearchResult &b) {
            return a.score < b.score;
        });

    if (results.size() > (size_t)maxResult) {
        for (size_t i=maxResult; i<results.size(); i++) {
            if (results[i].url)
                free(results[i].url);
        }

        results.resize(maxResult);
    }
}

bool Alg::Search(const FASearchParam *param, SearchResults &results)
{
    std::atomic<size_t> index(0);
    Chan<int> chan;
    bool searchIDs = param->faceIDs && param->faceIDCount > 0;
    std::vector<SearchResults> all(param->tagCount + (searchIDs ? 1 : 0));

    for (int i=0; i<param->tagCount; i++) {
        m_cmp_taskq->Push([=, &chan, &index, &all] {
            Store::Instance()->Search(param->tagNames[i], param->feature,
                    param->maxFaceCount, all[i]);
            if (++index == all.size())
                chan.Write(1);
        });
    }

    if (searchIDs) {
        m_cmp_taskq->Push([=, &chan, &index, &results, &all] {
            Store::Instance()->Search(param->faceIDs, param->faceIDCount, param->feature,
                    param->maxFaceCount, results);
            if (++index == all.size())
                chan.Write(1);
        });
    }

    chan.Read();

    for (auto &sub : all) {
        results.insert(results.end(), sub.begin(), sub.end());
    }

    SortAndCutSearchReuslts(results, param->maxFaceCount);

    for (auto &r : results) {
        r.score = ScoreToPercent(r.score);
    }

    return !results.empty();
}

bool Alg::DoTrack(const FADetectParam &param, DetectResults &results, Tracker *tracker)
{
#ifdef COST_PROFILING
    auto t1 = now();
#endif
    tracker->Track(results, param.trackThreshold);
#ifdef COST_PROFILING
    printf("track use %lldms\n", now() - t1);
#endif
    return true;
}

static inline int FARectWidth(const FARect &rc)
{
    return rc.right - rc.left + 1;
}

static inline int FARectHeight(const FARect &rc)
{
    return rc.bottom - rc.top + 1;
}

int Alg::CreateDetectChannel()
{
    static std::atomic<int> channel(1);
    std::unique_lock<std::mutex> lk(m_trackerMapMutex);
    m_trackerMap[channel] = new Tracker();
    return channel++;
}

int Alg::DestroyDetectChannel(int channel)
{
    std::unique_lock<std::mutex> lk(m_trackerMapMutex);
    auto it = m_trackerMap.find(channel);
    if (it != m_trackerMap.end()) {
        delete it->second;
        m_trackerMap.erase(it);
    }
    return 0;
}

Tracker* Alg::GetTracker(int channel)
{
    std::unique_lock<std::mutex> lk(m_trackerMapMutex);
    auto it = m_trackerMap.find(channel);
    return it == m_trackerMap.end() ? NULL : it->second;
}
