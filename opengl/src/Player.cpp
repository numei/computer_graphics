#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Player.h"

void Player::Update(float dt, const bool keys[1024], const glm::vec3 &cameraFront, const glm::vec3 &cameraUp)
{
    // === 水平移动（相对于当前视角） ===
    glm::vec3 forward = glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));
    glm::vec3 right = glm::normalize(glm::cross(forward, cameraUp));

    // Shift 冲刺：基于体力的速度倍率
    isSprinting = false;
    float sprintMultiplier = 1.0f;
    if (keys[GLFW_KEY_LEFT_SHIFT] || keys[GLFW_KEY_RIGHT_SHIFT])
    {
        if (stamina > 0.0f)
        {
            isSprinting = true;
            sprintMultiplier = 2.0f; // 冲刺时移动速度翻倍
        }
    }

    float baseSpeed = moveSpeed;
    float speed = baseSpeed * sprintMultiplier * dt;

    glm::vec3 move(0.0f);
    if (keys[GLFW_KEY_W])
        move += forward;
    if (keys[GLFW_KEY_S])
        move -= forward;
    if (keys[GLFW_KEY_A])
        move -= right;
    if (keys[GLFW_KEY_D])
        move += right;

    if (glm::length(move) > 0.01f)
    {
        move = glm::normalize(move) * speed;
        pos += move;
    }

    // === 跳跃输入与竖直运动（重力在 Game::Update 中处理） ===
    // 这里只处理跳跃按键和冷却计时，实际 verticalVel/pos.y 在 Game 中综合 floor 碰撞更新
    if (jumpCooldown > 0.0f)
        jumpCooldown -= dt;
    if (jumpCooldown < 0.0f)
        jumpCooldown = 0.0f;

    // Space 触发单段跳：必须在地面且冷却结束
    if (keys[GLFW_KEY_SPACE] && isGrounded && jumpCooldown <= 0.0f)
    {
        verticalVel = 6.0f;   // 初始起跳速度（可调）
        isGrounded = false;
        jumpCooldown = 0.3f;  // 防止频繁连跳（可调）
    }

    // 体力更新：冲刺时消耗，非冲刺时恢复
    if (isSprinting)
    {
        stamina -= staminaDrainRate * dt;
        if (stamina < 0.0f)
            stamina = 0.0f;
    }
    else
    {
        stamina += staminaRegenRate * dt;
        if (stamina > 1.0f)
            stamina = 1.0f;
    }
}
