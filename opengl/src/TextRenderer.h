#ifndef TEXT_RENDERER_HPP
#define TEXT_RENDERER_HPP

#include <string>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "stb_truetype.h"

struct FontAtlas
{
    GLuint tex = 0;
    stbtt_bakedchar data[96];
    int width = 512, height = 512;
    bool ok = false;
};

class TextRenderer
{
public:
    FontAtlas atlas;
    unsigned int vao = 0, vbo = 0;
    bool LoadFont(const char *ttf_path, int px_height = 48);
    void RenderText(const std::string &text, float x_ndc, float y_ndc, float scale, const glm::vec3 &color, int screenW, int screenH, unsigned int shader);
};
#endif
