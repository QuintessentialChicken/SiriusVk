//
// Created by Leon on 20/09/2025.
//

#pragma once
#include <memory>

#include "../graphics/model.h"

namespace sirius {

struct Transform2DComponent {
    glm::vec2 translation{};
    glm::vec2 scale{};
    float rotation;

    glm::mat2 mat2() {
        const float s = glm::sin(rotation);
        const float c = glm::cos(rotation);
        glm::mat2 rotMatrix{{c, s}, {-s, c}};
        glm::mat2 scaleMat{{scale.x, 0.0f}, {0.0f, scale.y}};
        return  rotMatrix * scaleMat;
    }
};

class srsObject {
public:
    using id_t = unsigned int;

    static srsObject createObject() {
        static id_t currentId = 0;
        return srsObject(currentId++);
    }

    srsObject(const srsObject&) = delete;
    srsObject& operator=(const srsObject&) = delete;
    srsObject(srsObject&&) = default;
    srsObject& operator=(srsObject&&) = delete;

    id_t getId() { return id; }

    std::shared_ptr<srsModel> model{};
    glm::vec3 color{};
    Transform2DComponent transform2d{};
private:
        srsObject(id_t objId) : id{objId} {}

    id_t id;
};
} // sirius

