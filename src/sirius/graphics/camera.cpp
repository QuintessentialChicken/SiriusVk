//
// Created by Leon on 29/10/2025.
//

#include "camera.h"

#include <iostream>
#include <ext/matrix_transform.hpp>


void Camera::ProcessWindowEvent(std::pair<float, float> keyInput, std::pair<float, float> mouseInput) {
    velocity_.z = keyInput.first;
    velocity_.x = keyInput.second;
}

void Camera::Update() {
    // glm::mat4 cameraRotation = GetRotationMatrix();
    position_ += glm::vec3(glm::vec4(velocity_ * -0.0005f, 0.0f));
}

glm::mat4 Camera::GetViewMatrix() {
    glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.0f), position_);
    return cameraTranslation;
    // glm::mat4 cameraRotation = GetRotationMatrix();

    // return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::GetRotationMatrix() {
    return glm::mat4{};
}
