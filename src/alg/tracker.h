/* ===================================================
 * Copyright (C) 2017 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: tracker.h
 *     Created: 2017-07-05 15:26
 * Description:
 * ===================================================
 */
#ifndef _TRACKER_H
#define _TRACKER_H

#include "alg.h"
#include "a/nyalg.h"

class Tracker {
public:
    Tracker(): tracker_(8) {}
    void Track(DetectResults &results, float threshold);

private:
    NYTracker tracker_;
};

#endif
