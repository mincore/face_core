/* ===================================================
 * Copyright (C) 2017 chenshangping All Right Reserved.
 *      Author: chenshuangping(mincore@163.com)
 *    Filename: src/common/mempool.h
 *     Created: 2017-06-03 10:07
 * Description:
 * ===================================================
 */
#ifndef _MEMPOOL_H
#define _MEMPOOL_H

#include <mutex>
#include "utils.h"

class Ring {
public:
    Ring(int cap):m_num(0), m_tail(0) {
        m_slot = new void*[cap];
        m_cap = cap;
    }
    ~Ring() {
        if (NULL != m_slot)
            delete[] m_slot;
    }

    void* Push(void *ptr) {
        if (NULL == ptr || NULL == m_slot || m_num == m_cap)
            return NULL;

        m_slot[m_tail] = ptr;
        m_tail = (m_tail + 1) % m_cap;
        m_num++;

        return ptr;
    }

    void* Pop() {
        unsigned pop;

        if (NULL == m_slot || m_num == 0)
            return NULL;

        pop = (m_tail - m_num + m_cap) % m_cap;
        m_num--;

        return m_slot[pop];
    }

private:
    void **m_slot;
    unsigned m_cap;
    unsigned m_num;
    unsigned m_tail;
};

template<unsigned itemSize, unsigned maxItemCount>
class FixedMemPool: public singleton<FixedMemPool<itemSize, maxItemCount> > {
public:
    FixedMemPool(): m_ring(maxItemCount) {}
    ~FixedMemPool() {}

    void* Alloc() {
        void *ret;
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            ret = m_ring.Pop();
        }

        if (ret == NULL)
            ret = new char[itemSize];

        return ret;
    }

    void Free(void *ptr) {
        if (NULL == ptr)
            return;

        void *ret;
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            ret = m_ring.Push(ptr);
        }

        if (NULL == ret)
            delete[] (char*)ptr;
    }

private:
    std::mutex m_mutex;
    Ring m_ring;
};

#endif
