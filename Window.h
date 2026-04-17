#pragma once
#ifndef WINDOW_H
#define WINDOW_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "shader.h"
#include "Cube.h"

class Window {
public:
    Cube cube;

    // Window dimensions
    float windowWidth;
    float windowHeight;
    float frameThickness;
    float glassThickness;
    glm::vec3 frameColor;
    glm::vec3 glassColor;

    // Default constructor
    Window() {
        windowWidth = 2.0f;
        windowHeight = 1.5f;
        frameThickness = 0.1f;
        glassThickness = 0.05f;
        frameColor = glm::vec3(0.4f, 0.25f, 0.12f);  // Warm walnut frame
        glassColor = glm::vec3(0.75f, 0.85f, 0.92f); // Clear sky blue glass
    }

    // Constructor with custom dimensions and colors
    Window(float width, float height,
        glm::vec3 fColor = glm::vec3(0.3f, 0.25f, 0.2f),
        glm::vec3 gColor = glm::vec3(0.6f, 0.8f, 0.9f)) {
        windowWidth = width;
        windowHeight = height;
        frameThickness = 0.1f;
        glassThickness = 0.05f;
        frameColor = fColor;
        glassColor = gColor;
    }

    // Draw window with frame
    void draw(Shader& shader, glm::mat4 parentModel, float tx, float ty, float tz,
        float rotX = 0.0f, float rotY = 0.0f, float rotZ = 0.0f) {

        glm::mat4 windowBase = glm::translate(parentModel, glm::vec3(tx, ty, tz));
        windowBase = glm::rotate(windowBase, glm::radians(rotX), glm::vec3(1.0f, 0.0f, 0.0f));
        windowBase = glm::rotate(windowBase, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
        windowBase = glm::rotate(windowBase, glm::radians(rotZ), glm::vec3(0.0f, 0.0f, 1.0f));

        // Top frame
        cube.draw(shader, windowBase, 0.0f, windowHeight, 0.0f,
            0.0f, 0.0f, 0.0f,
            windowWidth, frameThickness, frameThickness, frameColor);

        // Bottom frame
        cube.draw(shader, windowBase, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f,
            windowWidth, frameThickness, frameThickness, frameColor);

        // Left frame
        cube.draw(shader, windowBase, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f,
            frameThickness, windowHeight + frameThickness, frameThickness, frameColor);

        // Right frame
        cube.draw(shader, windowBase, windowWidth - frameThickness, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f,
            frameThickness, windowHeight + frameThickness, frameThickness, frameColor);

        // Middle horizontal divider
        cube.draw(shader, windowBase, 0.0f, windowHeight / 2.0f, 0.0f,
            0.0f, 0.0f, 0.0f,
            windowWidth, frameThickness / 2.0f, frameThickness, frameColor);

        // Middle vertical divider
        cube.draw(shader, windowBase, windowWidth / 2.0f - frameThickness / 4.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f,
            frameThickness / 2.0f, windowHeight + frameThickness, frameThickness, frameColor);

        // Glass panes (4 panes)
        float paneWidth = (windowWidth - frameThickness * 2.0f - frameThickness / 2.0f) / 2.0f;
        float paneHeight = (windowHeight - frameThickness / 2.0f) / 2.0f;

        // Top-left pane
        cube.draw(shader, windowBase, frameThickness, windowHeight / 2.0f + frameThickness / 4.0f, -glassThickness / 2.0f,
            0.0f, 0.0f, 0.0f,
            paneWidth, paneHeight, glassThickness, glassColor);

        // Top-right pane
        cube.draw(shader, windowBase, frameThickness + paneWidth + frameThickness / 2.0f, windowHeight / 2.0f + frameThickness / 4.0f, -glassThickness / 2.0f,
            0.0f, 0.0f, 0.0f,
            paneWidth, paneHeight, glassThickness, glassColor);

        // Bottom-left pane
        cube.draw(shader, windowBase, frameThickness, frameThickness, -glassThickness / 2.0f,
            0.0f, 0.0f, 0.0f,
            paneWidth, paneHeight, glassThickness, glassColor);

        // Bottom-right pane
        cube.draw(shader, windowBase, frameThickness + paneWidth + frameThickness / 2.0f, frameThickness, -glassThickness / 2.0f,
            0.0f, 0.0f, 0.0f,
            paneWidth, paneHeight, glassThickness, glassColor);
    }

    // Simplified draw with position vector
    void draw(Shader& shader, glm::vec3 position,
        float rotX = 0.0f, float rotY = 0.0f, float rotZ = 0.0f) {
        glm::mat4 identity = glm::mat4(1.0f);
        draw(shader, identity, position.x, position.y, position.z, rotX, rotY, rotZ);
    }
};

#endif
