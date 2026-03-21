#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

typedef enum {
    ERR_SUCCESS = 0,
    ERR_FAIL = 0x80000001,
    ERR_PARAM_INVALID = 0x80000002,
    ERR_PARAM_IS_NULL = 0x80000003,
    ERR_ALREADY_INITED = 0x80000004,
    ERR_NEED_INIT = 0x80000005,
    ERR_OPEN_FILE = 0x80000006,
    ERR_MEM_ALLOC_FAIL = 0x80000007,
} ErrCode;

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

#define RETURN_IF_ERROR(err)                                                                                           \
    if (err != ERR_SUCCESS) {                                                                                          \
        printf("%s %d ErrCode: 0x%x\n", FILE_ID, __LINE__, err);                                                       \
        return err;                                                                                                    \
    }

#define RETURN_IF_ERROR_(err)                                                                                          \
    if (err != ERR_SUCCESS) {                                                                                          \
        return err;                                                                                                    \
    }

#define RETURN_IF_COND(cond, err)                                                                                      \
    if (cond) {                                                                                                        \
        printf("%s %d ErrCode: 0x%x\n", FILE_ID, __LINE__, err);                                                       \
        return err;                                                                                                    \
    }

#define RETURN_IF_COND_(cond, err)                                                                                     \
    if (cond) {                                                                                                        \
        return err;                                                                                                    \
    }

#define TO_EXIT_IF_ERROR(err)                                                                                          \
    if (err != ERR_SUCCESS) {                                                                                          \
        printf("%s %d ErrCode: 0x%x\n", FILE_ID, __LINE__, err);                                                       \
        goto EXIT;                                                                                                     \
    }

#define TO_EXIT_IF_ERROR_(err)                                                                                         \
    if (err != ERR_SUCCESS) {                                                                                          \
        goto EXIT;                                                                                                     \
    }

#define TO_EXIT_IF_COND(cond, err_code)                                                                                \
    if (cond) {                                                                                                        \
        printf("%s %d ErrCode: 0x%x\n", FILE_ID, __LINE__, err_code);                                                  \
        err = err_code;                                                                                                \
        goto EXIT;                                                                                                     \
    }

#define TO_EXIT_IF_COND_(cond, err_code)                                                                               \
    if (cond) {                                                                                                        \
        err = err_code;                                                                                                \
        goto EXIT;                                                                                                     \
    }
