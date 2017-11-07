/* =====================================================================
 * Copyright (C) 2017 chenshangping All Right Reserved.
 *      Author: chenshuangping(mincore@163.com)
 *    Filename: store.cpp
 * Description:
 * =====================================================================
 */
#include <math.h>
#include <mutex>
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include "store.h"
#include "../msg/pb/fastore.pb.h"
#include "../alg/a/alga.h"

using namespace leveldb;

#define DB_NAME     "./db"
#define PREFIX_FID   "FID_"
#define PREFIX_TAG   "TAG_"

#define DELETED 1

bool Store::Init()
{
    Options opts;
    opts.create_if_missing = true;
    assert(DB::Open(opts, DB_NAME, &m_db).ok());
    m_nextFaceID = 0;
    m_nextTagID = 0;

    std::unordered_map<TagID, Tag> tagIdMap;
    if (!LoadTags(tagIdMap))
        return false;

    if (!LoadFaces(tagIdMap))
        return false;

    BuildIndex(tagIdMap);

    return true;
}

bool Store::Release()
{
    delete m_db;
    return true;
}

bool Store::LoadTags(std::unordered_map<TagID, Tag> &tagIdMap)
{
    int len = strlen(PREFIX_TAG);
    Iterator* iter = m_db->NewIterator(ReadOptions());

    for (iter->Seek(PREFIX_TAG); iter->Valid(); iter->Next()) {
        const char *key = iter->key().data();
        size_t size = iter->key().size();
        if (0 != memcmp(key, PREFIX_TAG, len))
            break;

        std::string tagName(key+len, size-len);
        Tag &tag = m_tagMap[tagName] = Tag(new _Tag);
        tag->m_name = tagName;
        tag->m_deleted = 0;
        tag->m_attr.ParseFromArray(iter->value().data(), iter->value().size());

        tagIdMap[tag->m_attr.tagid()] = tag;

        auto tagId = tag->m_attr.tagid();
        if (m_nextTagID < tagId+1)
            m_nextTagID = tagId+1;

        LOG_INFO("load tag: %s", tagName.c_str());
    }

    delete iter;
    return true;
}

bool Store::LoadFaces(const std::unordered_map<TagID, Tag> &tagIdMap)
{
    int len = strlen(PREFIX_FID);
    Iterator* iter = m_db->NewIterator(ReadOptions());

    for (iter->Seek(PREFIX_FID); iter->Valid(); iter->Next()) {
        const char *key = iter->key().data();
        if (0 != memcmp(key, PREFIX_FID, len))
            break;

        FaceID faceID = atoll(iter->key().data() + len);
        if (m_nextFaceID < faceID+1)
            m_nextFaceID = faceID+1;

        FaceAttr faceAttr;
        faceAttr.ParseFromArray(iter->value().data(), iter->value().size());
        if (faceAttr.flags() & DELETED)
            continue;

        m_faceMap[faceID] = faceAttr;

        for (auto &tagId : faceAttr.tagids()) {
            auto it = tagIdMap.find(tagId);
            if (it == tagIdMap.end()) {
                LOG_WARN("NO TAG:%d FOUND", tagId);
                continue;
            }

            Tag tag = it->second;
            LOG_DEBUG("push FaceID:%lld to tag:%s", (long long)faceID,
                    tag->m_name.c_str());
            tag->m_cached_faceAttrs.push_back(&m_faceMap[faceID]);
        }
    }

    LOG_INFO("load faces: %zd", m_faceMap.size());

    delete iter;
    return true;
}

void Store::BuildIndex(std::unordered_map<TagID, Tag> &tagIdMap)
{
    int dim = Alg::Instance()->GetDim();

    for (auto &it : tagIdMap) {
        Tag &tag = it.second;

        size_t size = tag->m_cached_faceAttrs.size();
        float *data = new float[size * dim];
        assert(data);
        for (size_t i=0; i<size; i++) {
            memcpy(data + i*dim, tag->m_cached_faceAttrs[i]->feature().c_str(),
                    dim*sizeof(float));
        }

        tag->m_index->add(size, data);
        tag->m_cached = size;

        delete []data;
    }
}

FaceID Store::AddFace(const std::string &tagName, const FABuffer &feature, const char *url)
{
    bool tagIsNew = false;
    Tag tag;
    {
        std::unique_lock<std::mutex> lk(m_tagMapMutex);
        auto it = m_tagMap.find(tagName);
        if (it != m_tagMap.end()) {
            tag = it->second;
        } else {
            tag = m_tagMap[tagName] = Tag(new _Tag);
            tagIsNew = true;
        }
    }

    if (tagIsNew) {
        tag->m_attr.set_tagid(m_nextTagID++);

        LOG_DEBUG("add tag to db. tag: %s", tagName.c_str());
        std::string realTagName = std::string(PREFIX_TAG) + tagName;
        std::string tmp;
        tag->m_attr.SerializeToString(&tmp);
        m_db->Put(WriteOptions(), realTagName, tmp);
    }

    FaceID faceID = m_nextFaceID++;
    FaceAttr *faceAttr;

    {
        FaceAttr tmp;
        tmp.set_faceid(faceID);
        tmp.add_tagids(tag->m_attr.tagid());
        tmp.set_feature(feature.data, feature.size);

        WLock lk(&m_faceMapMutex);
        m_faceMap[faceID] = tmp;
        faceAttr = &m_faceMap[faceID];
    }

    if (url)
        faceAttr->set_url(url);

    {
        WLock lk(&tag->m_indexMutex);
        tag->m_index->add(1, (float*)faceAttr->feature().c_str());
        tag->m_cached_faceAttrs.push_back(faceAttr);
        tag->m_cached++;
    }

    std::string strFaceID = std::string(PREFIX_FID) + std::to_string((long long)faceID);
    std::string tmp;
    LOG_DEBUG("add faceID: %lld to tag: %s", (long long)faceID, tagName.c_str());
    faceAttr->SerializeToString(&tmp);
    m_db->Put(WriteOptions(), strFaceID, tmp);

    return faceID;
}

void Store::ListTags(std::vector<std::string> &tagNames)
{
    std::unique_lock<std::mutex> lk(m_tagMapMutex);
    for (auto &i : m_tagMap) {
        tagNames.push_back(i.first);
    }
}

void Store::DeleteTag(const std::string &tagName)
{
    {
        std::unique_lock<std::mutex> lk(m_tagMapMutex);
        m_tagMap.erase(tagName);
    }

    std::string realTagName(PREFIX_TAG);
    realTagName += tagName;
    m_db->Delete(WriteOptions(), realTagName);
}

size_t Store::GetTagFaceCount(const std::string &tagName)
{
    Tag tag = GetTag(tagName);
    if (!tag.get())
        return 0;

    RLock lk(&tag->m_indexMutex);
    return tag->m_cached_faceAttrs.size() - tag->m_deleted;
}

Tag Store::GetTag(const std::string &tagName)
{
    std::unique_lock<std::mutex> lk(m_tagMapMutex);
    auto it = m_tagMap.find(tagName);
    if (it != m_tagMap.end())
        return it->second;

    return Tag();
}

int Store::DeleteFace(const std::string &tagName, FaceID faceID)
{
    Tag tag = GetTag(tagName);
    if (!tag.get())
        return -1;

    FaceAttr *faceAttr = NULL;
    {
        RLock lk(&m_faceMapMutex);
        auto it = m_faceMap.find(faceID);
        if (it != m_faceMap.end())
            faceAttr = &it->second;
    }

    if (!faceAttr)
        return -1;

    // remove from m_index is O(n), for speed I just maintain a flag
    faceAttr->set_flags(DELETED);
    tag->m_deleted++;

    std::string strFaceID = std::string(PREFIX_FID) + std::to_string((long long)faceID);
    std::vector<uint32_t> tagids;
    for (int i=0; i<faceAttr->tagids_size(); i++) {
        if (faceAttr->tagids(i) != tag->m_attr.tagid()) {
            tagids.push_back(faceAttr->tagids(i));
        }
    }
    if (tagids.empty()) {
        m_db->Delete(WriteOptions(), strFaceID);
    } else {
        std::string tmp;
        faceAttr->clear_tagids();
        for (auto id: tagids)
            faceAttr->add_tagids(id);
        faceAttr->SerializeToString(&tmp);
        LOG_DEBUG("remove faceID: %lld from tag: %s", (long long)faceID, tagName.c_str());
        m_db->Put(WriteOptions(), strFaceID, tmp);
    }

    return 0;
}

int Store::Search(const char *tagName, const FABuffer &feature,
        int maxResult, SearchResults &results)
{
    Tag tag = GetTag(tagName);
    if (!tag.get())
        return -1;

    int maxCount = maxResult + tag->m_deleted;
    if ((uint64_t)maxCount > tag->m_cached)
        maxCount = tag->m_cached;

    long *ids = new long[maxCount];
    float *dis = new float[maxCount];

    long size;
    {
        RLock lk(&tag->m_indexMutex);
        tag->m_index->search((const float*)feature.data, ids, dis, maxCount, 0);
        size = tag->m_cached_faceAttrs.size();
    }

    for (int i=0; i<maxCount; i++) {
        if (ids[i] >=0 && ids[i] < size) {
            FaceAttr* attr;
            {
                RLock lk(&tag->m_indexMutex);
                attr = tag->m_cached_faceAttrs[ids[i]];
            }

            if (attr->flags() & DELETED) {
                continue;
            }

            char *url = attr->url().empty() ? NULL : strdup(attr->url().c_str());
            SearchResult r = {attr->faceid(), sqrtf(dis[i]), tagName, url};
            results.push_back(r);

            if (results.size() >= (size_t)maxResult) {
                break;
            }
        }
    }
    delete []ids;
    delete []dis;

    return 0;
}

int Store::Search(uint64_t *faceIDs, int faceIDCount, const FABuffer &feature,
        int maxResult, SearchResults &results)
{
    std::shared_ptr<Index> pindex(Index::New(CPU_NN, Alg::Instance()->GetDim()));
    std::vector<FaceAttr*> faceAttrs;
    faceAttrs.reserve(faceIDCount);

    {
        RLock lk(&m_faceMapMutex);
        for (int i=0; i<faceIDCount; i++) {
            auto it = m_faceMap.find(faceIDs[i]);
            if (it == m_faceMap.end())
                continue;
            faceAttrs.push_back(&it->second);
        }
    }

    if (faceAttrs.empty())
        return -1;

    for (auto attr : faceAttrs) {
        pindex->add(1, (float*)attr->feature().c_str());
    }

    int maxCount = (int)faceAttrs.size();
    long *ids = new long[maxCount];
    float *dis = new float[maxCount];
    pindex->search((const float*)feature.data, ids, dis, maxCount, 0);

    for (int i=0; i<maxCount; i++) {
        if (ids[i] < 0 || ids[i] >= maxCount)
            continue;

        FaceAttr* attr = faceAttrs[ids[i]];
        char *url = attr->url().empty() ? NULL : strdup(attr->url().c_str());
        SearchResult r = {attr->faceid(), sqrtf(dis[i]), NULL, url};
        results.push_back(r);

        if (results.size() >= (size_t)maxResult)
            break;
    }

    delete []ids;
    delete []dis;

    return 0;
}
