#pragma once
#ifndef CUBE_H
#define CUBE_H

#include <glad/glad.h>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "shader.h"

class Cube {
public:
    glm::vec3 color;
    glm::mat4 model;

    Cube() {
        setUpCube();
        model = glm::mat4(1.0f);
        color = glm::vec3(1.0f);
    }

    ~Cube() {
        glDeleteVertexArrays(1, &cubeVAO);
        glDeleteBuffers(1, &cubeVBO);
        glDeleteBuffers(1, &cubeEBO);
    }

    // ---------------------------------------------------------------
    // Plain colour draw (unchanged signature & behaviour)
    // ---------------------------------------------------------------
    void draw(Shader& shader, glm::mat4 parentModel,
        float tx, float ty, float tz,
        float rx, float ry, float rz,
        float sx, float sy, float sz,
        glm::vec3 colorVec)
    {
        glm::mat4 m = buildModel(parentModel, tx, ty, tz, rx, ry, rz, sx, sy, sz);
        shader.setMat4("model", m);

        shader.setVec3("material.ambient", colorVec * 0.3f);
        shader.setVec3("material.diffuse", colorVec);
        shader.setVec3("material.specular", glm::vec3(0.3f));
        shader.setFloat("material.shininess", 32.0f);

        shader.setBool("useTexture", false);

        glBindVertexArray(cubeVAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    // ---------------------------------------------------------------
    // Textured draw � same transform args, just adds textureID.
    // ---------------------------------------------------------------
    void drawTextured(Shader& shader, glm::mat4 parentModel,
        float tx, float ty, float tz,
        float rx, float ry, float rz,
        float sx, float sy, float sz,
        unsigned int textureID,
        glm::vec3 tintColor = glm::vec3(1.0f))
    {
        glm::mat4 m = buildModel(parentModel, tx, ty, tz, rx, ry, rz, sx, sy, sz);
        shader.setMat4("model", m);

        shader.setVec3("material.ambient", tintColor * 0.3f);
        shader.setVec3("material.diffuse", tintColor);
        shader.setVec3("material.specular", glm::vec3(0.3f));
        shader.setFloat("material.shininess", 32.0f);

        shader.setBool("useTexture", true);
        shader.setInt("texUnit", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);

        glBindVertexArray(cubeVAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glBindTexture(GL_TEXTURE_2D, 0);
        shader.setBool("useTexture", false);
    }

private:
    unsigned int cubeVAO, cubeVBO, cubeEBO;

    glm::mat4 buildModel(glm::mat4 parent,
        float tx, float ty, float tz,
        float rx, float ry, float rz,
        float sx, float sy, float sz)
    {
        glm::mat4 t = glm::translate(parent, glm::vec3(tx, ty, tz));
        glm::mat4 rX = glm::rotate(t, glm::radians(rx), glm::vec3(1, 0, 0));
        glm::mat4 rY = glm::rotate(rX, glm::radians(ry), glm::vec3(0, 1, 0));
        glm::mat4 rZ = glm::rotate(rY, glm::radians(rz), glm::vec3(0, 0, 1));
        return        glm::scale(rZ, glm::vec3(sx, sy, sz));
    }

    void setUpCube() {
        // Vertex layout: position(3) + normal(3) + uv(2) = 8 floats per vertex
        // 4 vertices per face x 6 faces = 24 vertices (no sharing � each face has own UVs)
        float vertices[] = {
            // pos                 normal              uv
            // ---- FRONT face  (z = 0, normal -Z) ----
            0.0f, 0.0f, 0.0f,   0.0f,  0.0f, -1.0f,   0.0f, 0.0f,
            1.0f, 0.0f, 0.0f,   0.0f,  0.0f, -1.0f,   1.0f, 0.0f,
            1.0f, 1.0f, 0.0f,   0.0f,  0.0f, -1.0f,   1.0f, 1.0f,
            0.0f, 1.0f, 0.0f,   0.0f,  0.0f, -1.0f,   0.0f, 1.0f,

            // ---- BACK face   (z = 1, normal +Z) ----
            0.0f, 0.0f, 1.0f,   0.0f,  0.0f,  1.0f,   1.0f, 0.0f,
            1.0f, 0.0f, 1.0f,   0.0f,  0.0f,  1.0f,   0.0f, 0.0f,
            1.0f, 1.0f, 1.0f,   0.0f,  0.0f,  1.0f,   0.0f, 1.0f,
            0.0f, 1.0f, 1.0f,   0.0f,  0.0f,  1.0f,   1.0f, 1.0f,

            // ---- BOTTOM face (y = 0, normal -Y) ----
            0.0f, 0.0f, 0.0f,   0.0f, -1.0f,  0.0f,   0.0f, 0.0f,
            1.0f, 0.0f, 0.0f,   0.0f, -1.0f,  0.0f,   1.0f, 0.0f,
            1.0f, 0.0f, 1.0f,   0.0f, -1.0f,  0.0f,   1.0f, 1.0f,
            0.0f, 0.0f, 1.0f,   0.0f, -1.0f,  0.0f,   0.0f, 1.0f,

            // ---- TOP face    (y = 1, normal +Y) ----
            0.0f, 1.0f, 0.0f,   0.0f,  1.0f,  0.0f,   0.0f, 0.0f,
            1.0f, 1.0f, 0.0f,   0.0f,  1.0f,  0.0f,   1.0f, 0.0f,
            1.0f, 1.0f, 1.0f,   0.0f,  1.0f,  0.0f,   1.0f, 1.0f,
            0.0f, 1.0f, 1.0f,   0.0f,  1.0f,  0.0f,   0.0f, 1.0f,

            // ---- LEFT face   (x = 0, normal -X) ----
            0.0f, 0.0f, 0.0f,  -1.0f,  0.0f,  0.0f,   1.0f, 0.0f,
            0.0f, 0.0f, 1.0f,  -1.0f,  0.0f,  0.0f,   0.0f, 0.0f,
            0.0f, 1.0f, 1.0f,  -1.0f,  0.0f,  0.0f,   0.0f, 1.0f,
            0.0f, 1.0f, 0.0f,  -1.0f,  0.0f,  0.0f,   1.0f, 1.0f,

            // ---- RIGHT face  (x = 1, normal +X) ----
            1.0f, 0.0f, 0.0f,   1.0f,  0.0f,  0.0f,   0.0f, 0.0f,
            1.0f, 0.0f, 1.0f,   1.0f,  0.0f,  0.0f,   1.0f, 0.0f,
            1.0f, 1.0f, 1.0f,   1.0f,  0.0f,  0.0f,   1.0f, 1.0f,
            1.0f, 1.0f, 0.0f,   1.0f,  0.0f,  0.0f,   0.0f, 1.0f,
        };

        unsigned int indices[] = {
             0,  1,  2,   2,  3,  0,   // Front
             4,  6,  5,   6,  4,  7,   // Back
             8,  9, 10,  10, 11,  8,   // Bottom
            12, 14, 13,  14, 12, 15,   // Top
            16, 17, 18,  18, 19, 16,   // Left
            20, 22, 21,  22, 20, 23    // Right
        };

        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        glGenBuffers(1, &cubeEBO);

        glBindVertexArray(cubeVAO);

        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        const int stride = 8 * sizeof(float);

        // location 0 : position (3 floats)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(0);

        // location 1 : normal (3 floats)
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // location 2 : UV (2 floats)
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
    }
};
#endif