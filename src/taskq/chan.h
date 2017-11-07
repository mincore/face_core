/* =====================================================================
 * Copyright (C) 2017 chenshangping All Right Reserved.
 *      Author: chenshuangping(mincore@163.com)
 *    Filename: chan.h
 * Description:
 * =====================================================================
 */
#ifndef _CHAN_H
#define _CHAN_H

#include <vector>
#include <thread>
#include <condition_variable>

template<class T>
class Ring {
public:
    Ring(int cap): m_cap(cap), m_count(0), m_next(0) {
        if (m_cap < 1) m_cap = 1;
        m_data = new T[m_cap];
    }
    ~Ring() {
        delete []m_data;
    }

    int Cap() {
        return m_cap;
    }

    int Count() {
        return m_count;
    }

    bool Empty() {
        return m_count == 0;
    }

    bool Full() {
        return m_count == m_cap;
    }

    bool Push(const T &t) {
        if (Full()) return false;
        m_data[m_next] = t;
        m_next = (m_next + 1) % m_cap;
        m_count++;
        return true;
    }

    bool Pop(T &t) {
        if (Empty()) return false;
        int index = (m_next - m_count + m_cap) % m_cap;
        m_count--;
        t = m_data[index];
        return true;
    }

private:
    T *m_data;
    int m_cap;
    int m_count;
    int m_next;
};

template<class T>
class _Chan {
public:
    _Chan(int cap = 1): m_ring(cap) {}

    T Read() {
        T t;
        std::unique_lock<std::mutex> lk(m_mutex);
        m_rcond.wait(lk, [this]{ return !m_ring.Empty(); });
        m_ring.Pop(t);
        m_wcond.notify_one();
        return t;
    }

    void Write(const T &t) {
        std::unique_lock<std::mutex> lk(m_mutex);
        m_wcond.wait(lk, [this]{ return !m_ring.Full(); });
        m_ring.Push(t);
        m_rcond.notify_one();
    }

private:
    _Chan(const _Chan &chan) = delete;
    const _Chan& operator=(const _Chan&) = delete;

private:
    Ring<T> m_ring;
    std::mutex m_mutex;
    std::condition_variable m_rcond;
    std::condition_variable m_wcond;
};

template<class T>
class Chan {
public:
    Chan(int cap = 1): m_chan(new _Chan<T>(cap)) {}

    T Read() {
        return m_chan->Read();
    }

    void ReadN(int n, std::vector<T> *v = NULL) {
        while (n--) {
            T t = Read();
            if (v)
                v->push_back(t);
        }
    }

    void Write(const T &t) {
        m_chan->Write(t);
    }

private:
    std::shared_ptr<_Chan<T> > m_chan;
};

#endif
