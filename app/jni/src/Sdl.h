#ifndef ANDROID_PROJECT_SDL_H
#define ANDROID_PROJECT_SDL_H

#include <semaphore.h>

#include <SDL.h>
#include <SDL_ttf.h>

#include "Common.h"

class Sdl {

public:
    int32_t Init();
    int32_t process();
    Sdl();
    ~Sdl();

private:
    int32_t socketInit();
    int32_t release();
    static int32_t createWindow();
    static void calcRect(int32_t imageW,    int32_t imageH, SDL_Rect &rect, int32_t location);
    static int32_t recvBufAndImg(int acceptfd, int32_t imgIndex);
    static void threadSocket(int acceptfd);
    static void threadSdl();
    static int32_t sendMsgCmd(int acceptfd, int cmd);
    static int32_t updateTextureAndRenderCopy(imgInfo &info);
    static int32_t displayProgress(imgInfo &info);

private:
    int32_t mId;
    static SDL_Window   *mWin;
    static SDL_Renderer *mRender;
    static SDL_Texture  *mTexture;
    static _TTF_Font    *mFont;
    static SDL_DisplayMode mMode;
    static bool          mDisplayRect[MAX_TEST_CASE]; //screen max test

    bool          mWinCreated;
    uint8_t      *mImageBuf;
    int32_t       mTotalFrame;
    int32_t       mPercentage;
    int32_t       mFrameNum;
    int32_t       mSockfd;

    static sem_t mSocket2Sdl;
    static sem_t mSdl2Socket;
};


#endif
