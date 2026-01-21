#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <unistd.h>
#elif _WIN32
#define WIN32_LEAN_AND_MEAN // Reduce Windows.h includes
#include <windows.h>
#include <direct.h>
// Undefine PlaySound macro to avoid conflict with Audio::PlaySound
#ifdef PlaySound
#undef PlaySound
#endif
#else
#include <unistd.h>
#include <limits.h>
#endif
#include "Shader.h"
#include "TextRenderer.h"
#include "UI.h"
#include "Game.h"
#include "Audio.h"
const int WINW = 1280, WINH = 920;
bool keys[1024] = {0};
bool mousePressed = false;

static float survivalTime = 0.0f;

glm::vec3 cameraFront = glm::vec3(0.0f, 0.6f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

bool firstMouse = true;
float yaw = -90.0f; // yaw is initialized to -90.0 degrees since a yaw of 0.0 results in a direction vector pointing to the right so we initially rotate a bit to the left.
float pitch = 0.0f;
float lastX = 800.0f / 2.0;
float lastY = 600.0f / 2.0;
float aspect = 45.0f;
static glm::vec3 smoothCamPos = glm::vec3(0.0f, 5.0f, 8.0f);
static glm::vec3 camVel = glm::vec3(0.0f);
glm::vec3 lightPos = glm::vec3(3.0f, 6.0f, 3.0f);
bool firstPerson = false;
int lastV = GLFW_RELEASE;
enum class State
{
    MENU,
    PLAYING,
    GAMEOVER
};
State state = State::MENU;

void key_cb(GLFWwindow *w, int key, int sc, int action, int mods);
void scroll_cb(GLFWwindow *w, double xoff, double yoff);
void cursor_cb(GLFWwindow *w, double xpos, double ypos);
void mouse_cb(GLFWwindow *w, int button, int action, int mods);
void firstPersonInit();
void thirdPersonInit();

std::string GetExecutableDir()
{
#ifdef _WIN32
    // Windows platform
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string full(path);
    size_t pos = full.find_last_of("\\/");
    return full.substr(0, pos);
#elif __APPLE__
    // macOS platform
    char path[1024];
    uint32_t size = sizeof(path);
    _NSGetExecutablePath(path, &size);

    char resolved[1024];
    realpath(path, resolved);

    std::string full(resolved);
    size_t pos = full.find_last_of("/");
    return full.substr(0, pos);
#else
    // Linux platform
    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    if (count != -1)
    {
        std::string full(path);
        size_t pos = full.find_last_of("/");
        return full.substr(0, pos);
    }
    return ".";
#endif
}

int main()
{

    glfwInit();
    if (!glfwInit())
    {
        std::cerr << "glfwInit failed\n";
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow *win = glfwCreateWindow(WINW, WINH, "CatDodgeModern", NULL, NULL);
    if (!win)
    {
        std::cerr << "window create failed\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(win);
    glfwSetKeyCallback(win, key_cb);
    glfwSetCursorPosCallback(win, cursor_cb);
    glfwSetMouseButtonCallback(win, mouse_cb);
    glfwSetScrollCallback(win, scroll_cb);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "glad failed\n";
        return -1;
    }
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_FRAMEBUFFER_SRGB);
    std::string base = GetExecutableDir();
    Audio audio;
    audio.Init();
    unsigned int dropBuffer = audio.LoadWAV(base + "/assets/sound/drop.wav");
    audio.PlaySound(dropBuffer, true); // loop background sound
    Shader shader3D(
        (base + "/shaders/phong.vs").c_str(),
        (base + "/shaders/phong.fs").c_str());
    Shader shadowShader((base + "/shaders/shadow_depth.vs").c_str(), (base + "/shaders/shadow_depth.fs").c_str());

    Shader shaderText((base + "/shaders/text.vs").c_str(), (base + "/shaders/text.fs").c_str());
    UI ui;
    ui.Init((base + "/assets/fonts/Roboto-Regular.ttf").c_str(), 48); // ensure assets/Roboto-Regular.ttf exists relative to build dir
    Game game;
    game.LoadResources(base + "/assets");
    game.shadowShader = shadowShader.ID;
    game.Reset();
    game.InitShadowMap();
    // Load walk_cat.obj model file
    std::string modelPath = base + "/assets/models/walk_cat.obj";
    game.LoadPlayerModel(modelPath.c_str());
    game.playerModel.modelScale = glm::vec3(0.5f);
    std::vector<float> data;

    float cubeVerts[] = {
        -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f,
        0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
        0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f,
        -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
        0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f, -0.5f,
        0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
        -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f,
        0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f,
        -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f,
        0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f};
    // duplicate colors for each vertex: use single color attribute per draw by setting vertex attribute 1 as constant via glVertexAttrib3f later
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    // set attribute 1 to be constant color using glVertexAttrib3f before drawing
    glBindVertexArray(0);
    // store vao id in a global known to Game::Render via binding assumption:
    // We'll attach VAO id to glObjectLabel? Simpler: set as global
    glBindVertexArray(VAO); // keep bound for draw calls later (we'll not unbind)
    // To make it accessible, set to 1 (not ideal), but we'll use VAO 1 implicitly
    // after creating VAO (variable name VAO)
    game.SetCubeVAO(VAO);
    // Create Text renderer and UI

    auto last = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(win))
    {
        glfwPollEvents();
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        if (keys[GLFW_KEY_V] && lastV == GLFW_RELEASE)
        {
            firstPerson = !firstPerson;
            if (firstPerson)
                firstPersonInit();
            else
                thirdPersonInit();
            lastV = GLFW_PRESS;
        }
        if (!keys[GLFW_KEY_V])
            lastV = GLFW_RELEASE;
        if (keys[GLFW_KEY_ESCAPE])
            glfwSetWindowShouldClose(win, true);

        // get window size and cursor in window coords
        int winW, winH;
        glfwGetWindowSize(win, &winW, &winH);
        double winX, winY;
        glfwGetCursorPos(win, &winX, &winY);
        float mx = float(winX / winW * 2.0 - 1.0);
        float my = float(1.0 - winY / winH * 2.0);

        // poll mouse button
        bool mouseDown = (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

        // pass window coords into UI (note changed function signature: last param is outAction)
        int uiAction = 0;
        ui.UpdateMouse(mx, my, mouseDown, winW, winH, &uiAction, state == State::GAMEOVER);
        if (state == State::MENU)
        {
            if (uiAction == 1)
            {
                state = State::PLAYING;
                game.Reset();
            }
            if (uiAction == 2)
            {
                break;
            }
        }
        else if (state == State::GAMEOVER)
        {
            if (uiAction == 3)
            {
                state = State::PLAYING;
                game.Reset();
            }
            if (uiAction == 2)
            {
                break;
            }
        }
        else if (state == State::PLAYING)
        {
            game.Update(dt, keys, cameraFront, cameraUp);
            if (game.playerDead)
                state = State::GAMEOVER;
        }
        int W, H;
        glfwGetFramebufferSize(win, &W, &H);
        glViewport(0, 0, W, H);
        glClearColor(0.08f, 0.05f, 0.03f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //  Setup projection / view
        glm::mat4 proj = glm::perspective(glm::radians(aspect), (float)W / H, 0.1f, 100.0f);
        glm::mat4 view;
        glm::vec3 cameraPos;
        if (firstPerson)
        {
            cameraPos = game.player.pos + glm::vec3(0.0f, 0.6f, -0.6f);
            view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        }
        else
        {
            // 方案A：用鼠标 yaw/pitch 得到的 cameraFront 来定义第三人称的绕猫视角
            // 目标：相机位于玩家后上方，方向与当前视角一致，距离固定，可平滑跟随
            glm::vec3 viewDir = glm::normalize(cameraFront); // direction from camera toward target

            // 基础参数可按手感微调
            const float followDistance = 12.0f;   // 相机到玩家的水平距离
            const float heightOffset = 2.5f;      // 相机自身的抬高
            const glm::vec3 targetOffset(0.0f, 0.8f, 0.0f); // 让视线瞄准玩家上方一点

            // 以“玩家位置 - 视线方向 * 距离”为基准，再加上垂直抬高
            glm::vec3 desiredPos = game.player.pos - viewDir * followDistance + glm::vec3(0.0f, heightOffset, 0.0f);

            // 平滑跟随：在 desiredPos 与当前 smoothCamPos 之间插值
            float followSpeed = 6.0f;
            float t = glm::clamp(followSpeed * dt, 0.0f, 1.0f);
            smoothCamPos = glm::mix(smoothCamPos, desiredPos, t);

            // 视线目标：玩家位置稍微抬高
            glm::vec3 camTarget = game.player.pos + targetOffset;
            cameraPos = smoothCamPos;
            view = glm::lookAt(smoothCamPos, camTarget, glm::vec3(0, 1, 0));
        }
        if (state == State::PLAYING)
        {
            glBindVertexArray(VAO);
            shader3D.use();
            shader3D.setMat4("uView", view);
            shader3D.setMat4("uProj", proj);

            // now render the game (Game::Render should bind VAO and use shader uniforms)
            game.Render(shader3D.ID, cameraPos);

            glBindVertexArray(0);
        }
        // draw UI overlays
        if (state != State::PLAYING)
        {
            survivalTime = 0.0f;
            // dim overlay
            // draw panel using UI::Render that uses text shader
            // first draw background panels if needed (UI::Render renders buttons)
            bool gameOver = (state == State::GAMEOVER);
            firstPerson = false;
            ui.Render(winW, winH, shaderText.ID, gameOver);
            if (state == State::MENU)
            {
                // title text
                ui.text.RenderText("CAT DODGE", -0.35f, 0.45f, 1.8f, glm::vec3(0.95f), winW, winH, shaderText.ID);
            }
            else
            {
                ui.text.RenderText("GAME OVER", -0.25f, 0.4f, 1.6f, glm::vec3(0.95f), winW, winH, shaderText.ID);
            }
        }
        else
        {
            survivalTime += dt;
            // HUD timer
            char buf[64];
            snprintf(buf, sizeof(buf), "Time: %.2f s", survivalTime);
            ui.text.RenderText(buf, -0.98f, 0.9f, 0.8f, glm::vec3(0.95f), winW, winH, shaderText.ID);
        }

        glfwSwapBuffers(win);
    }
    audio.Shutdown();
    glfwTerminate();
    return 0;
}

void key_cb(GLFWwindow *w, int key, int sc, int action, int mods)
{
    if (action == GLFW_PRESS)
        keys[key] = true;
    else if (action == GLFW_RELEASE)
        keys[key] = false;
}
void cursor_cb(GLFWwindow *w, double x, double y)
{
    float xpos = static_cast<float>(x);
    float ypos = static_cast<float>(y);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f; // change this value to your liking
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // make sure that when pitch is out of bounds, screen doesn't get flipped
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}
void mouse_cb(GLFWwindow *w, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        mousePressed = (action == GLFW_PRESS);
}
void scroll_cb(GLFWwindow *window, double xoffset, double yoffset)
{
    if (firstPerson == true)
        return;
    if (aspect >= 1.0f && aspect <= 45.0f)
        aspect -= yoffset;
    if (aspect <= 1.0f)
        aspect = 1.0f;
    if (aspect >= 45.0f)
        aspect = 45.0f;
}
void thirdPersonInit()
{
    yaw = -90.0f; // yaw is initialized to -90.0 degrees since a yaw of 0.0 results in a direction vector pointing to the right so we initially rotate a bit to the left.
    pitch = 0.0f;
    lastX = 800.0f / 2.0;
    lastY = 600.0f / 2.0;
}
void firstPersonInit()
{
    aspect = 45.0f;
}