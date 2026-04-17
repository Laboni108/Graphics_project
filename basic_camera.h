#pragma once
#ifndef BASIC_CAMERA_H
#define BASIC_CAMERA_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum Camera_Movement { FORWARD, BACKWARD, LEFT, RIGHT, UP, DOWN };

class BasicCamera {
public:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    float Yaw;
    float Pitch;
    float Roll; // Roll

    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;

    BasicCamera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = -90.0f, float pitch = 0.0f)
        : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(2.5f), MouseSensitivity(0.1f), Zoom(45.0f), Roll(0.0f) {
        Position = position;
        WorldUp = up;
        Yaw = yaw;
        Pitch = pitch;
        updateCameraVectors();
    }

    glm::mat4 GetViewMatrix() {
        return glm::lookAt(Position, Position + Front, Up);
    }

    void ProcessKeyboard(Camera_Movement direction, float deltaTime) {
        float velocity = MovementSpeed * deltaTime;
        if (direction == FORWARD)  Position += Front * velocity;
        if (direction == BACKWARD) Position -= Front * velocity;
        if (direction == LEFT)     Position -= Right * velocity;
        if (direction == RIGHT)    Position += Right * velocity;
        if (direction == UP)       Position += Up * velocity;
        if (direction == DOWN)     Position -= Up * velocity;
    }

    void ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch = true) {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        Yaw += xoffset;
        Pitch += yoffset;

        if (constrainPitch) {
            if (Pitch > 89.0f) Pitch = 89.0f;
            if (Pitch < -89.0f) Pitch = -89.0f;
        }
        updateCameraVectors();
    }

    // Pitch rotation
    void ProcessPitch(float offset) {
        Pitch += offset;
        if (Pitch > 89.0f) Pitch = 89.0f;
        if (Pitch < -89.0f) Pitch = -89.0f;
        updateCameraVectors();
    }

    // Yaw rotation
    void ProcessYaw(float offset) {
        Yaw += offset;
        updateCameraVectors();
    }

    // Roll 
    void ProcessRoll(float offset) {
        Roll += offset;
        updateCameraVectors();
    }

    void updateCameraVectors() {
        // Calculate new Front vector
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(front);

        // Calculate Right and Up vector
        Right = glm::normalize(glm::cross(Front, WorldUp));

        // Calculate Up vector with Roll
        // Standard Up is cross(Right, Front). We rotate this Up vector around Front by Roll angle
        glm::vec3 standardUp = glm::normalize(glm::cross(Right, Front));

        // Apply Roll
        glm::mat4 rollMat = glm::rotate(glm::mat4(1.0f), glm::radians(Roll), Front);
        Up = glm::vec3(rollMat * glm::vec4(standardUp, 1.0f));
    }
};
#endif