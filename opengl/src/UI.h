#ifndef UI_HPP
#define UI_HPP
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "TextRenderer.h"

struct UIButton
{
    float cx, cy, w, h; // NDC coords
    std::string label;
    bool hovered = false;
};

class UI
{
public:
    std::vector<UIButton> mainButtons;
    std::vector<UIButton> gameoverButtons;
    TextRenderer text;
    UI();
    void Init(const char *fontpath, int fontPx);
    void UpdateMouse(float mx, float my, bool mousePressed, int winW, int winH, int *outAction, bool gameOver); // outAction: 0 none, 1 start, 2 quit, 3 retry

    // 渲染菜单界面（开始/结束）
    void Render(int winW, int winH, unsigned int textShader, bool gameOver);

    // 在游戏进行中渲染 HUD：血条 & 体力条
    void RenderHUD(int winW, int winH, unsigned int textShader,
                   int playerHealth, int playerMaxHealth,
                   float stamina,
                   int score,
                   float hitEffectTimer);
};
#endif
