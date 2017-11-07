/* =====================================================================
 * Copyright (C) 2017 chenshangping All Right Reserved.
 *      Author: chenshuangping(mincore@163.com)
 *    Filename: module/module.h
 * Description:
 * =====================================================================
 */
#ifndef _MODULE_H
#define _MODULE_H

#include <stdlib.h>
#include "common/common.h"
#include "taskq/taskq.h"

struct Module {
public:
    virtual bool Init() = 0;
    virtual bool Exit() = 0;
    const char* Name() { return m_name; }
protected:
    const char *m_name;
};

#define MACROSTR(x) #x
#define MODULE(name, INITFUNC, EXITFUNC)\
    class Module##name: public Module {\
    public:\
        virtual bool Init() {\
            m_name = MACROSTR(MODULE_##name);\
            return ::INITFUNC();\
        }\
        virtual bool Exit() {\
            return ::EXITFUNC();\
        }\
    };\
    static Module##name gModule;\
    Module* MODULE_##name = &gModule;

int RunAllModules();
int ExitAllModules();

#endif
