/* ===================================================
 * Copyright (C) 2017 hangzhou nanyuntech All Right Reserved.
 *      Author: chenshuangping(mincore@163.com)
 *    Filename: test/test.cpp
 *     Created: 2017-06-14 13:47
 * Description:
 * ===================================================
 */
#include <opencv2/opencv.hpp>
#include <unistd.h>
#include <dirent.h>
#include <vector>
#include <thread>
#include <stdio.h>
#include <string.h>
#include "faapi.h"
#include "../src/common/common.h"

#define panic(fmt, ...) do { \
    printf ("[%s:%s:%d] " fmt, __FILE__, __func__, __LINE__, ## __VA_ARGS__); \
    abort(); \
} while (0)

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#define THRESHOLD { 0.8, 0.7, 0.7 }

#include "taskq/taskq.h"
#include "taskq/chan.h"

TaskQueue tq(10);

class Img: public FAImg {
public:
    Img(const char *file) {
        mat = cv::imread(file, CV_LOAD_IMAGE_COLOR);
        isRaw = true;
        width = mat.size().width;
        height = mat.size().height;
        buf.size = mat.total() * mat.elemSize();
        buf.data = mat.data;
    }
private:
    cv::Mat mat;
};
typedef std::vector<Img> Imgs;

static Imgs LoadImgs(int argc, char **argv)
{
    Imgs imgs;
    for (int i=0; i<argc; i++) {
        imgs.emplace_back(Img(argv[i]));
    }
    return imgs;
}

static void do_detect(const FAImg *img, int channel)
{
    FADetectParam param = {*img, 60, 0, THRESHOLD, channel, 55};
    FADetectResult detectResult;

    auto n1 = now();
    if (FADetect(&param, &detectResult) < 0) {
        printf("detect failed\n");
        return;
    }
    printf("FADetect use %dms\n", (int)(now() -n1));

    for (int i=0; i<detectResult.count; i++) {
        FARect &rc = detectResult.result[i].rect;
        printf("detectID: %lld, channel: %d, trackID: %lld, quality: %f, rect %d %d %d %d\n",
                (long long)detectResult.detectID,
                channel, (long long)detectResult.result[i].trackID,
                detectResult.result[i].quality,
                rc.left, rc.top, rc.right, rc.bottom);
    }

    FADetectFree(&detectResult);
}

static void test_detect(int argc, char **argv)
{
    int channel = FACreateDetectChannel();

    for (auto &img : LoadImgs(argc, argv))
        do_detect(&img, channel);

    FADestroyDetectChannel(channel);
}

static void detect_and_extract(const FAImg *img, std::vector<ExtractResult> &results)
{
    FADetectParam param = {*img, 60, 0, THRESHOLD, 0};
    FADetectResult detectResult;

    //auto n1 = now();
    if (FADetect(&param, &detectResult) < 0) {
        printf("detect failed\n");
        return;
    }
    //printf("detect use %dms\n", (int)(now() -n1));

    Chan<int> c(detectResult.count);
    for (int i=0; i<detectResult.count; i++) {
        tq.Push([=, &detectResult, &results, &c] {
            auto t1 = now();
            ExtractResult extractResult;
            FAExtract(img, &detectResult.result[i].faceInfo, &extractResult);
            printf("extract use %dms\n", (int)(now() -t1));
            c.Write(1);
            static std::mutex mutex;
            std::unique_lock<std::mutex> lk(mutex);
            results.push_back(extractResult);
        });
    }
    c.ReadN(detectResult.count);
    //for (int i=0; i<detectResult.count; i++) {
    //    ExtractResult extractResult;
    //    n1 = now();
    //    if (FAExtract(img, &detectResult.result[i].faceInfo, &extractResult) < 0) {
    //        printf("extract failed\n");
    //        continue;
    //    }
    //    printf("extract use %dms\n", (int)(now() -n1));
    //    results.push_back(extractResult);
    //}

    FADetectFree(&detectResult);
}

static void ExtraceResultsFree(std::vector<ExtractResult> &results)
{
    for (auto &r : results)
        FAExtractFree(&r);
}

static void test_search_one(const FABuffer *feature, const char **tagNames, int tagCount)
{
    std::vector<uint64_t> ids = {0,1,2,3};
    FASearchParam param = {
        *feature,
        100,
        tagNames,
        tagCount,
        //&ids[0],
        //(int)ids.size()
        NULL,
        0
    };

    FASearchResult result;

    auto t = now();
    if (FASearch(&param, &result) < 0) {
        printf("search failed\n");
        return;
    }
    printf("search use: %lldms\n", now() - t);

    SearchResult &r = result.result[0];
    printf("faceCount: %d, top faceID: %lld score: %f tag: %s\n",
            result.count,
            (long long)r.faceID, r.score, r.tagName);

    FASearchFree(&result);
}

static void do_search(const FAImg *img, const char **tagNames, int tagCount)
{
    std::vector<ExtractResult> extractResults;
    detect_and_extract(img, extractResults);
    if (extractResults.empty()) {
        printf("extract 0 features\n");
        return;
    }

    for (auto &r : extractResults) {
        test_search_one(&r.feature, tagNames, tagCount);
    }

    ExtraceResultsFree(extractResults);
}

static void test_search(int argc, char **argv)
{
    const char *tag[] = { "g1" };

    for (auto &img: LoadImgs(argc, argv))
        do_search(&img, tag, 1);
}

static void do_extract_and_checkin(const FAImg *img, const char *tagName)
{
    std::vector<ExtractResult> extractResults;
    detect_and_extract(img, extractResults);

    for (auto &r : extractResults) {
        uint64_t faceID = 0;
        FAInsert(tagName, &r.feature, &faceID, NULL);
        //printf("checkin faceID: %lld to %s\n", (long long)faceID, tagName);
    }

    ExtraceResultsFree(extractResults);
}

static void test_checkin(int argc, char **argv)
{
    for (auto &img : LoadImgs(argc, argv))
        do_extract_and_checkin(&img, "g1");
}

static void test_compare(int argc, char **argv)
{
    if (argc < 2)
        panic("input should be two file\n");

    Imgs imgs = LoadImgs(argc, argv);

    std::vector<ExtractResult> r1;
    detect_and_extract(&imgs[0], r1);

    std::vector<ExtractResult> r2;
    detect_and_extract(&imgs[1], r2);

    float score = 0;
    FACmpFeature(&r1[0].feature, &r2[0].feature, &score);
    printf("compare score: %f\n", score);

    ExtraceResultsFree(r1);
    ExtraceResultsFree(r2);
}

static void test_video(int argc, char **argv)
{
    const char *videoFile = argv[0];
    cv::VideoCapture cap(videoFile);
    if(!cap.isOpened()) {
        printf("open %s failed\n", videoFile);
        return;
    }

    int channel = FACreateDetectChannel();

    cv::Mat mat;
    while (cap.read(mat)) {
        FAImg img = { {mat.data, 0}, mat.size().width, mat.size().height, 1 };
        do_detect(&img, channel);
    }

    FADestroyDetectChannel(channel);
}

static void test(char opt, int argc, char **argv)
{
    int C = 1;
    char *str = std::getenv("LOOP");
    if (str)
        C = std::atoi(str);

    FAInit();

    switch (opt) {
        case 'd': while (C--) test_detect(argc, argv); break;
        case 'c': while (C--) test_checkin(argc, argv); break;
        case 's': while (C--) test_search(argc, argv); break;
        case 'v': while (C--) test_video(argc, argv); break;
        case 'b': while (C--) test_compare(argc, argv); break;
    }

    FADestory();
}

int main(int argc, char *argv[])
{
    int c = getopt(argc, argv, "d:c:s:v:b:");
    if (c != -1)
        test(c, argc-optind+1, &argv[optind-1]);

    return 0;
}
