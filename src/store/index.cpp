/* ===================================================
 * Copyright (C) 2017 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: src/store/index.cpp
 *     Created: 2017-07-22 14:05
 * Description:
 * ===================================================
 */
#include <faiss/IndexFlat.h>
#include <faiss/IndexIVF.h>
#include <faiss/gpu/StandardGpuResources.h>
#include <faiss/gpu/GpuIndexFlat.h>
#include <faiss/gpu/GpuIndexIVFFlat.h>
#include <faiss/gpu/GpuAutoTune.h>
#include <faiss/IndexFlat.h>
#include <faiss/IndexIVF.h>
#include <faiss/gpu/StandardGpuResources.h>
#include <faiss/gpu/GpuIndexFlat.h>
#include <faiss/gpu/GpuIndexIVFFlat.h>
#include <faiss/index_io.h>
#include <faiss/AuxIndexStructures.h>

#include "index.h"

class cpu_nn: public Index {
public:
    cpu_nn(int dim) {
        nn_index_ = new faiss::IndexFlat(dim, faiss::METRIC_L2);
    }
    ~cpu_nn() {
        delete nn_index_;
    }

    virtual void add(long count, const float* gallery) {
        nn_index_->add(count, gallery);
    }

    virtual void remove(long id) {
        faiss::Index::idx_t ids = id;
        faiss::IDSelectorBatch idBatch = faiss::IDSelectorBatch(1, &ids);
        nn_index_->remove_ids(idBatch);
    }

    virtual void search(const float* query, long* I, float* D, int ntop, int nregion) {
        (void)nregion;
        nn_index_->search(1, query, ntop, D, I);
    }

private:
    faiss::IndexFlat *nn_index_;
};

class cpu_ann: public Index {
public:
    cpu_ann(int dim, int nregion) {
        ann_quantizer_ = new faiss::IndexFlat(dim, faiss::METRIC_L2);
        ann_index_ = new faiss::IndexIVFFlat(ann_quantizer_, dim, nregion, faiss::METRIC_L2);
        ann_index_->own_fields = true;
    }
    ~cpu_ann() {
        delete ann_index_;
        delete ann_quantizer_;
    }

    virtual void train(long count, const float* gallery) {
        ann_index_->train(count, gallery);
    }

    virtual void add(long count, const float* gallery) {
        ann_index_->add(count, gallery);
    }

    virtual void remove(long id) {
        faiss::Index::idx_t ids = id;
        faiss::IDSelectorBatch idBatch = faiss::IDSelectorBatch(1, &ids);
        ann_index_->remove_ids(idBatch);
    }

    virtual void search(const float* query, long* I, float* D, int ntop, int nregion) {
        ann_index_->nprobe = nregion;
        ann_index_->search(1, query, ntop, D, I);
    }

private:
    faiss::IndexFlat* ann_quantizer_;
    faiss::IndexIVFFlat* ann_index_;
};

class gpu_nn: public Index {
public:
    gpu_nn(int dim, int device) {
        nn_res_ = new faiss::gpu::StandardGpuResources;
        config_.device = device;
        nn_index_gpu_ = new faiss::gpu::GpuIndexFlat(nn_res_, dim, faiss::METRIC_L2, config_);
    }
    ~gpu_nn() {
        delete nn_index_gpu_;
        delete nn_res_;
    }

    virtual void add(long count, const float* gallery) {
        nn_index_gpu_->add(count, gallery);
    }

    virtual void remove(long id) {
        printf("remove id from gpu_index is very inefficient. abort\n");
        abort();
        //faiss::Index::idx_t ids = id;
        //faiss::IDSelectorBatch idBatch = faiss::IDSelectorBatch(1, &ids);
        //auto cpu_index = faiss::gpu::index_gpu_to_cpu(nn_index_gpu_);
        //cpu_index->remove_ids(idBatch);
        //delete nn_index_gpu_;
        //nn_index_gpu_ = (faiss::gpu::GpuIndexFlat*)faiss::gpu::index_cpu_to_gpu(nn_res_, config_.device, cpu_index, NULL);
        //delete cpu_index;
    }

    virtual void search(const float* query, long* I, float* D, int ntop, int nregion) {
        (void)nregion;
        nn_index_gpu_->search(1, query, ntop, D, I);
    }

private:
    faiss::gpu::StandardGpuResources* nn_res_;
    faiss::gpu::GpuIndexFlat* nn_index_gpu_;
    faiss::gpu::GpuIndexFlatConfig config_;
};

class gpu_ann: public Index {
public:
    gpu_ann(int dim, int nregion, int device) {
        ann_res_ = new faiss::gpu::StandardGpuResources;
        config_.device = device;
        ann_index_gpu_ = new faiss::gpu::GpuIndexIVFFlat(ann_res_, dim, nregion, faiss::METRIC_L2, config_);
    }
    ~gpu_ann() {
        delete ann_index_gpu_;
        delete ann_res_;
    }

    virtual void train(long count, const float* gallery) {
        ann_index_gpu_->train(count, gallery);
    }

    virtual void add(long count, const float* gallery) {
        ann_index_gpu_->add(count, gallery);
    }

    virtual void remove(long id) {
        printf("remove id from gpu_index is very inefficient. abort\n");
        abort();
    }

    virtual void search(const float* query, long* I, float* D, int ntop, int nregion) {
        ann_index_gpu_->setNumProbes(nregion);
        ann_index_gpu_->search(1, query, ntop, D, I);
    }

private:
    faiss::gpu::StandardGpuResources* ann_res_;
    faiss::gpu::GpuIndexIVFFlat* ann_index_gpu_;
    faiss::gpu::GpuIndexIVFFlatConfig config_;
};

Index* Index::New(INDEX_TYPE type, int dim, int nregion, int device)
{
    switch (type) {
    default: return new cpu_nn(dim);
    case CPU_ANN: return new cpu_ann(dim, nregion);
    case GPU_NN: return new gpu_nn(dim, device);
    case GPU_ANN: return new gpu_ann(dim, nregion, device);
    }
}
