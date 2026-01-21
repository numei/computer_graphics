#ifndef GAME_HPP
#define GAME_HPP
#include <vector>
#include <random>
#include <iostream>
#include <glm/glm.hpp>
#include "Player.h"
#include "StaticModel.h"
#include "Shader.h"

enum CatPart
{
    CAT_BODY = 0,
    CAT_HEAD = 1,
    CAT_TAIL = 2,
    CAT_LEG0 = 3,
    CAT_LEG1 = 4,
    CAT_LEG2 = 5,
    CAT_LEG3 = 6,
    CAT_PART_COUNT = 7
};

struct Falling
{
    glm::vec3 pos;
    glm::vec3 vel;
    glm::vec3 color;
    bool alive;
    float rot;            // current rotation angle (radians)
    glm::vec3 rotAxis;    // rotation axis
    float rotSpeed;       // radians per second
    glm::vec3 modelScale; // instance scale
    glm::mat4 modelMatrix;
    glm::vec3 halfExtents; // for AABB collision
    int modelIndex;        // which model to use (if multiple)
};

class Game
{
public:
    Player player;
    std::vector<Falling> falling;

    // cat part prototypes (loaded once)
    StaticModel catParts[CAT_PART_COUNT];
    glm::vec3 catPartLocalOffset[CAT_PART_COUNT]; // in body-local coordinates
    glm::vec3 catPartScale[CAT_PART_COUNT];

    StaticModel fallingPrototype; // e.g. crate model
    StaticModel floorModel;       // detailed floor model
    StaticModel fallingModels[3]; // optional multiple falling models
    float floorTop = -0.5f;       // 可在 LoadResources 后用 floorModel bbox 覆盖
    float floorYOffset;
    float spawnTimer;
    bool playerDead;

    std::mt19937 rng;
    // ===== Shadow mapping =====
    unsigned int depthFBO = 0;
    unsigned int depthMap = 0;

    static constexpr unsigned int SHADOW_SIZE = 2048;

    // shadow shader program id
    unsigned int shadowShader = 0;

    Game();
    void InitShadowMap();
    void Reset();
    void Update(float dt, const bool keys[1024], const glm::vec3 &cameraFront, const glm::vec3 &cameraUp);
    void Render(unsigned int shader3D, float dt, const glm::vec3 &cameraPos);
    void SetCubeVAO(unsigned int vao) { cubeVAO = vao; }

    StaticModel playerModel;

    void LoadCatParts(const std::string &assetDir);

    // Draw function using models
    void DrawAnimatedCatWithModels(const glm::vec3 &pos, float speedFactor,
                                   unsigned int shader3D, unsigned int cubeVAO /*optional*/);

    void LoadPlayerModel(const std::string &path)
    {
        if (!playerModel.LoadFromFile(path))
        {
            std::cerr << "Failed to load player model: " << path << std::endl;
        }
        else
        {

            // 可选：设置默认缩放来匹配原来 cube 大小
            playerModel.modelScale = glm::vec3(0.6f);
        }
        // std::cout << "scene->mNumMeshes: " << this->scene->mNumMeshes << ", scene->mRootNode->mNumChildren: " << this->scene->mRootNode->mNumChildren << std::endl;
    }

    bool LoadResources(const std::string &assetsDir);

private:
    unsigned int cubeVAO = 0;
    void SpawnObject();
};
#endif
