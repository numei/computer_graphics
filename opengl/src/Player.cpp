#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Player.h"

void Player::Update(float dt, const bool keys[1024], const glm::vec3 &cameraFront, const glm::vec3 &cameraUp)
{
    glm::vec3 forward = glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));
    glm::vec3 right = glm::normalize(glm::cross(forward, cameraUp));
    float speed = 3.0f * dt;
    glm::vec3 move(0.0f);
    if (keys[GLFW_KEY_W])
        pos += speed * cameraFront;
    if (keys[GLFW_KEY_S])
        pos -= speed * cameraFront;
    if (keys[GLFW_KEY_A])
        pos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
    if (keys[GLFW_KEY_D])
        pos += glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
    if (glm::length(move) > 0.01f)
        move = glm::normalize(move) * moveSpeed * dt;

    pos += move;

    // Keep player's Y fixed at groundY
    pos.y = groundY;
}
