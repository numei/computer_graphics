#include "UI.h"
#include <iostream>
#include <glad/glad.h>
#include <glm/ext/matrix_clip_space.hpp>

UI::UI()
{
    // NDC positions (center x/y + width/height)
    mainButtons.push_back({0.0f, 0.2f, 0.6f, 0.12f, "Start Game", false});
    mainButtons.push_back({0.0f, -0.05f, 0.6f, 0.12f, "Quit", false});
    gameoverButtons.push_back({0.0f, 0.2f, 0.6f, 0.12f, "Restart", false});
    gameoverButtons.push_back({0.0f, -0.05f, 0.6f, 0.12f, "Quit", false});
}

void UI::Init(const char *fontpath, int fontPx)
{
    text.LoadFont(fontpath, fontPx);
}

// NDC check
static bool PointInRectNDC(float px, float py, const UIButton &b)
{
    float x0 = b.cx - b.w * 0.5f;
    float x1 = b.cx + b.w * 0.5f;
    float y0 = b.cy - b.h * 0.5f;
    float y1 = b.cy + b.h * 0.5f;
    return (px >= x0 && px <= x1 && py >= y0 && py <= y1);
}

void UI::UpdateMouse(float mouseNDCx, float mouseNDCy, bool mouseDown, int winW, int winH, int *outAction, bool gameOver)
{
    // Rising edge click
    static bool prev = false;
    bool clicked = (!prev && mouseDown);
    prev = mouseDown;
    if (!clicked)
        return;
    *outAction = 0;

    // Update hover state in NDC
    if (!gameOver)
    {
        for (auto &b : mainButtons)
            b.hovered = PointInRectNDC(mouseNDCx, mouseNDCy, b);
        for (auto &b : mainButtons)
        {
            if (b.hovered)
            {
                if (b.label == "Start Game")
                {
                    *outAction = 1;
                    return;
                }
                if (b.label == "Quit")
                {
                    *outAction = 2;
                    return;
                }
            }
        }
        return;
    }

    for (auto &b : gameoverButtons)
        b.hovered = PointInRectNDC(mouseNDCx, mouseNDCy, b);
    for (auto &b : gameoverButtons)
    {
        if (b.hovered)
        {
            if (b.label == "Restart")
            {
                *outAction = 3;
                return;
            }
            if (b.label == "Quit")
            {
                *outAction = 2;
                return;
            }
        }
    }
}

void UI::Render(int winW, int winH, unsigned int textShader, bool gameover)
{
    glDisable(GL_DEPTH_TEST);

    auto DrawRect = [&](float cx, float cy, float w, float h, glm::vec3 color)
    {
        float x0 = cx - w * 0.5f, x1 = cx + w * 0.5f;
        float y0 = cy - h * 0.5f, y1 = cy + h * 0.5f;

        float verts[6 * 4]{
            x0, y0, 0, 0,
            x1, y0, 1, 0,
            x1, y1, 1, 1,

            x0, y0, 0, 0,
            x1, y1, 1, 1,
            x0, y1, 0, 1};

        glUseProgram(textShader);
        glm::mat4 ortho = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f);
        glUniformMatrix4fv(glGetUniformLocation(textShader, "uOrtho"), 1, GL_FALSE, &ortho[0][0]);
        glUniform3f(glGetUniformLocation(textShader, "uColor"), color.r, color.g, color.b);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, text.atlas.tex);
        glUniform1i(glGetUniformLocation(textShader, "uTex"), 0);

        glBindVertexArray(text.vao);
        glBindBuffer(GL_ARRAY_BUFFER, text.vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    };

    if (!gameover)
    {
        // Draw main menu
        for (auto &b : mainButtons)
        {
            glm::vec3 base = b.hovered ? glm::vec3(0.9f, 0.7f, 0.4f) : glm::vec3(0.7f, 0.6f, 0.5f);
            DrawRect(b.cx, b.cy, b.w, b.h, base);
            text.RenderText(b.label, b.cx - 0.22f, b.cy - 0.03f, 1.0f, glm::vec3(0.08f), winW, winH, textShader);
        }
    }
    else
    {
        for (auto &b : gameoverButtons)
        {
            glm::vec3 base = b.hovered ? glm::vec3(0.9f, 0.7f, 0.4f) : glm::vec3(0.7f, 0.6f, 0.5f);
            DrawRect(b.cx, b.cy, b.w, b.h, base);
            text.RenderText(b.label, b.cx - 0.22f, b.cy - 0.03f, 1.0f, glm::vec3(0.08f), winW, winH, textShader);
        }
    }

    glEnable(GL_DEPTH_TEST);
}
