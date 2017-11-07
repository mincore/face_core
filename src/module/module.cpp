/* =====================================================================
 * Copyright (C) 2017 chenshangping All Right Reserved.
 *      Author: chenshuangping(mincore@163.com)
 *    Filename: module/module.cpp
 * Description:
 * =====================================================================
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "common/common.h"
#include "module.h"

extern Module *MODULE_STORE;
extern Module *MODULE_ALG;

Module* gModules[] =
{
    MODULE_ALG,
    MODULE_STORE,
};

int RunAllModules()
{
    for (size_t i=0; i<ARRAY_SIZE(gModules); i++)
    {
        Module *pModule = gModules[i];
        if (!pModule->Init())
        {
            LOG_ERROR("Init Module: %s failed", pModule->Name());
            return -1;
        }
        LOG_INFO("Module: %s Inited", pModule->Name());
    }

    return 0;
}

int ExitAllModules()
{
    for (size_t i=0; i<ARRAY_SIZE(gModules); i++)
    {
        Module *pModule = gModules[i];
        if (!pModule->Exit())
        {
            LOG_ERROR("Init Module: %s failed", pModule->Name());
            break;
        }
        LOG_INFO("Module: %s Exit", pModule->Name());
    }

    return 0;
}
