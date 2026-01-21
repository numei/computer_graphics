#ifndef PLAYER_HPP
#define PLAYER_HPP
#include <glm/glm.hpp>

class Player
{
public:
    glm::vec3 pos;
    glm::vec3 color;
    glm::mat4 modelMatrix;
    float moveSpeed = 5.0f;
    float groundY = 0.5f; // Default player height

    // 跳跃 & 冲刺相关状态
    float verticalVel = 0.0f;      // 竖直速度（用于跳跃/重力）
    bool isGrounded = true;        // 是否在地面上
    float jumpCooldown = 0.0f;     // 跳跃冷却计时（秒）

    // 体力（用于冲刺）
    float stamina = 1.0f;          // [0,1] 归一化体力
    float staminaRegenRate = 0.3f; // 每秒恢复多少（归一化）
    float staminaDrainRate = 0.4f; // 冲刺时每秒消耗多少
    bool isSprinting = false;      // 当前是否在冲刺

public:
    Player() {}
    void Update(float dt, const bool keys[1024],
                const glm::vec3 &cameraFront,
                const glm::vec3 &cameraUp);
};
#endif
