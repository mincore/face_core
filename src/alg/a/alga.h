/* =====================================================================
 * Copyright (C) 2017 chenshangping All Right Reserved.
 *      Author: chenshuangping(mincore@163.com)
 *    Filename: alga.h
 * Description:
 * =====================================================================
 */
#ifndef _ALGA_H
#define _ALGA_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <future>
#include "../alg.h"
#include "nyalg.h"

namespace AlgA {

void Init();

class Detector {
public:
    Detector(int gpu);
    bool Detect(const FADetectParam &param, DetectResults &result);
private:
    std::shared_ptr<NYDetector> detector_;
};

class Extractor {
public:
    struct Item {
        const Img img;
        const FABuffer faceInfo;
        std::promise<FABuffer> result;
        long long time;
    };

    Extractor(int gpu);
    bool Extract(std::vector<Item*> &items);
    int GetDim();
private:
    std::shared_ptr<NYExtractor> extractor_;
};

};

#endif
