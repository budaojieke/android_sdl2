LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main
$(info "__ANDROID_API__ $(__ANDROID_API__)")

SDL_PATH := ../SDL
SDL_ttf_PATH := ../SDL2_ttf

LOCAL_C_INCLUDES := . $(LOCAL_PATH)/$(SDL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/$(SDL_ttf_PATH)/include

# Add your application source files here...
LOCAL_SRC_FILES := main.cpp Sdl.cpp

LOCAL_SHARED_LIBRARIES := SDL2 SDL2_ttf

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -lOpenSLES -llog -landroid

include $(BUILD_SHARED_LIBRARY)
