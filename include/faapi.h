/* ===================================================
 * Copyright (C) 2017 hangzhou nanyuntech All Right Reserved.
 *      Author: chenshuangping(mincore@163.com)
 *    Filename: faapi.h
 *     Created: 2017-06-14 10:51
 * Description:
 * ===================================================
 */
#ifndef _FAAPI_H
#define _FAAPI_H

#include <stdint.h>

#ifdef __cplusplus
#define FAAPI extern "C"
#else
#define FAAPI
#endif

typedef struct {
    int left;
    int top;
    int right;
    int bottom;
} FARect;

typedef struct {
    void *data;
    int size;
    int ref;
} FABuffer;

typedef struct {
    FABuffer buf;
    int width;
    int height;
    int isRaw; /* if true, it's a raw img(bgr, yuv...), buf.data, width and height must be provided.
                * otherwise, it's a encoded img(jpg, png...), buf.data, buf.size must be provided.
                */
    int channel;
} FAImg;

typedef struct {
    float confidence;
    FARect rect;
    FABuffer faceInfo;
    uint64_t trackID;
    float quality;
    float age;
    float yaw;
    float pitch;
    float sex;
} DetectResult;

typedef struct {
    FARect rect;
    FABuffer feature;
} ExtractResult;

typedef struct {
    FAImg img;
    int miniSize;
    int maxFace;
    float detectThreshold[3];

    /* for track only */
    int channel;
    float trackThreshold;

    int getProperty;
} FADetectParam;

typedef struct {
    DetectResult *result;
    int count;
    uint64_t detectID;
} FADetectResult;

typedef struct {
    FABuffer feature;
    int maxFaceCount;
    const char **tagNames;
    int tagCount;
    uint64_t *faceIDs;
    int faceIDCount;
} FASearchParam;

typedef struct {
    uint64_t faceID;
    float score;
    const char *tagName;
    char *url;
} SearchResult;

typedef struct {
    SearchResult *result;
    int count;
} FASearchResult;

// All api return 0:ok, <0:fail
FAAPI int FAInit();
FAAPI int FADestory();

// if < 0, fail
FAAPI int FACreateDetectChannel();
FAAPI int FADestroyDetectChannel(int channel);

// note: must call FADetectFree
FAAPI int FADetect(const FADetectParam *param, FADetectResult *result);
FAAPI int FADetectFree(FADetectResult *result);

// note: must call FAExtractFree
FAAPI int FAExtract(const FAImg *img, const FABuffer *faceInfo, ExtractResult *result);
FAAPI int FAExtractFree(ExtractResult *result);

// note: must call FASearchFree
FAAPI int FASearch(const FASearchParam *param, FASearchResult *result);
FAAPI int FASearchFree(FASearchResult *result);

// compare two feature
FAAPI int FACmpFeature(const FABuffer *f1, const FABuffer *f2, float *score);


/////////////face db api: will be splited out later///////////
typedef struct {
    char **names;
    int count;
} TagNames;

// note: must call FAFreeTagNames
FAAPI int FAGetTagNames(TagNames *tagNames);
FAAPI int FAFreeTagNames(TagNames *tagNames);

// delete tag and faces in it
FAAPI int FADetelTag(const char *tagName);

// insert a feature into a tag
FAAPI int FAInsert(const char *tagName, const FABuffer *feature, uint64_t *faceID, const char *url);

// delete a feature from a tag
FAAPI int FADelete(const char *tagName, uint64_t faceID);

FAAPI int FAGetTagFeatureCount(const char *tagName);

#endif
