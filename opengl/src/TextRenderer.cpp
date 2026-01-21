#include "TextRenderer.h"
#include <fstream>
#include <vector>
#include <iostream>
#include <glad/glad.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

bool TextRenderer::LoadFont(const char *ttf_path, int px_height)
{
    std::ifstream in(ttf_path, std::ios::binary | std::ios::ate);
    if (!in.is_open())
    {
        std::cerr << "Font not found: " << ttf_path << "\n";
        return false;
    }
    size_t size = in.tellg();
    in.seekg(0);
    std::vector<unsigned char> buf(size);
    in.read((char *)buf.data(), size);
    in.close();

    std::vector<unsigned char> bitmap(atlas.width * atlas.height);
    int res = stbtt_BakeFontBitmap(buf.data(), 0, px_height, bitmap.data(), atlas.width, atlas.height, 32, 96, atlas.data);
    if (res <= 0)
    {
        std::cerr << "Font bake failed\n";
        return false;
    }
    glGenTextures(1, &atlas.tex);
    glBindTexture(GL_TEXTURE_2D, atlas.tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlas.width, atlas.height, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    atlas.ok = true;

    // create VAO/VBO for quads (dynamic)
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4 * 100, nullptr, GL_DYNAMIC_DRAW); // reserve
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0); // pos
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float))); // uv
    glBindVertexArray(0);
    return true;
}

void TextRenderer::RenderText(const std::string &text, float x_ndc, float y_ndc, float scale, const glm::vec3 &color, int screenW, int screenH, unsigned int shader)
{
    if (!atlas.ok)
        return;
    glUseProgram(shader);
    // ortho uniform should be set by caller
    glUniform3f(glGetUniformLocation(shader, "uColor"), color.x, color.y, color.z);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas.tex);
    glUniform1i(glGetUniformLocation(shader, "uTex"), 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(vao);

    // start positions in pixel space (stb baked expects pixels)
    float px = (x_ndc + 1.0f) * 0.5f * screenW;
    float py = (1.0f - (y_ndc + 1.0f) * 0.5f) * screenH;

    std::vector<float> verts;
    verts.reserve(text.size() * 6 * 4);

    for (char c : text)
    {
        if (int(c) < 32 || int(c) >= 128)
            continue;
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(atlas.data, atlas.width, atlas.height, c - 32, &px, &py, &q, 1);
        // convert pixel coords -> NDC for positions; but we'll upload directly positions in NDC
        float x0 = (q.x0 / (float)screenW) * 2.0f - 1.0f;
        float x1 = (q.x1 / (float)screenW) * 2.0f - 1.0f;
        float y0 = 1.0f - (q.y0 / (float)screenH) * 2.0f;
        float y1 = 1.0f - (q.y1 / (float)screenH) * 2.0f;
        float s0 = q.s0, t0 = q.t0, s1 = q.s1, t1 = q.t1;

        // two triangles
        verts.insert(verts.end(), {x0, y0, s0, t0, x1, y0, s1, t0, x1, y1, s1, t1,
                                   x0, y0, s0, t0, x1, y1, s1, t1, x0, y1, s0, t1});
    }

    if (!verts.empty())
    {
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts.size() / 4));
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
}
