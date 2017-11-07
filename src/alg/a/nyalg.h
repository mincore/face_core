/* ===================================================
 * Copyright (C) 2017 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: nyalg.h
 *     Created: 2017-06-17 16:17
 * Description:
 * ===================================================
 */
#ifndef _NYALG_H
#define _NYALG_H

#include <vector>
#include <opencv2/opencv.hpp>

#define API extern "C"

typedef struct {
  float x1;
  float y1;
  float x2;
  float y2;
  float score;
} FaceRect;

typedef struct {
  float x[5],y[5];
} FacePts;

typedef struct {
  FaceRect bbox;
  cv::Vec4f regression;
  FacePts facePts;
  float quality;
  float age;
  float yaw;
  float pitch;
  float sex;
} FaceInfo;

typedef struct {
    const char *model;
    const char *featureModel;
    const char *featureTrained;
    const char *qualityModel;
    const char *qualityTrained;
    const char *propertyModel;
    const char *propertyTrained;
} NYConfig;

API int NYInit(const NYConfig *cfg);
API int NYExit();

////////////detector/////////
API void* NYDetectNew(int device);
API void NYDetectDel(void*);
API void NYDetect(void*, const cv::Mat &mat, int miniSize, const float threshold[3], bool getProperty, std::vector<FaceInfo> &result);

////////////extractor/////////
API void* NYExtractNew(int device);
API void NYExtractDel(void*);
API void NYExtract(void*, const std::vector<cv::Mat> &mats, const std::vector<FaceInfo>& faceInfos, std::vector<std::vector<float> > &features);
API int NYExtractDim(void*);

////////////tracker/////////
API void* NYTrackNew(int maxLoss);
API void NYTrackDel(void*);
API void NYTrack(void*, const std::vector<FaceInfo> &faceInfo, float threshold, std::vector<long long> &trackIDs);


//////////////////////////
static inline void NYExtractOne(void*h, const cv::Mat &mat, const FaceInfo &info, std::vector<float> &feat) {
    std::vector<cv::Mat> mats;
    std::vector<FaceInfo> faceInfos;
    std::vector<std::vector<float> > features;

    mats.push_back(mat);
    faceInfos.push_back(info);
    NYExtract(h, mats, faceInfos, features);
    feat.swap(features[0]);
}

////////////helper/////////
template<void*(*f1)(int), void(*f2)(void*)>
class Object {
public:
    Object(int arg = 0) {
        ptr_ = f1(arg);
    }
    ~Object() {
        f2(ptr_);
    }
    operator void*() { return ptr_; }
private:
    void* ptr_;
};

typedef Object<NYDetectNew, NYDetectDel> NYDetector;
typedef Object<NYExtractNew, NYExtractDel> NYExtractor;
typedef Object<NYTrackNew, NYTrackDel> NYTracker;

#endif
