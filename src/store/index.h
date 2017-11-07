/* ===================================================
 * Copyright (C) 2017 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: src/store/index.h
 *     Created: 2017-07-22 14:05
 * Description:
 * ===================================================
 */
#ifndef _INDEX_HPP_
#define _INDEX_HPP_

enum INDEX_TYPE {
    CPU_NN,
    CPU_ANN,
    GPU_NN,
    GPU_ANN,
};

class Index {
public:
    static Index* New(INDEX_TYPE type, int dim, int nregion = 10, int device = 0);
    virtual void train(long count, const float* gallery) {}
    virtual void add(long count, const float* gallery) = 0;
    virtual void remove(long id) = 0;
    virtual void search(const float* query, long* I, float* D, int ntop, int nregion) = 0;
};

#endif
