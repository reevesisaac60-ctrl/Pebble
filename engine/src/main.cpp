#include "application.hpp"

#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    std::vector<std::string> arguments(argv + 1, argv + argc);

    // SDL-only diagnostic mode: test SDL initialization without creating a
    // window, OpenGL context, or Dear ImGui. Useful when the desktop
    // environment blocks SDL3 video subsystem initialization.
    bool sdlDiag = false;
    for (const std::string& arg : arguments) {
        if (arg == "--sdl-diag" || arg == "-d") {
            sdlDiag = true;
        }
    }

    Application application;

    if (sdlDiag) {
        if (!application.initSDLOnly()) {
            application.shutdown();
            std::fprintf(stderr, "SDL diagnostic: SDL_INIT_VIDEO failed\n");
            return 1;
        }
        std::fprintf(stdout, "SDL diagnostic: SDL_INIT_VIDEO succeeded\n");
        application.shutdown();
        return 0;
    }

    if (!application.init(arguments)) {
        application.shutdown();
        return 1;
    }

    application.run();
    application.shutdown();
    return 0;
}
