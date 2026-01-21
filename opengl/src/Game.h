#ifndef GAME_HPP
#define GAME_HPP
#include <vector>
#include <random>
#include <iostream>
#include <glm/glm.hpp>
#include "Player.h"
#include "StaticModel.h"
#include "Shader.h"

struct Collectible
{
    glm::vec3 pos;
    glm::vec3 color;
    float lifetime;  // seconds remaining
    bool alive;
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
    StaticModel fallingPrototype; // e.g. crate model
    StaticModel floorModel;       // detailed floor model
    StaticModel fallingModels[3]; // optional multiple falling models
    std::vector<Collectible> collectibles;
    float floorTop = -0.5f;       // 可在 LoadResources 后用 floorModel bbox 覆盖
    float floorYOffset;
    float spawnTimer;
    bool playerDead;

    // 玩家生命值：最多 3 次受击
    int playerMaxHealth = 3;
    int playerHealth = 3;
    int score = 0;

    // 受击红光特效计时
    float hitEffectTimer = 0.0f;

    // 收集物生成控制
    float collectSpawnTimer = 0.0f;

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
    void Render(unsigned int shader3D, const glm::vec3 &cameraPos);
    void SetCubeVAO(unsigned int vao) { cubeVAO = vao; }

    StaticModel playerModel;

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
    }

    bool LoadResources(const std::string &assetsDir);

private:
    unsigned int cubeVAO = 0;
    void SpawnObject();
};
#endif
