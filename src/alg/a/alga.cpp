/* =====================================================================
 * Copyright (C) 2017 chenshangping All Right Reserved.
 *      Author: chenshuangping(mincore@163.com)
 *    Filename: alga.cpp
 * Description:
 * =====================================================================
 */
#include <atomic>
#include <cstdlib>
#include <opencv/cv.h>
#include <mutex>
#include <time.h>
#include "alga.h"

#define MAX_MAT_WIDTH 960

using namespace cv;

static NYConfig gCfg;
static int gFeatureDim = 0;

static char* GetEnv(const char *key)
{
    char *val = std::getenv(key);
    if (val && std::string(val).empty())
        return NULL;
    return val;
}

static void LoadConfig(NYConfig *cfg)
{
    char *str = GetEnv("ALG_FEATURE_DIM");
    if (str) {
        gFeatureDim = std::atoi(str);
    }

    cfg->model = GetEnv("ALG_MODEL");
    cfg->featureModel = GetEnv("ALG_FEATUREMODEL");
    cfg->featureTrained = GetEnv("ALG_FEATURETRAINED");
    cfg->qualityModel = GetEnv("ALG_QUALITYMODEL");
    cfg->qualityTrained = GetEnv("ALG_QUALITYTRAINED");
    cfg->propertyModel = GetEnv("ALG_PROPERTYMODEL");
    cfg->propertyTrained = GetEnv("ALG_PROPERTYTRAINED");

    LOG_INFO("model: %s", cfg->model);
    LOG_INFO("featureModel: %s", cfg->featureModel);
    LOG_INFO("featureTrained: %s", cfg->featureTrained);
    LOG_INFO("qualityModel: %s", cfg->qualityModel);
    LOG_INFO("qualityTrained: %s", cfg->qualityTrained);
    LOG_INFO("propertyModel: %s", cfg->propertyModel);
    LOG_INFO("propertyTrained: %s", cfg->propertyTrained);

    assert(cfg->model);
    assert(cfg->featureModel);
    assert(cfg->featureTrained);
    assert(cfg->qualityModel);
    assert(cfg->qualityTrained);
    assert(cfg->propertyModel);
    assert(cfg->propertyTrained);
};

namespace AlgA {

void Init()
{
    LoadConfig(&gCfg);
    NYInit(&gCfg);
}

static inline FARect ToFARect(const FaceRect &faceRect, int width, int height, float ratio = 0) {
    FaceRect tmp = faceRect;

    if (ratio > 0) {
        tmp.x1 /= ratio;
        tmp.y1 /= ratio;
        tmp.x2 /= ratio;
        tmp.y2 /= ratio;
    }

    if (tmp.x1 < 0) tmp.x1 = 0;
    if (tmp.y1 < 0) tmp.y1 = 0;
    if (tmp.x2 >= width) tmp.x2 = width - 1;
    if (tmp.y2 >= height) tmp.y2 = height - 1;

    return  {
        (int)tmp.x1,
        (int)tmp.y1,
        (int)tmp.x2,
        (int)tmp.y2,
    };
}

static inline void FaceInfoScale(FaceInfo &faceInfo, float ratio)
{
    if (ratio > 0) {
        for (size_t i=0; i<ARRAY_SIZE(faceInfo.facePts.x); i++) {
            faceInfo.facePts.x[i] /= ratio;
            faceInfo.facePts.y[i] /= ratio;
        }
    }
}

static Mat ImgToMat(const Img &src)
{
    if (!src.isRaw) {
        Mat rawData = Mat(1, src.buf.size, CV_8UC1, src.buf.data);
        return imdecode(rawData, CV_LOAD_IMAGE_COLOR);
    }

    return Mat(src.height, src.width, CV_8UC3, src.buf.data);
}

static void dump_pics(const cv::Mat &mat, DetectResults &result)
{
    char *path = GetEnv("CAPTURE_PATH");
    if (!path || result.data.empty()) {
        return;
    }

    char file[256];
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    uint64_t index = result.detectID;

    for (size_t i=0; i<result.data.size(); i++) {
        FARect &tmp = result.data[i].rect;
        cv::Rect rc(tmp.left, tmp.top, tmp.right - tmp.left, tmp.bottom - tmp.top);
        cv::Mat m(mat, rc);
        snprintf(file, sizeof(file), "%s/%02d%02d%02d_%lld_%zd.jpg", path,
                tm.tm_hour, tm.tm_min, tm.tm_sec,
                (long long)index, i);
        cv::imwrite(file, m);
    }

    snprintf(file, sizeof(file), "%s/%02d%02d%02d_%lld.jpg", path,
            tm.tm_hour, tm.tm_min, tm.tm_sec,
            (long long)index);
    cv::imwrite(file, mat);
}

Detector::Detector(int gpu)
{
    detector_.reset(new NYDetector(gpu));
}

bool Detector::Detect(const FADetectParam &param, DetectResults &result)
{
    std::vector<FaceInfo> infos;

#ifdef COST_PROFILING
    auto t1 = now();
#endif
    Mat src = ImgToMat(param.img);
#ifdef COST_PROFILING
    printf("---step0, img to mat use %lldms\n", now() - t1);
#endif

    Mat dst;
    Mat *mat = &src;
    int width = src.size().width;
    int height = src.size().height;

    float ratio = 0;

    if (width > MAX_MAT_WIDTH)
        ratio = 0.5;

#ifdef COST_PROFILING
    t1 = now();
#endif
    if (ratio > 0) {
        cv::Size size((int)(width * ratio), (int)(height * ratio));
        cv::resize(src, dst, size);
        mat = &dst;
    }
#ifdef COST_PROFILING
    printf("---step1, scale use %lldms\n", now() - t1);
#endif

#ifdef COST_PROFILING
    t1 = now();
#endif
    NYDetect(*detector_, *mat, param.miniSize, param.detectThreshold, param.getProperty, infos);
#ifdef COST_PROFILING
    printf("---step2, NYDetect use %lldms\n", now() - t1);
#endif
    if (infos.empty()) {
        LOG_DEBUG("no face detect");
        return false;
    }

    if (param.maxFace > 0 && infos.size() > (size_t)param.maxFace)
        infos.resize(param.maxFace);

    result.data.resize(infos.size());

    for (size_t i=0; i<infos.size(); i++) {
        auto info = infos[i];
        DetectResult &tmp = result.data[i];

        tmp.confidence = info.bbox.score;
        tmp.rect = ToFARect(info.bbox, width, height, ratio);
        FaceInfoScale(info, ratio);

        tmp.faceInfo = FABufferAlloc(sizeof(info.facePts));
        memcpy(tmp.faceInfo.data, &info.facePts, sizeof(info.facePts));

        tmp.quality = info.quality;
        tmp.age = info.age;
        tmp.yaw = info.yaw;
        tmp.pitch = info.pitch;
        tmp.sex = info.sex;
    }

    dump_pics(src, result);

    return true;
}

Extractor::Extractor(int gpu)
{
    extractor_.reset(new NYExtractor(gpu));
}

bool Extractor::Extract(std::vector<Item*> &items)
{
    std::vector<cv::Mat> mats;
    std::vector<FaceInfo> faceInfos;
    std::vector<std::vector<float> > features;

    for (auto p: items) {
        Mat mat = ImgToMat(p->img);
        mats.push_back(mat);

        FacePts *pts = (FacePts*)p->faceInfo.data;
        FaceInfo faceInfo;
        memcpy(&faceInfo.facePts, pts, sizeof(FacePts));
        faceInfos.push_back(faceInfo);
    }

    NYExtract(*extractor_, mats, faceInfos, features);

    for (size_t i=0; i<items.size(); i++) {
        FABuffer tmp = FABufferAlloc(features[i].size() * sizeof(float));
        memcpy(tmp.data, &features[i][0], features[i].size() * sizeof(float));
        items[i]->result.set_value(tmp);
    }

    return true;
}

int Extractor::GetDim()
{
    return gFeatureDim ? gFeatureDim : NYExtractDim(*extractor_);
}

}
