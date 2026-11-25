// =====================================================================
// main.cpp - FreeGLUT entry point of the native OpenGL frontend
// =====================================================================
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
#include "App/App.h"
#include "../../backend/Config.h"
#include <GL/freeglut.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#endif
#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D   // OpenGL 1.3 token; Windows gl.h only ships 1.1 headers
#endif

static App* g_app = nullptr;

static void onDisplay() { g_app->display(); }
static void onReshape(int w, int h) { g_app->reshape(w, h); glutPostRedisplay(); }
static void onMouse(int b, int s, int x, int y) { g_app->mouseButton(b, s, x, y); }
static void onMotion(int x, int y) { g_app->mouseMove(x, y); }
static void onWheel(int, int dir, int x, int y) { g_app->wheel(dir, x, y); }
static void onKey(unsigned char c, int, int) {
    if ((glutGetModifiers() & GLUT_ACTIVE_CTRL) && (c == 'v' || c == 'V')) c = 22;
    g_app->keyChar(c);
}
static void onSpecial(int k, int, int) { g_app->keySpecial(k); }
static void onTimer(int) {
    g_app->tick();
    glutPostRedisplay();          // ~60 fps for hover states, caret blink and 3-D rotation (data is cached)
    glutTimerFunc(16, onTimer, 0);
}

int main(int argc, char** argv) {
    if (!scms::Config::instance().load("config/app.conf"))
        std::fprintf(stderr, "warning: config/app.conf not found - using defaults (run from the project root)\n");
#ifdef _WIN32
    SetProcessDPIAware();
    float pixelScale = 1.0f;
    HDC screen = GetDC(nullptr);
    if (screen) {
        pixelScale = std::max(1.0f, GetDeviceCaps(screen, LOGPIXELSX) / 96.0f);
        ReleaseDC(nullptr, screen);
    }
#else
    float pixelScale = 1.0f;
#endif
    glutInit(&argc, argv);
    glutSetOption(GLUT_MULTISAMPLE, 8);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_MULTISAMPLE);
    int screenWidth = glutGet(GLUT_SCREEN_WIDTH);
    int screenHeight = glutGet(GLUT_SCREEN_HEIGHT);
    int logicalWidth = std::max(App::MIN_W, std::min(1500, static_cast<int>(screenWidth * 0.9f / pixelScale)));
    int logicalHeight = std::max(App::MIN_H, std::min(920, static_cast<int>(screenHeight * 0.85f / pixelScale)));
    glutInitWindowSize(static_cast<int>(std::lround(logicalWidth * pixelScale)),
                       static_cast<int>(std::lround(logicalHeight * pixelScale)));
    glutCreateWindow("SmartCity - Complaint Monitoring & Management");
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    static App app;
    app.setPixelScale(pixelScale);
    g_app = &app;
    app.init();

    glutDisplayFunc(onDisplay);
    glutReshapeFunc(onReshape);
    glutMouseFunc(onMouse);
    glutMotionFunc(onMotion);
    glutPassiveMotionFunc(onMotion);
    glutMouseWheelFunc(onWheel);
    glutKeyboardFunc(onKey);
    glutSpecialFunc(onSpecial);
    glutTimerFunc(16, onTimer, 0);
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);
    glutMainLoop();
    return 0;
}
