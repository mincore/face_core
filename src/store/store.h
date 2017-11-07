/* =====================================================================
 * Copyright (C) 2017 chenshangping All Right Reserved.
 *      Author: chenshuangping(mincore@163.com)
 *    Filename: store.h
 * Description:
 * =====================================================================
 */
#ifndef _STORE_H
#define _STORE_H

#include <mutex>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <list>
#include <set>
#include "../msg/pb/fastore.pb.h"
#include "../alg/alg.h"
#include "index.h"
#include "common/rwlock.h"

namespace leveldb {
    class DB;
}
using namespace fastore;

typedef std::uint64_t FaceID;
typedef std::uint32_t TagID;

struct _Tag {
    _Tag(): m_deleted(0), m_cached(0), m_index(Index::New(CPU_NN, Alg::Instance()->GetDim())) {}
    std::string m_name;
    TagAttr m_attr;

    std::atomic<uint64_t> m_deleted;
    std::atomic<uint64_t> m_cached;
    RWMutex m_indexMutex;
    std::shared_ptr<Index> m_index;
    std::vector<FaceAttr*> m_cached_faceAttrs;
};
typedef std::shared_ptr<_Tag> Tag;

class Store: public singleton<Store> {
public:
    Store(): m_db(NULL) {}
    bool Init();
    bool Release();

    void ListTags(std::vector<std::string> &tagNames);
    size_t GetTagFaceCount(const std::string &tagName);
    void DeleteTag(const std::string &tagName);

    FaceID AddFace(const std::string &tagName, const FABuffer &feature, const char *url);
    int DeleteFace(const std::string &tagName, FaceID faceID);
    int Search(const char *tagName, const FABuffer &feature,
        int maxResult, SearchResults &results);
    int Search(uint64_t *faceIDs, int faceIDCount, const FABuffer &feature,
        int maxResult, SearchResults &results);

private:
    bool LoadTags(std::unordered_map<TagID, Tag> &tagIdMap);
    bool LoadFaces(const std::unordered_map<TagID, Tag> &tagIdMap);
    void BuildIndex(std::unordered_map<TagID, Tag> &tagIdMap);
    Tag GetTag(const std::string &tagName);

private:
    leveldb::DB *m_db;
    std::atomic<FaceID> m_nextFaceID;
    std::atomic<TagID> m_nextTagID;

    std::unordered_map<FaceID, FaceAttr> m_faceMap;
    RWMutex m_faceMapMutex;

    std::unordered_map<std::string, Tag> m_tagMap;
    std::mutex m_tagMapMutex;
};

#endif
