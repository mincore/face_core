/* ===================================================
 * Copyright (C) 2017 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: tracker.cpp
 *     Created: 2017-07-05 15:37
 * Description:
 * ===================================================
 */
#include "tracker.h"

void Tracker::Track(DetectResults &results, float threshold) {
    std::vector<FaceInfo> infos;
    for (auto &r : results.data) {
        FaceInfo info;
        info.bbox.x1 = r.rect.left;
        info.bbox.y1 = r.rect.top;
        info.bbox.x2 = r.rect.right;
        info.bbox.y2 = r.rect.bottom;

        infos.push_back(info);
    }
    std::vector<long long> trackIDs;
    NYTrack(tracker_, infos, threshold/100.0, trackIDs);
    for (size_t i=0;i <results.data.size(); i++) {
        results.data[i].trackID = trackIDs[i];
    }
}
