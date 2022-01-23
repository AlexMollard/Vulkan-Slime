#pragma once

#include <VulkanTypes.h>

#include <SDL_events.h>
#include <glm/glm.hpp>


struct PlayerCamera {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 inputAxis;

    float pitch{ 0 }; //up-down rotation
    float yaw{ 0 }; //left-right rotation

    bool bSprint = false;
    bool bLocked;

    void process_input_event(SDL_Event* ev);
    void update_camera(float deltaSeconds);


    glm::mat4 get_view_matrix();
    glm::mat4 get_projection_matrix(bool bReverse = true);
    glm::mat4 get_rotation_matrix();
};