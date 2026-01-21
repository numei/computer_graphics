// src/StaticModel.h
#pragma once
#include <string>
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <unordered_map>

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

    void DrawAnimated(const glm::mat4 &rootModel, float deltaTime, unsigned int shaderID);

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
    bool isMoving = false;
    bool animEnable = false; // whether to animate legs
    float animBlend = 0.0f;

private:
    // store computed local-space pivot for nodes by name (local coordinates of the model file)
    std::unordered_map<std::string, glm::vec3> nodePivots;

    // compute pivots after scene loaded
    void ComputeNodePivots();

    // recursive draw used by DrawAnimated
    void DrawNodeAnimated(const aiNode *node, const glm::mat4 &parentTransform, unsigned int shaderID);

    // helper: compute mesh bbox in node local space (returns min/max)
    void ComputeMeshAABBForNode(const aiNode *node, glm::vec3 &outMin, glm::vec3 &outMax) const;

    Assimp::Importer importer;
    // pointer to loaded Assimp scene (assume you already have it)
    const aiScene *scene = nullptr;

    std::vector<MeshRenderData> meshes;
    std::string directory;

    void Cleanup();

    // helper to load texture file, returns 0 on failure
    static GLuint LoadTextureFromFile(const std::string &filename, bool &outHasAlpha, bool silent);
    void ComputeBBoxRecursive(aiNode *node,
                              const aiScene *scene,
                              const glm::mat4 &parentTransform);
    // Draw single mesh by index (used by DrawNodeAnimated)
    void DrawMeshByIndex(unsigned int meshIndex, unsigned int shaderID) const;
};
