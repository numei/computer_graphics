// src/StaticModel.h
#pragma once
#include <string>
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <assimp/scene.h>

struct SimpleVertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct MeshRenderData
{
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;

    // material
    bool hasDiffuse = false;
    GLuint diffuseTex = 0;
    glm::vec3 diffuseColor = glm::vec3(1.0f);
    // hair/alpha behavior
    bool hasAlpha = false;    // texture contains alpha
    bool isHair = false;      // treat as hair: alpha + alpha cutoff + blending
    float alphaCutoff = 0.5f; // default alpha cutoff for alpha-test
};

class StaticModel
{
public:
    StaticModel();
    ~StaticModel();

    // Load model via Assimp (.obj/.fbx/.gltf/.glb)
    bool LoadFromFile(const std::string &path);

    // Draw with currently bound shader. Caller must set uModel, uNormalMat, and shader must
    // support uHasDiffuse, uHasAlpha, uUseAlphaTest, uAlphaCutoff, uMatDiffuse, and sampler2D uDiffuseMap.
    void Draw(GLuint shaderProgram) const;
    void DrawDepth() const;
    GLuint getDiffuseTexID() const;
    // convenience scale
    glm::vec3 modelScale = glm::vec3(1.0f);
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    glm::vec3 bboxMin = glm::vec3(0.0f);
    glm::vec3 bboxMax = glm::vec3(0.0f);
    bool bboxInitialized = false;

private:
    std::vector<MeshRenderData> meshes;
    std::string directory;

    void Cleanup();

    // helper to load texture file, returns 0 on failure
    static GLuint LoadTextureFromFile(const std::string &filename, bool &outHasAlpha, bool silent);
    void ComputeBBoxRecursive(aiNode *node,
                              const aiScene *scene,
                              const glm::mat4 &parentTransform);
};
