#include <string>
#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>
#include <unistd.h>



#include "Sdl.h"

int main(int argc, char *argv[]) {
    int32_t rc = NO_ERROR;
    Sdl *sdl = new Sdl;
    LOGI("SDL  -----------------enter")
    rc = sdl->Init();
    if (SUCCEED(rc)) {
        sdl->process();
    }
    delete sdl;
    LOGI("SDL  -----------------exit")

    return 0;
}
