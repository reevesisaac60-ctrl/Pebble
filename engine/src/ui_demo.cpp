#include "ui/opengl_renderer.hpp"
#include "ui/ui_context.hpp"

#include <SDL3/SDL.h>
#include <epoxy/gl.h>

#include <algorithm>
#include <memory>
#include <string>

namespace {
using namespace pebble::ui;

constexpr Color bg{0.055F, 0.057F, 0.068F, 1.0F};
constexpr Color surface{0.078F, 0.081F, 0.095F, 1.0F};
constexpr Color raised{0.105F, 0.108F, 0.126F, 1.0F};
constexpr Color line{0.17F, 0.17F, 0.20F, 1.0F};
constexpr Color primary{0.91F, 0.90F, 0.93F, 1.0F};
constexpr Color muted{0.53F, 0.53F, 0.59F, 1.0F};
constexpr Color lavender{0.73F, 0.67F, 0.91F, 1.0F};
constexpr Color lavenderDim{0.19F, 0.17F, 0.25F, 1.0F};
constexpr Color cyan{0.56F, 0.78F, 0.83F, 1.0F};

DisplayMetrics metricsFor(SDL_Window* window) {
    int ww = 1, wh = 1, fw = 1, fh = 1;
    SDL_GetWindowSize(window, &ww, &wh);
    SDL_GetWindowSizeInPixels(window, &fw, &fh);
    return {std::max(1, ww), std::max(1, wh), std::max(1, fw), std::max(1, fh)};
}

void label(Renderer& r, float x, float y, std::string_view s, Color c = primary, float size = 13.0F) {
    r.text(x, y, s, c, size);
}
void panel(Renderer& r, Rect rect, Color fill = surface) {
    r.fillRect(rect, fill, 8.0F);
    r.strokeRect(rect, line, 1.0F, 8.0F);
}

std::unique_ptr<Element> makeDemoTree(Button*& primaryButton, CheckBox*& optionCheck, TextField*& nameField) {
    auto root = std::make_unique<Element>("ProjectSetup");
    root->width = Length::flex();
    root->height = Length::flex();
    root->gap = 10.0F;
    root->axis = Axis::Vertical;
    root->crossAlign = Align::Stretch;

    auto heading = std::make_unique<Label>("PROJECT CONFIGURATION");
    heading->height = Length::fixed(20.0F);
    root->add(std::move(heading));
    auto name = std::make_unique<TextField>("My first project");
    name->width = Length::flex();
    name->height = Length::fixed(38.0F);
    nameField = name.get();
    root->add(std::move(name));
    auto check = std::make_unique<CheckBox>("Enable responsive preview", true);
    check->height = Length::fixed(28.0F);
    optionCheck = check.get();
    root->add(std::move(check));
    auto button = std::make_unique<Button>("Create project", [] {});
    button->width = Length::fixed(176.0F);
    button->height = Length::fixed(38.0F);
    primaryButton = button.get();
    root->add(std::move(button));
    return root;
}

void drawDashboard(Renderer& r, const DisplayMetrics& m, std::string_view status) {
    const float w = static_cast<float>(m.windowWidth);
    const float h = static_cast<float>(m.windowHeight);
    const bool compact = w < 900.0F;
    const float rail = compact ? 64.0F : 214.0F;
    const float left = rail + 28.0F;
    const float contentW = std::max(240.0F, w - left - 28.0F);

    r.fillRect({0, 0, w, h}, bg);
    // App bar
    r.fillRect({0, 0, w, 58}, surface);
    r.line(0, 57, w, 57, line, 1);
    r.fillRect({22, 17, 24, 24}, lavenderDim);
    label(r, 29, 23, "P", lavender, 16);
    label(r, 58, 20, "PEBBLE", primary, 15);
    label(r, 58, 39, "ENGINE WORKSPACE", muted, 9);
    label(r, std::max(rail + 30.0F, w - 290.0F), 25, "CUSTOM UI PREVIEW", muted, 10);
    r.fillRect({w - 42, 19, 8, 8}, cyan);

    // Navigation rail
    r.fillRect({0, 58, rail, h - 58}, surface);
    r.line(rail - 1, 58, rail - 1, h, line, 1);
    if (!compact) {
        label(r, 22, 84, "WORKSPACE", muted, 10);
        r.fillRect({10, 101, rail - 20, 34}, lavenderDim, 6.0F);
        r.fillRect({10, 101, 3, 34}, lavender);
        label(r, 25, 112, "Overview", primary, 13);
        label(r, 25, 153, "Scene Editor", muted, 13);
        label(r, 25, 189, "Asset Library", muted, 13);
        label(r, 25, 225, "Packages", muted, 13);
        r.line(18, 251, rail - 18, 251, line, 1);
        label(r, 22, 270, "RECENT PROJECTS", muted, 10);
        r.fillRect({23, 293, 8, 8}, lavender);
        label(r, 40, 290, "Untitled", primary, 12);
        label(r, 40, 309, "Local workspace", muted, 10);
        r.fillRect({23, 337, 8, 8}, cyan);
        label(r, 40, 334, "Sandbox", muted, 12);
        label(r, 22, h - 43, "PebbleUI · Preview", muted, 10);
    } else {
        label(r, 24, 112, "O", lavender, 14);
        label(r, 24, 153, "S", muted, 14);
        label(r, 24, 194, "A", muted, 14);
        label(r, 24, 235, "P", muted, 14);
    }

    // Main heading
    label(r, left, 82, "Good to see you.", primary, 24);
    label(r, left, 116, "Build something worth playing.", muted, 13);
    const float actionW = compact ? 0.0F : 138.0F;
    if (!compact) {
        r.fillRect({w - actionW - 28, 83, actionW, 34}, lavender, 7.0F);
        label(r, w - actionW - 10, 94, "+  New project", bg, 12);
    }

    // Feature banner
    const float heroY = 151.0F;
    const float heroH = compact ? 120.0F : 136.0F;
    panel(r, {left, heroY, contentW, heroH}, raised);
    r.fillRect({left, heroY, 4, heroH}, lavender, 2.0F);
    label(r, left + 22, heroY + 17, "PEBBLE ENGINE", lavender, 10);
    label(r, left + 22, heroY + 39, "Your next world starts here.", primary, compact ? 16 : 20);
    label(r, left + 22, heroY + 68, "A responsive editor foundation, built in-house.", muted, 12);
    if (!compact) {
        const float badgeX = left + contentW - 148;
        r.fillRect({badgeX, heroY + 22, 122, 26}, lavenderDim, 6.0F);
        label(r, badgeX + 12, heroY + 30, "UI FOUNDATION", lavender, 10);
        label(r, badgeX, heroY + 66, "SDL3  /  OpenGL 3.3", muted, 10);
    }

    // Status cards
    const float gap = 12.0F;
    const float cardW = (contentW - gap * 2.0F) / 3.0F;
    const float cardsY = heroY + heroH + 14.0F;
    const float cardH = 76.0F;
    const char* titles[] = {"LAYOUT ENGINE", "INPUT SYSTEM", "OPENGL BACKEND"};
    const char* values[] = {"Responsive", "Interactive", "Connected"};
    const char* details[] = {"Logical units + DPI", "Mouse + keyboard", "Batched primitives"};
    const Color accents[] = {lavender, cyan, lavender};
    for (int i = 0; i < 3; ++i) {
        const float x = left + static_cast<float>(i) * (cardW + gap);
        panel(r, {x, cardsY, cardW, cardH});
        r.fillRect({x + 13, cardsY + 14, 3, 35}, accents[i]);
        label(r, x + 25, cardsY + 11, titles[i], muted, 9);
        label(r, x + 25, cardsY + 29, values[i], primary, compact ? 11 : 14);
        if (cardW > 175.0F) label(r, x + 25, cardsY + 51, details[i], muted, 10);
    }

    // Workspace panel containing interactive controls
    const float formY = cardsY + cardH + 14.0F;
    const float formH = std::max(150.0F, h - formY - 58.0F);
    panel(r, {left, formY, contentW, formH});
    label(r, left + 18, formY + 15, "QUICK START", primary, 12);
    label(r, left + 18, formY + 35, "Configure a project preview", muted, 11);

    // Footer
    r.fillRect({rail, h - 34, w - rail, 34}, surface);
    r.line(rail, h - 34, w, h - 34, line, 1);
    label(r, left, h - 22, status, muted, 10);
    label(r, std::max(left + 170.0F, w - 180.0F), h - 22, "CUSTOM RENDERER  ·  DEMO", muted, 9);
}
}

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow("PebbleUI — Obsidian Workspace Preview", 1280, 800,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) { SDL_Quit(); return 2; }
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) { SDL_DestroyWindow(window); SDL_Quit(); return 3; }
    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(1);
    SDL_StartTextInput(window);
    glEnable(GL_FRAMEBUFFER_SRGB);

    auto renderer = std::make_unique<OpenGLRenderer>();
    if (!renderer->initialize()) {
        SDL_GL_DestroyContext(glContext); SDL_DestroyWindow(window); SDL_Quit(); return 4;
    }
    Style style;
    style.background = bg; style.panel = surface; style.panelRaised = raised;
    style.border = line; style.text = primary; style.textMuted = muted; style.accent = lavender;
    Context context(style);
    Button* primaryButton = nullptr;
    CheckBox* optionCheck = nullptr;
    TextField* nameField = nullptr;
    auto root = makeDemoTree(primaryButton, optionCheck, nameField);
    std::string status = "Ready  /  Preview only";
    primaryButton->onClick = [&status, nameField, optionCheck] {
        status = "Preview: " + nameField->value + (optionCheck->checked ? "  /  responsive on" : "  /  responsive off");
    };

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED: running = false; break;
            case SDL_EVENT_MOUSE_MOTION:
                context.handlePointer({PointerEventType::Move, event.motion.x, event.motion.y}); break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                context.handlePointer({PointerEventType::Down, event.button.x, event.button.y, 0, 0, static_cast<int>(event.button.button)}); break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                context.handlePointer({PointerEventType::Up, event.button.x, event.button.y, 0, 0, static_cast<int>(event.button.button)}); break;
            case SDL_EVENT_TEXT_INPUT: context.handleText(event.text.text); break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE) running = false;
                else if (event.key.key == SDLK_BACKSPACE) context.handleBackspace();
                else if (event.key.key == SDLK_TAB) context.handleTab((event.key.mod & SDL_KMOD_SHIFT) != 0);
                break;
            default: break;
            }
        }

        const DisplayMetrics metrics = metricsFor(window);
        glViewport(0, 0, metrics.framebufferWidth, metrics.framebufferHeight);
        glClearColor(bg.r, bg.g, bg.b, bg.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderer->beginFrame(metrics);
        drawDashboard(*renderer, metrics, status);
        renderer->endFrame();

        const float width = static_cast<float>(metrics.windowWidth);
        const float height = static_cast<float>(metrics.windowHeight);
        const bool compact = width < 900.0F;
        const float rail = compact ? 64.0F : 214.0F;
        const float left = rail + 46.0F;
        const float contentW = std::max(200.0F, width - left - 46.0F);
        const float heroH = compact ? 120.0F : 136.0F;
        const float formY = 151.0F + heroH + 14.0F + 76.0F + 14.0F;
        root->padding = {left + 18.0F, formY + 62.0F, std::max(20.0F, width - left - contentW + 18.0F), std::max(42.0F, height - formY - 92.0F)};
        root->maximum.width = contentW - 36.0F;
        root->maximum.height = std::max(100.0F, height - formY - 92.0F);
        context.beginFrame(metrics);
        context.render(*root, *renderer);
        context.endFrame();
        SDL_GL_SwapWindow(window);
    }

    SDL_StopTextInput(window);
    renderer.reset();
    SDL_GL_DestroyContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
