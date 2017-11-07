/* ===================================================
 * Copyright (C) 2017 chenshangping All Right Reserved.
 *      Author: chenshuangping(mincore@163.com)
 *    Filename: procmsg.cpp
 *     Created: 2017-05-31 11:49
 * Description:
 * ===================================================
 */
#include "common/common.h"
#include "module/module.h"
#include "msg/msg.h"
#include "store.h"
#include "alg/alg.h"

static bool Init()
{
    return Store::Instance()->Init();
}

static bool Exit()
{
    return Store::Instance()->Release();
}

MODULE(STORE, Init, Exit);
