//
// Created by Leon on 29/10/2025.
//

#pragma once

#include <vec3.hpp>
#include <mat4x4.hpp>
#include <utility>

class Camera {
public:
    void ProcessWindowEvent(std::pair<float, float> keyInput, std::pair<float, float> mouseInput);

    void Update();

    glm::mat4 GetViewMatrix();

    glm::mat4 GetRotationMatrix();

    glm::vec3 velocity_;
    glm::vec3 position_;
    float pitch_{0.0f};
    float yaw_{0.0f};
};
