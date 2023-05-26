#include <error.h>
#include <string>
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <map>
#include <algorithm>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>

#include "Sdl.h"

#define FONT_PATH   "/system/fonts/ZUKChinese.ttf"
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static const char *const gCmdName[] = {
    [START]             = "START",
    [FRAME_IN]          = "FRAME_IN",
    [FRAME_OUT]         = "FRAME_OUT",
    [PROCESS]           = "PROCESS",
    [PROCESS_FINISHED]  = "PROCESS_FINISHED",
    [ACK]               = "ACK",
    [END]               = "END",
};

static const char * const gFormatStr[] = {
    [FORMAT_YVU_SEMI_PLANAR] = "nv21sp",
    [FORMAT_YUV_SEMI_PLANAR] = "nv12sp",
    [FORMAT_YUV_PLANAR]      = "yuv",
    [FORMAT_YUV_MONO]        = "mono",
    [FORMAT_JPEG]            = "jpg",
    [FORMAT_HEIF]            = "heif",
    [FORMAT_BAYER]           = "bayer",
    [FORMAT_TEXTURE]         = "texture",
    [FORMAT_YUV_NV12P010]    = "P010",
    [FORMAT_RGB]             = "rgb",
    [FORMAT_HLS]             = "hls",
    [FORMAT_MAX_INVALID]     = "FORMAT_MAX_INVALID",
};

std::map<int, imgInfo> gMap;
std::mutex gMtx;

bool          Sdl::mQuit = false;
SDL_Window   *Sdl::mWin = nullptr;
SDL_Renderer *Sdl::mRender = nullptr;
_TTF_Font    *Sdl::mFont = nullptr;
SDL_DisplayMode Sdl::mMode;
bool          Sdl::mDisplayRect[MAX_TEST_CASE];

sem_t         Sdl::mSocket2Sdl;
sem_t         Sdl::mSdl2Socket;

int32_t Sdl::socketInit()
{
    int32_t rc = NO_ERROR;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    if (SUCCEED(rc)) {
        if((mSockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            LOGE("fail to socket %s", strerror(errno));
            rc = SYS_ERROR;
        }
    }

    if (SUCCEED(rc)) {
        int iSockOptVal = 1;
        if (setsockopt(mSockfd, SOL_SOCKET, SO_REUSEADDR, &iSockOptVal, sizeof(iSockOptVal)) < 0) {
            LOGE("fail to setsockopt %s", strerror(errno));
            rc = SYS_ERROR;
        }
    }

    if (SUCCEED(rc)) {
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        server_addr.sin_port = htons(8888);

        if(bind(mSockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            LOGE("fail to bind %s", strerror(errno));
            rc = SYS_ERROR;
        }
    }

    if (SUCCEED(rc)) {
        if(listen(mSockfd, 5) < 0) {
            LOGE("fail to listen %s", strerror(errno));
            rc = SYS_ERROR;
        }
    }

    return rc;
}

int32_t Sdl::Init()
{
    int32_t rc = NO_ERROR;
    rc = socketInit();

    if (SUCCEED(rc)) {
        if (SDL_Init(SDL_INIT_VIDEO)) {
            LOGE("Could not initialize SDL - %s", SDL_GetError());
            rc = EXTERNAL_ERROR;
        }
    }

    if (SUCCEED(rc)) {
        if (TTF_Init()) {
            LOGE("Could not initialize TTF - %s", SDL_GetError());
            rc = EXTERNAL_ERROR;
        }
    }

    if (SUCCEED(rc)) {
        mFont = TTF_OpenFont(FONT_PATH, 128);
        if(!mFont) {
            LOGE("Open ttf failed, %s %s", FONT_PATH, TTF_GetError());
            rc = NOT_INITED;
        }
    }

    sem_init(&mSocket2Sdl, 0, 0);
    sem_init(&mSdl2Socket, 0, 0);

    return rc;
}

int32_t Sdl::sendMsgCmd(int acceptfd, int cmd)
{
    int32_t rc = NO_ERROR;
    size_t msgsend;

    LOGI("send cmd %s", gCmdName[cmd]);
    msgsend = send(acceptfd, &cmd, sizeof(int), 0);
    if (msgsend != sizeof(int)) {
        LOGE("fail to send %s msgsend %zu", strerror(errno), msgsend);
        rc = CLIENT_ERROR;
    }

    return rc;
}

int32_t Sdl::createWindow()
{
    int32_t rc = NO_ERROR;

    if (SUCCEED(rc)) {
        mWin = SDL_CreateWindow("Pandora Test",
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              IMAGE_W * 2, IMAGE_H, SDL_WINDOW_SHOWN|SDL_WINDOW_FULLSCREEN);
        if (ISNULL(mWin)) {
            LOGE("SDL: could not SDL_CreateWindow - %s", SDL_GetError());
            rc = EXTERNAL_ERROR;
        }
    }

    if (SUCCEED(rc)) {
        mRender = SDL_CreateRenderer(mWin, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (ISNULL(mRender)) {
            LOGE("SDL: could not SDL_CreateRenderer - %s", SDL_GetError());
            rc = EXTERNAL_ERROR;
        }
    }

    return rc;
}

void Sdl::calcRect(int32_t imageW, int32_t imageH, SDL_Rect &rect, int32_t location)
{
    int32_t rectW = mMode.w;
    int32_t rectH = mMode.h / MAX_TEST_CASE;

    rect.h = MIN(imageH, rectH);
    rect.w = MIN(imageW * rect.h / imageH, rectW / 2);
    rect.h = imageH * rect.w / imageW;
    rect.x = rect.w * rect.x / imageW;
    rect.y = rectH * location;
}

int32_t Sdl::recvBufAndImg(int acceptfd, int32_t imgIndex)
{
    int32_t rc = NO_ERROR;
    size_t recvSize = 0;
    bufInfo buf;
    uint8_t *imgBuf = nullptr;

    if (SUCCEED(rc)) {
        recvSize = recv(acceptfd, &buf, sizeof(buf), 0);
        LOGI("recv buf %zu", recvSize);
        if (recvSize != sizeof(buf)) {
            LOGE("recv buf %zu", recvSize);
            rc = CLIENT_ERROR;
        }
    }

    if (SUCCEED(rc)) {
        std::lock_guard<std::mutex> lck (gMtx);
        auto it = gMap.find(acceptfd);
        if(it != gMap.end()) {

            LOGI("recv acceptfd buf %d", acceptfd);
            if (it->second.img[imgIndex] == nullptr) {
                it->second.img[imgIndex] = (uint8_t *) malloc(buf.size);
                if (ISNULL(it->second.img[imgIndex])) {
                    LOGE("fail to malloc");
                    rc = NO_MEMORY;
                }
            } else if (it->second.info.size != buf.size) {
                free(it->second.img[imgIndex]);
                it->second.img[imgIndex] = (uint8_t *) malloc(buf.size);
                if (ISNULL(it->second.img[imgIndex])) {
                    LOGE("fail to malloc");
                    rc = NO_MEMORY;
                }
            }
            it->second.ready = true;
            it->second.info.w = buf.w;
            it->second.info.h = buf.h;
            it->second.info.format = buf.format;
            it->second.info.size = buf.size;
            it->second.info.percentage = buf.percentage;
            imgBuf = it->second.img[imgIndex];
        }
    }

    if (SUCCEED(rc)) {
        recvSize = 0;
        do {
            recvSize += recv(acceptfd, imgBuf + recvSize, buf.size - recvSize, 0);
        } while(recvSize < buf.size);
        if (recvSize != buf.size) {
            LOGE("recv buf %zu/%zu", recvSize, buf.size);
            rc = CLIENT_ERROR;
        }
    }

    if (SUCCEED(rc)) {
        sendMsgCmd(acceptfd, ACK);
    }

    return rc;
}

int32_t Sdl::updateTextureAndRenderCopy(imgInfo &info)
{
    int32_t rc = NO_ERROR;
    SDL_Texture *texture;

    if (SUCCEED(rc)) {
        int32_t sdlFormat = 0;
        switch (info.info.format) {
            case FORMAT_YUV_SEMI_PLANAR:
                sdlFormat = SDL_PIXELFORMAT_NV12;
                break;
            case FORMAT_YVU_SEMI_PLANAR:
                sdlFormat = SDL_PIXELFORMAT_NV21;
                break;
            case FORMAT_YUV_PLANAR:
                sdlFormat = SDL_PIXELFORMAT_IYUV;
                break;
            case FORMAT_YUV_MONO:
                sdlFormat = SDL_PIXELFORMAT_IYUV;
                break;
            default:
                sdlFormat = SDL_PIXELFORMAT_NV21;
                LOGI("%d default format nv21", info.info.format);
                break;
        }

        texture = SDL_CreateTexture(mRender, sdlFormat, SDL_TEXTUREACCESS_STREAMING, info.info.w, info.info.h);
        if (ISNULL(texture)) {
            LOGE("fail to SDL_CreateTexture %s", SDL_GetError());
            rc = UNKNOWN_ERROR;
        }
    }

    if (SUCCEED(rc)) {
        if (SDL_UpdateTexture(texture, nullptr, info.img[0], info.info.w) < 0) {
            LOGE("Failed to update texture, %s", SDL_GetError());
            rc = EXTERNAL_ERROR;
        }
        SDL_Rect dstrect = { 0, 0, info.info.w, info.info.h };
        calcRect(info.info.w, info.info.h, dstrect, info.location);

        if (SDL_RenderCopy(mRender, texture, NULL, &dstrect) < 0) {
            LOGE("Failed to copy input render. %s", SDL_GetError());
            rc = EXTERNAL_ERROR;
        }
    }

    if (SUCCEED(rc)) {
        if (SDL_UpdateTexture(texture, nullptr, info.img[1], info.info.w) < 0) {
            LOGE("Failed to update texture, %s", SDL_GetError());
            rc = EXTERNAL_ERROR;
        }
        SDL_Rect dstrect = { info.info.w, 0, info.info.w, info.info.h };
        calcRect(info.info.w, info.info.h, dstrect, info.location);
        if (SDL_RenderCopy(mRender, texture, NULL, &dstrect) < 0) {
            LOGE("Failed to copy input render. %s", SDL_GetError());
            rc = EXTERNAL_ERROR;
        }
    }

    if (NOTNULL(texture)) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }

    return rc;
}

int32_t Sdl::displayProgress(imgInfo &info)
{
    int32_t rc = NO_ERROR;
    SDL_Rect sdlRect;
    LOGI("w %d, h %d, format %s percent %d location %d", info.info.w, info.info.h,
        gFormatStr[info.info.format], info.info.percentage, info.location);

    if (SUCCEED(rc)) {
        char text[128];
        SDL_Texture *textureFont = nullptr;
        SDL_Surface *surface = nullptr;
        SDL_Color color = {255, 255, 255};
        sdlRect.x = 0;
        sdlRect.y = info.location * mMode.h / MAX_TEST_CASE;
        sdlRect.w = 360;
        sdlRect.h = 40;
        snprintf(text, sizeof(text), "%dx%d %s %d%% %zu", info.info.w, info.info.h, gFormatStr[info.info.format],
            info.info.percentage, gMap.size());
        surface = TTF_RenderText_Blended(mFont, text, color);
        if (ISNULL(surface)) {
            LOGE("fail to TTF_RenderText_Blended");
            rc = UNKNOWN_ERROR;
        }
        if (SUCCEED(rc)) {
            textureFont = SDL_CreateTextureFromSurface(mRender, surface);
            if (ISNULL(textureFont)) {
                LOGE("fail to SDL_CreateTextureFromSurface");
                rc = UNKNOWN_ERROR;
            }
        }
        if (SUCCEED(rc)) {
            SDL_RenderCopy(mRender, textureFont, NULL, &sdlRect);
        }
        if (NOTNULL(surface)) {
            SDL_FreeSurface(surface);
        }
        if (NOTNULL(textureFont)) {
            SDL_DestroyTexture(textureFont);
        }
    }

    if (SUCCEED(rc)) {
        if (info.info.percentage > 95) {
            char text[128];
            SDL_Texture *textureFont = nullptr;
            SDL_Surface *surface = nullptr;
            SDL_Color color = {0, 255, 0};
            sdlRect.x = 0;
            sdlRect.y = 0;
            sdlRect.w = 0;
            sdlRect.h = 0;
            calcRect(info.info.w, info.info.h, sdlRect, info.location);
            sdlRect.w *= 2;
            snprintf(text, sizeof(text), "TEST PASSED!");
            surface = TTF_RenderText_Blended(mFont, text, color);
            textureFont = SDL_CreateTextureFromSurface(mRender, surface);
            SDL_RenderCopy(mRender, textureFont, NULL, &sdlRect);
            if (NOTNULL(surface)) {
                SDL_FreeSurface(surface);
            }
            if (NOTNULL(textureFont)) {
                SDL_DestroyTexture(textureFont);
            }
        }
    }

    return rc;
}

void Sdl::threadSocket(int acceptfd)
{
    imgInfo info = {0};

    {
        std::lock_guard<std::mutex> lck (gMtx);

        for (int i = 0; i < MAX_TEST_CASE; i++) {
            if (mDisplayRect[i] == false) {
                mDisplayRect[i] = true;
                info.location = i;
                break;
            }
        }
        gMap[acceptfd] = info;
    }

    while(!mQuit) {
        MsgCmd msg = START;
        if (recv(acceptfd, &msg, sizeof(msg), 0) <= 0) {
            LOGE("fail to recv %s", strerror(errno));
            break;
        }

        if (msg == FRAME_IN || msg == FRAME_OUT) {
            int32_t imgIndex = msg - FRAME_IN;
            recvBufAndImg(acceptfd, imgIndex);
        } else if (msg == PROCESS) {
            sem_post(&mSocket2Sdl);
            sem_wait(&mSdl2Socket);
            msg = PROCESS_FINISHED;
            sendMsgCmd(acceptfd, msg);
        } else if (msg == END) {
            close(acceptfd);
            break;
        }
    }

    SECURE_FREE(info.img[0]);
    SECURE_FREE(info.img[1]);
    mDisplayRect[info.location] = false;
    std::lock_guard<std::mutex> lck (gMtx);
    gMap.erase(acceptfd);

    LOGI("---------- exit %d", acceptfd);
}

void Sdl::threadSdl()
{
    int32_t rc = NO_ERROR;

    if (SUCCEED(rc)) {
        rc = createWindow();
        if (FAILED(rc)) {
            LOGE("fail to createWindow");
            rc = EXTERNAL_ERROR;
        }
    }

    if (SUCCEED(rc)) {
        const int windowDisplayIndex = SDL_GetWindowDisplayIndex(mWin);
        if (0 == SDL_GetCurrentDisplayMode(windowDisplayIndex, &mMode)) {
            LOGI("SDL_GetCurrentDisplayMode: %dx%d@%d",
                         mMode.w, mMode.h, mMode.refresh_rate);
        }
    }

    if (SUCCEED(rc)) {
        while(!mQuit) {
            sem_wait(&mSocket2Sdl);
            SDL_RenderClear(mRender);
            std::lock_guard<std::mutex> lck (gMtx);
            for (auto &it : gMap) {
                if (it.second.ready) {
                    rc = updateTextureAndRenderCopy(it.second);
                    if (FAILED(rc)) {
                        break;
                    }
                    displayProgress(it.second);
                    if (FAILED(rc)) {
                        break;
                    }
                }
            }
            SDL_RenderPresent(mRender);
            sem_post(&mSdl2Socket);
        }
    }
}

int32_t Sdl::process()
{
    int32_t rc = NO_ERROR;
    fd_set readfds;
    fd_set tempfds;
    int32_t maxfd;
    std::vector<std::thread> workers;
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    FD_ZERO(&readfds);
    FD_SET(mSockfd, &readfds);
    maxfd = mSockfd;

    workers.push_back(std::thread(threadSdl));

    while (!mQuit) {
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        tempfds = readfds;
        int32_t fdCount = select(maxfd + 1, &tempfds, NULL, NULL, &timeout);
        if (fdCount < 0) {
            LOGE("fail to select %s", strerror(errno));
            rc = SYS_ERROR;
        } else if (fdCount == 0) {
            LOGI("select timeout %s, mSockfd %d", strerror(errno), mSockfd);
        }

        if (SUCCEED(rc)) {
            for (int32_t i = 0; i < maxfd + 1; i++) {
                if (FD_ISSET(i, &readfds)) {
                    if (i == mSockfd) {
                        int acceptfd = 0;
                        acceptfd = accept(mSockfd, (struct sockaddr *)&client_addr, &addrlen);
                        if (acceptfd < 0) {
                            LOGE("fail to accept %s", strerror(errno));
                            rc = CLIENT_ERROR;
                        } else {
                            LOGI("acceptfd %d %s ---> %d\n", acceptfd, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                        }
                        if (SUCCEED(rc)) {
                            workers.push_back(std::thread(threadSocket, acceptfd));
                        }
                    }
                }
            }
        }

        SDL_Event event;
        event.type = 0;
        while (SDL_PollEvent(&event)) {
            LOGI("event %d %d", event.type, event.window.event);
            switch (event.type) {
                case SDL_WINDOWEVENT:
                     if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                        mQuit = true;
                    }
                break;
                case SDL_QUIT:
                case SDL_APP_WILLENTERBACKGROUND:
                case SDL_APP_DIDENTERBACKGROUND:
                    mQuit = true;
                    break;
            }
        }
    }
    sem_post(&mSocket2Sdl);
    std::for_each(workers.begin(), workers.end(), [](std::thread &t){t.join();LOGI("thread exit");});

    return rc;
}

int32_t Sdl::release()
{
    int32_t rc = NO_ERROR;

    if (SUCCEED(rc)) {
        mQuit = false;
        close(mSockfd);
    }

    if (NOTNULL(mRender)) {
        SDL_DestroyRenderer(mRender);
        mRender = nullptr;
    }

    if (NOTNULL(mWin)) {
        SDL_DestroyWindow(mWin);
        mWin = nullptr;
    }

    if (NOTNULL(mFont)) {
        TTF_CloseFont(mFont);
        mFont = nullptr;
    }

    if (SUCCEED(rc)) {
        TTF_Quit();
        SDL_TLSCleanup();
        SDL_Quit();
    }

    return rc;
}

Sdl::Sdl() :
    mSockfd(0)
{
}

Sdl::~Sdl()
{
    release();
}


