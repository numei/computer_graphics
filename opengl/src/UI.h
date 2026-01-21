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
    void Render(int winW, int winH, unsigned int textShader, bool gameOver);
};
#endif
