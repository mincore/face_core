/* ===================================================
 * Copyright (C) 2017 chenshangping All Right Reserved.
 *      Author: chenshuangping(mincore@163.com)
 *    Filename: faapi.cpp
 *     Created: 2017-06-14 13:26
 * Description:
 * ===================================================
 */
#include <algorithm>
#include "faapi.h"
#include "module/module.h"
#include "alg/alg.h"
#include "store/store.h"

#define CHECK_ARGS() do {\
    if (!param || !result) {\
        LOG_ERROR("invalid param");\
        return -1; \
    }\
} while(0)

const int DEFAULT_MINISIZE = 60;
const float DEFAULT_THRESHOLD[3] = { 0.8, 0.7, 0.7 };
const float DEFAULT_TRACK_THRESHOLD = 55.0;

int FAInit()
{
    return RunAllModules();
}

int FADestory()
{
    return ExitAllModules();
}

FABuffer FABufferAlloc(int size)
{
    return {malloc(size), size, 1};
}

bool FABufferFree(FABuffer &buf)
{
    if (__sync_sub_and_fetch(&buf.ref, 1) == 0) {
        free(buf.data);
        return true;
    }
    return false;
}

FABuffer& FABufferRef(FABuffer &buf)
{
    __sync_fetch_and_add(&buf.ref, 1);
    return buf;
}

static void SetDefaultDetectParam(FADetectParam *param)
{
    int &miniSize = param->miniSize;
    if (miniSize <= 0 || miniSize >= 2000) miniSize = DEFAULT_MINISIZE;

    int &maxFace = param->maxFace;
    if (maxFace < 0) maxFace = 0;

    float &trackThreshold = param->trackThreshold;
    if (trackThreshold <= 0 || trackThreshold >= 100) trackThreshold  = DEFAULT_TRACK_THRESHOLD;

    for (int i=0; i<3; i++) {
        float &x = param->detectThreshold[i];
        if (x <= 0 || x >= 1.0)
            x = DEFAULT_THRESHOLD[i];
    }
}

int FADetect(const FADetectParam *param, FADetectResult *result)
{
    CHECK_ARGS();
    DetectResults results;
    static std::atomic<uint64_t> detectID(0);

    SetDefaultDetectParam((FADetectParam*)param);

    results.detectID = detectID++;
    if (!Alg::Instance()->Detect(*param, results)) {
        LOG_DEBUG("detect failed");
        return -1;
    }

    result->count = results.data.size();
    result->result = (DetectResult*)malloc(sizeof(DetectResult) * result->count);
    result->detectID = results.detectID;

    for (int i=0; i<result->count; i++) {
        result->result[i] = results.data[i];
    }

    return 0;
}

int FADetectFree(FADetectResult *result)
{
    for (int i=0; i<result->count; i++) {
        FABufferFree(result->result[i].faceInfo);
    }
    free(result->result);
    return 0;
}

int FASearch(const FASearchParam *param, FASearchResult *result)
{
    CHECK_ARGS();

    SearchResults searchResults;
    if (!Alg::Instance()->Search(param, searchResults)) {
        LOG_DEBUG("search failed");
        return -1;
    }

    result->count = searchResults.size();
    result->result = (SearchResult*)malloc(sizeof(SearchResult) * result->count);
    memcpy(result->result, &searchResults[0], sizeof(SearchResult) * result->count);

    return 0;
}

int FASearchFree(FASearchResult *result)
{
    for (int i=0; i<result->count; i++) {
        if (result->result[i].url)
            free(result->result[i].url);
    }
    free(result->result);
    return 0;
}

int FACmpFeature(const FABuffer *f1, const FABuffer *f2, float *score)
{
    *score = Alg::Instance()->Compare(*f1, *f2);
    return 0;
}

int FAExtract(const FAImg *img, const FABuffer *faceInfo, ExtractResult *result)
{
    return Alg::Instance()->Extract(*img, *faceInfo, *result);
}

int FAExtractFree(ExtractResult *result)
{
    FABufferFree(result->feature);
    return 0;
}

int FAInsert(const char *tagName, const FABuffer *feature, uint64_t *faceID, const char *url)
{
    *faceID = Store::Instance()->AddFace(tagName, *feature, url);
    return 0;
}

int FADelete(const char *tagName, uint64_t faceID)
{
    return Store::Instance()->DeleteFace(tagName, faceID);
}

int FAGetTagNames(TagNames *tagNames)
{
    std::vector<std::string> names;
    Store::Instance()->ListTags(names);

    if (names.empty()) {
        tagNames->count = 0;
        tagNames->names = NULL;
        return 0;
    }

    tagNames->count = names.size();
    tagNames->names = (char**)malloc(names.size() * sizeof(void*));

    for (size_t i=0; i<names.size(); i++) {
        tagNames->names[i] = strdup(names[i].c_str());
    }

    return 0;
}

int FAFreeTagNames(TagNames *tagNames)
{
    if (!tagNames->names)
        return 0;

    for (int i=0; i<tagNames->count; i++) {
        free(tagNames->names[i]);
    }
    free(tagNames->names);

    return 0;
}

int FADetelTag(const char *tagName)
{
    Store::Instance()->DeleteTag(tagName);
    return 0;
}

int FAGetTagFeatureCount(const char *tagName)
{
    return Store::Instance()->GetTagFaceCount(tagName);
}

int FACreateDetectChannel()
{
    return Alg::Instance()->CreateDetectChannel();
}

int FADestroyDetectChannel(int channel)
{
    return Alg::Instance()->DestroyDetectChannel(channel);
}
