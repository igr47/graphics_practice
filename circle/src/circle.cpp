#include <cstddef>
#include <cstdlib>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cmath>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

void framebuffer_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
struct Vertex {
    glm::vec3 pos;
};

struct Ball {
    glm::vec3 pos;
    glm::vec2 velocity;
    glm::vec4 color;
    float radius; // Needed for collision radius
};

std::vector<Vertex> drawCircle(float radius, int segments);
//glm::vec3 bounce(glm::vec3 circlePos, float &velocityY, float dt);
void draw(GLuint shaderProgram, GLuint vao, glm::vec4 color, GLsizei vertexCount);
void applyPairForces(std::vector<Ball>& balls, float interactionRadius, float repulsionK, float dt);
void integateAndCollide(std::vector<Ball>& balls,float dt, float aspect, float gravity, float damping, float restitution);

const char* vertexShaderSource = R"(
#version 330 core 
layout (location = 0) in vec3 aPos;

uniform mat4 projection;
uniform vec3 offset;

void main() {
    gl_Position = projection * vec4(aPos + offset, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core 

uniform vec4 color;

out vec4 FragColor;

void main() {
    FragColor = color;
}
)";

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Circle", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize glad!" << std::endl;
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    int succes; char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &succes);
    if (!succes) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "VERTEX::SHADER::COMPILATION::ERROR: " << infoLog << std::endl;
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &succes); 
    if (!succes) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "FRAGMENT::SHADER::COMPILATION::ERROR: " << infoLog << std::endl;
    }

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &succes);
    if (!succes) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "SHADER::LINKING::ERROR: " << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    auto circleVerts = drawCircle(0.1f, 64);

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, circleVerts.size() * sizeof(Vertex), circleVerts.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    //glm::vec3 circlePos = glm::vec3(0.0f, 0.0f, 0.0f);
    //float velocityY = 0.0f;
    std::vector<Ball> balls;
    balls.push_back({ glm::vec3(0.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), 0.1f});

    bool dkeyWasPressed = false;

    int targetBallCount = 1;

    float lastTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Controlls");

        ImGui::SliderInt("BallCount", &targetBallCount, 0, 100);

        if (ImGui::Button("Reset")) targetBallCount = 1;

        ImGui::Text("Currently Active: %d", (int)balls.size());

        ImGui::End();
 
        glUseProgram(shaderProgram);

        int fbWidth, fbHeight;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        float aspect = (float)fbWidth / (float)fbHeight;

        while ((int)balls.size() < targetBallCount) {
            float randX = -aspect + 0.2f + static_cast<float>(rand()) / RAND_MAX * (2.0f * aspect -0.4f);
            glm::vec4 randColor(
                    static_cast<float>(rand()) / RAND_MAX,
                    static_cast<float>(rand()) / RAND_MAX,
                    static_cast<float>(rand()) / RAND_MAX,
                    1.0f
            );
            balls.push_back({glm::vec3(randX, 0.5f, 0.0f), glm::vec2(0.0f, 0.0f), randColor, 0.1f});
        }

        while ((int)balls.size() > targetBallCount) {
            balls.pop_back();
        }
        
        glm::mat4 projection = glm::ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        //dkeyWasPressed = dkeyPressed;

        float currentTime = (float)glfwGetTime();
        float dt = currentTime - lastTime;
        lastTime = currentTime;
        if (dt > 0.05f) dt = 0.05f;

        const float gravity = -0.8f;
        const float damping = 0.985f;
        const float restitution = 0.3f;
        const float interactionRadius = 0.22f;
        const float repulsionK = 40.0f;

        applyPairForces(balls, interactionRadius, repulsionK, dt);
        integateAndCollide(balls, dt, aspect, gravity, damping, restitution);

        for (auto& ball : balls) {
            //ball.pos = bounce(ball.pos, ball.velocityY, dt);
            glUniform3fv(glGetUniformLocation(shaderProgram, "offset"), 1, glm::value_ptr(ball.pos));
            draw(shaderProgram, VAO, ball.color, (GLsizei)circleVerts.size());
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();

    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}

void draw(GLuint shaderProgram, GLuint vao, glm::vec4 color, GLsizei vertexCount) {
    glUniform4fv(glGetUniformLocation(shaderProgram, "color"), 1, glm::value_ptr(color));
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, vertexCount);
    glBindVertexArray(0);
}

/*glm::vec3 bounce(glm::vec3 circlePos, float &velocityY , float dt) {
    const float gravity = -2.5f;
    const float floorY = -0.9f;

    const float restitution = 0.7f;

    velocityY += gravity * dt;
    circlePos.y += velocityY * dt;

    if (circlePos.y < floorY) {
        circlePos.y = floorY;
        velocityY = -velocityY * restitution;

        if(std::fabs(velocityY) < 0.1f) velocityY = 0.0f;
    }
    return circlePos;
}*/

void applyPairForces(std::vector<Ball>& balls, float interactionRadius, float repulsionK, float dt) {
    const int n = (int)balls.size();
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            glm::vec2 delta = glm::vec2(balls[j].pos - balls[i].pos);
            float dist = glm::length(delta);
            if (dist < 0.001f || dist > interactionRadius) continue;

            glm::vec2 dir = delta / dist;
            // Linear repulsion 
            float mag = repulsionK * (interactionRadius - dist);

            // Push appart(equal and opposite). Mass of 1 is assumed 
            balls[i].velocity -= dir * mag * dt;
            balls[j].velocity += dir * mag * dt;
        }
    }
}

void integateAndCollide(std::vector<Ball>& balls,float dt, float aspect, float gravity, float damping, float restitution) {
    const float floorY = -0.9f;
    const float cellingY = 0.9f;
    float leftX = -aspect + 0.1f;
    float rightX = aspect - 0.1f;

    for (auto& b : balls) {
        // gravity 
        b.velocity.y += gravity * dt;

        // Dampint (mild to prevent liquid from boiling forever)
        b.velocity *= damping;

        // Integrate position 
        b.pos.x += b.velocity.x * dt;
        b.pos.y += b.velocity.y * dt;

        // Floor 
        if (b.pos.y < floorY) {
            b.pos.y = floorY;
            b.velocity.y = -b.velocity.y * restitution;
            if (std::fabs(b.velocity.y) < 0.05f) b.velocity.y = 0.0f;
        }

        // Celling 
        if (b.pos.y > cellingY) {
            b.pos.y = cellingY;
            b.velocity.y = -b.velocity.y * restitution;
        }

        // Left wall 
        if (b.pos.x < leftX) {
            b.pos.x = leftX;
            b.velocity.x = -b.velocity.x * restitution;
        }

        // Right wall 
        if (b.pos.x > rightX) {
            b.pos.x = rightX;
            b.velocity.x = -b.velocity.x * restitution;
        }
    }
}

void framebuffer_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}


std::vector<Vertex> drawCircle(float radius, int segments) {
    std::vector<Vertex> verts;

    verts.reserve(segments + 2);

    verts.push_back({glm::vec3(0.0f, 0.0f, 0.0f)});

    for (int i = 0; i <= segments; ++i) {
        float theta = 2 * 3.14159265f * float(i) / float(segments);
        float x = radius * cos(theta);
        float y = radius * sin(theta);

        verts.push_back({ glm::vec3(x, y, 0.0f)});
    }

    return verts;
}
