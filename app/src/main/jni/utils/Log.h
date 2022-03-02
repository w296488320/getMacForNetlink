
#ifndef VMP_LOG_H
#define VMP_LOG_H


#include <android/log.h>

#define TAG "Netlink"

#ifdef CAMEL_BUILD_TYPE_NOLOG

#define LOGE(...)   ((void)0);
#define LOGI(...)   ((void)0);
#define LOGD(...)   ((void)0);
#define LOGW(...)   ((void)0);
#define ALOGI(...) ((void)0);
#define ALOGD(...) ((void)0);
#define ALOGW(...) ((void)0);
#define ALOGE(...) ((void)0);



#else


#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__);
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG ,__VA_ARGS__);
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG ,__VA_ARGS__);
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG ,__VA_ARGS__);

#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG ,__VA_ARGS__);
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG ,__VA_ARGS__);
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG ,__VA_ARGS__);
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG ,__VA_ARGS__);


#endif


//专门用于调试ptrace的代码,学习ptrace流程使用
//不想打印的话可以在cmake中去掉
#ifdef CAMEL_PTRACE_DEBUG

#define PLOGE(...)  __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__);
#define PLOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG ,__VA_ARGS__);
#define PLOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG ,__VA_ARGS__);
#define PLOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG ,__VA_ARGS__);

#else

#define PLOGE(...)  ((void)0);
#define PLOGI(...)  ((void)0);
#define PLOGD(...)  ((void)0);
#define PLOGW(...)  ((void)0);

#endif

#endif //VMP_LOG_H
