/* ===================================================
 * Copyright (C) 2017 chenshangping All Right Reserved.
 *      Author: chenshuangping(mincore@163.com)
 *    Filename: utils.h
 *     Created: 2017-06-02 15:24
 * Description:
 * ===================================================
 */
#ifndef _UTILS_H
#define _UTILS_H

#include <stdio.h>
#include <string>
#include <sstream>
#include <vector>
#include <chrono>

class noncopyable {
protected:
    noncopyable() {}
    ~noncopyable() {}
private:
    noncopyable(const noncopyable&);
    const noncopyable& operator=(const noncopyable&);
};

template <typename T> class singleton: private noncopyable {
public:
    static T* Instance() {
       static T t;
       return &t;
    }
};

static inline std::string& strrtrim(std::string& s, const char* t)
{
    s.erase(s.find_last_not_of(t) + 1);
    return s;
}

static inline std::string& strltrim(std::string& s, const char* t)
{
    s.erase(0, s.find_first_not_of(t));
    return s;
}

static inline std::string& strtrim(std::string& s, const char* t)
{
    return strltrim(strrtrim(s, t), t);
}

static void inline strsplit(const char *s, char delim, std::vector<std::string> &elems)
{
    std::stringstream ss(s);
    std::string item;
    elems.clear();

    while (getline(ss, item, delim))
    {
        elems.push_back(strtrim(item, " \t"));
    }
}

static inline long long now()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
            ).count();
}

#endif
