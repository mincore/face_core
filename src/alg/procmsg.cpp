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
#include "alg.h"
#include "store/store.h"
#include "msg/msg.h"

static bool Init()
{
    return Alg::Instance()->Init();
}

static bool Exit()
{
    return Alg::Instance()->Release();
}

MODULE(ALG, Init, Exit);
