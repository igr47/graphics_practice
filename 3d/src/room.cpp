#include <cmath>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_projection.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/trigonometric.hpp>
#include <iostream>
#include <vector>

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

void framebuffer_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

const char* vertexShaderSource = R"(
#version 330 core 
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core 

out vec4 FragColor;
uniform vec3 objectColor;

void main () {
    FragColor = vec4(objectColor, 1.0);
}
)";

unsigned int compileShader(unsigned int Shader, const char* source, const char* name) {
    int ok; char log[512];
    unsigned int shader = glCreateShader(Shader);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glGetShaderInfoLog(shader, 512, NULL, log);
        std::cout << name << "COMPILATION::ERROR:" << log << std::endl;
    }
    return shader;
}

// Helper to generate room wireframe lines
std::vector<float> getRoomLines(float w, float h, float d) {
    float x = w / 2.0f, y = h, z = d / 2.0f;
    std::vector<float> v = {
        // Bottom rectangle
        -x, 0, -z,  x, 0, -z,  x, 0, -z,  x, 0, z,  x, 0, z,  -x, 0, z,  -x, 0, z,  -x, 0, -z,
        // Top rectangle
        -x, y, -z,  x, y, -z,  x, y, -z,  x, y, z,  x, y, z,  -x, y, z,  -x, y, z,  -x, y, -z,
        // Vertical pillars
        -x, 0, -z, -x, y, -z,   x, 0, -z,  x, y, -z,   x, 0, z,  x, y, z,  -x, 0, z, -x, y, z
    };
    return v;
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "3D ROOM", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to load GLAD" << std::endl;
        return -1;
    }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);

    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource, "VERTEX");
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource, "FRAGMENT");

    unsigned int shaderProgram =glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);
    {
        int ok; char log[512];
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &ok);
        if (!ok) {
            glGetProgramInfoLog(shaderProgram, 512, NULL, log);
            std::cout << "SHADER::LINKING::ERROR: " << log << std::endl;
        }
    }

    glUseProgram(shaderProgram);
    int modelLoc = glGetUniformLocation(shaderProgram, "model");
    int viewLoc = glGetUniformLocation(shaderProgram, "view");
    int projLoc = glGetUniformLocation(shaderProgram, "projection");
    int colorLoc = glGetUniformLocation(shaderProgram, "objectColor");

        unsigned int VAO, VBO;

    // 1. Room Wireframe (Cube)
    std::vector<float> roomVerts = getRoomLines(4.0f, 3.0f, 6.0f); // Width, Height, Depth
    unsigned int roomVAO, roomVBO;
    glGenVertexArrays(1, &roomVAO);
    glGenBuffers(1, &roomVBO);
    glBindVertexArray(roomVAO);
    glBindBuffer(GL_ARRAY_BUFFER, roomVBO);
    glBufferData(GL_ARRAY_BUFFER, roomVerts.size() * sizeof(float), roomVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 2. Floor (Two Triangles)
    float floorVerts[] = {
        -2.0f, 0.0f, -3.0f,   2.0f, 0.0f, -3.0f,   2.0f, 0.0f, 3.0f,
        -2.0f, 0.0f, -3.0f,   2.0f, 0.0f,  3.0f,  -2.0f, 0.0f, 3.0f
    };
    unsigned int floorVAO, floorVBO;
    glGenVertexArrays(1, &floorVAO);
    glGenBuffers(1, &floorVBO);
    glBindVertexArray(floorVAO);
    glBindBuffer(GL_ARRAY_BUFFER, floorVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(floorVerts), floorVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 3. Sun (Single Point)
    float sunVert[] = { 0.0f, 2.0f, 0.0f };
    unsigned int sunVAO, sunVBO;
    glGenVertexArrays(1, &sunVAO);
    glGenBuffers(1, &sunVBO);
    glBindVertexArray(sunVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sunVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(sunVert), sunVert, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 4. Axis Gizmo (Origin: X=Red, Y=Green, Z=Blue)
    float axisVerts[] = {
        0,0,0, 1,0,0, // X
        0,0,0, 0,1,0, // Y
        0,0,0, 0,0,1  // Z
    };
    unsigned int axisVAO, axisVBO;
    glGenVertexArrays(1, &axisVAO);
    glGenBuffers(1, &axisVBO);
    glBindVertexArray(axisVAO);
    glBindBuffer(GL_ARRAY_BUFFER, axisVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(axisVerts), axisVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glm::mat4 view = glm::lookAt(
        glm::vec3(4.0f, 3.0f, 5.0f),  // Camera position
        glm::vec3(0.0f, 1.0f, 0.0f),  // Look at center
        glm::vec3(0.0f, 1.0f, 0.0f)   // Up vector
    );

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        glClearColor(0.22f, 0.22f, 0.22f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        // 1. Draw Room Wireframe (Yellow-Green)
        glUniform3f(colorLoc, 0.6f, 0.6f, 0.2f);
        glBindVertexArray(roomVAO);
        glDrawArrays(GL_LINES, 0, roomVerts.size() / 3);

        // 2. Draw Floor (Dark Blue/Grey)
        glUniform3f(colorLoc, 0.15f, 0.20f, 0.25f);
        glBindVertexArray(floorVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // 3. Draw Sun (White Point)
        glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f);
        glPointSize(15.0f); 
        glBindVertexArray(sunVAO);
        glDrawArrays(GL_POINTS, 0, 1);

        // 4. Draw Axis Gizmo
        glBindVertexArray(axisVAO);
        // X Axis (Red)
        glUniform3f(colorLoc, 1.0f, 0.0f, 0.0f);
        glDrawArrays(GL_LINES, 0, 2);
        // Y Axis (Green)
        glUniform3f(colorLoc, 0.0f, 1.0f, 0.0f);
        glDrawArrays(GL_LINES, 2, 2);
        // Z Axis (Blue)
        glUniform3f(colorLoc, 0.0f, 0.0f, 1.0f);
        glDrawArrays(GL_LINES, 4, 2);


        glfwSwapBuffers(window);
        glfwPollEvents();
    }

        // Cleanup
    glDeleteVertexArrays(1, &roomVAO);
    glDeleteVertexArrays(1, &floorVAO);
    glDeleteVertexArrays(1, &sunVAO);
    glDeleteVertexArrays(1, &axisVAO);
    glDeleteBuffers(1, &roomVBO);
    glDeleteBuffers(1, &floorVBO);
    glDeleteBuffers(1, &sunVBO);
    glDeleteBuffers(1, &axisVBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}
