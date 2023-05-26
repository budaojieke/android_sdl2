#ifndef ANDROID_PROJECT_COMMON_H
#define ANDROID_PROJECT_COMMON_H

#include <android/log.h>

#define TAG "SDL"

#define LOG_INFO(...)    __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOG_ERROR(...)   __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

#define LOGI(fmt, args...)  LOG_INFO("[INFO ] %s %d() " fmt "\n", __func__, __LINE__, ##args);
#define LOGE(fmt, args...)  LOG_ERROR("[ERROR] %s %d() " fmt "\n", __func__, __LINE__, ##args);

#define IMAGE_W 1920
#define IMAGE_H 1080
#define MAX_TEST_CASE 5

#define SUCCEED(rc)        ((rc) == NO_ERROR)
#define FAILED(rc)         (!SUCCEED(rc))
#define ISNULL(p)          ((p) == NULL)
#define NOTNULL(ptr)       (!ISNULL(ptr))

#define SECURE_FREE(ptr) \
    do { \
        if (!ISNULL(ptr)) { \
            free(ptr); \
            (ptr) = NULL; \
        } \
    } while(0)


enum err_raeson {
    NO_ERROR = 0,
    NOT_INITED,
    PARAM_INVALID,
    PERM_DENIED,
    NOT_READY,
    FORMAT_INVALID,
    NO_MEMORY,
    INSUFF_SIZE,
    TIMEDOUT,
    NOT_FOUND,
    NOT_EXIST,
    TOO_MANY,
    ALREADY_INITED,
    ALREADY_EXISTS,
    NOT_REQUIRED,
    NOT_SUPPORTED,
    USER_ABORTED,
    JUMP_DONE,
    INTERNAL_ERROR,
    EXTERNAL_ERROR,
    BAD_PROTOCOL,
    NOT_NEEDED,
    NOT_IMPLEMENTED,
    SYS_ERROR,
    NOT_MATCH,
    BAD_TIMING,
    SERVER_ERROR,
    CLIENT_ERROR,
    CONNECTION_LOST,
    TEST_FAILED,
    FLUSHED,
    WATCHDOG_BITE,
    UNKNOWN_ERROR,
};

struct bufInfo {
    int32_t w;
    int32_t h;
    int32_t format;
    size_t  size;
    int32_t percentage;
};

enum MsgCmd {
    START,
    FRAME_IN,
    FRAME_OUT,
    PROCESS,
    PROCESS_FINISHED,
    ACK,
    END,
};

struct imgInfo {
    bool ready;
    bufInfo info;
    int32_t location;
    uint8_t *img[2]; //in, out img
};

enum Format {
    FORMAT_YVU_SEMI_PLANAR, //NV21
    FORMAT_YUV_SEMI_PLANAR, //NV12
    FORMAT_YUV_PLANAR,
    FORMAT_YUV_MONO,
    FORMAT_JPEG,
    FORMAT_HEIF,
    FORMAT_BAYER,
    FORMAT_TEXTURE,
    FORMAT_YUV_NV12P010,
    FORMAT_RGB,
    FORMAT_HLS,
    FORMAT_MAX_INVALID,
};

#endif
