/* =====================================================================
 * Copyright (C) 2017 chenshangping All Right Reserved.
 *    Filename: log.h
 * Description:
 *     Created: Thu 23 Feb 2017 05:21:40 PM CST
 *      Author: chenshuangping
 * =====================================================================
 */
#ifndef _LOG_H
#define _LOG_H

#include <stdio.h>

#define PRINT_RED         "\033[40;31m"
#define PRINT_GREEN       "\033[40;32m"
#define PRINT_YELLOW      "\033[40;33m"
#define PRINT_BLUE        "\033[40;34m"
#define PRINT_END         "\033[0m\n"

#define LOG_INFO(fmt, ...) do { \
    printf (PRINT_GREEN "LOG_INFO [%s:%s:%d] " fmt PRINT_END, __FILE__, __func__, __LINE__, ## __VA_ARGS__); \
} while (0)

#define LOG_WARN(fmt, ...) do { \
    printf (PRINT_YELLOW "LOG_WARN [%s:%s:%d] " fmt PRINT_END, __FILE__, __func__, __LINE__, ## __VA_ARGS__); \
} while (0)

#define LOG_ERROR(fmt, ...) do { \
    printf (PRINT_RED "LOG_ERROR [%s:%s:%d] " fmt PRINT_END, __FILE__, __func__, __LINE__, ## __VA_ARGS__); \
} while (0)

#ifdef DEBUG
#define LOG_DEBUG(fmt, ...) do { \
    printf (PRINT_BLUE "LOG_DEBUG [%s:%s:%d] " fmt PRINT_END, __FILE__, __func__, __LINE__, ## __VA_ARGS__); \
} while (0)
#else
#define LOG_DEBUG(fmt, ...)
#endif

#endif
