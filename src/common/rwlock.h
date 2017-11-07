/* ===================================================
 * Copyright (C) 2017 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: rwlock.h
 *     Created: 2017-07-19 16:27
 * Description:
 * ===================================================
 */
#ifndef _RWLOCK_H
#define _RWLOCK_H

#include <pthread.h>

class RWMutex {
public:
    RWMutex() {
        pthread_rwlock_init(&m_lock, NULL);
    }
    void rlock() {
        pthread_rwlock_rdlock(&m_lock);
    }
    void wlock() {
        pthread_rwlock_wrlock(&m_lock);
    }
    void unlock() {
        pthread_rwlock_unlock(&m_lock);
    }

private:
    pthread_rwlock_t m_lock;
};

class RLock {
public:
    RLock(RWMutex *mutex): m_mutex(mutex) {
        m_mutex->rlock();
    }
    ~RLock() {
        m_mutex->unlock();
    }
private:
    RWMutex *m_mutex;
};

class WLock {
public:
    WLock(RWMutex *mutex): m_mutex(mutex) {
        m_mutex->wlock();
    }
    ~WLock() {
        m_mutex->unlock();
    }
private:
    RWMutex *m_mutex;
};

#endif
