#include <algorithm>
#include <ctime>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/trigonometric.hpp>
#include <iostream>
#include <vector>
#include <cmath>

// Window dimenstions
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 720;

// Shaders 
const char* vertexShaderSource = R"(
#version 330 core 
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
uniform mat4  model;
uniform mat4 view;
uniform mat4 projection;
out vec3 Normal;
out vec3 FragPos;
void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core 
out vec4 FragColor;
uniform vec3 objectColor;
uniform vec3 lightPos;
uniform vec3 viewPos;
in vec3 Normal;
in vec3 FragPos;

void main() {
    // Simple lighting to make 3D shapes visible. Will come back later
    float ambient = 0.3f;
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);

    // Fake specilar 
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

    vec3 result = (ambient + diff + spec * 0.5) * objectColor;
    FragColor = vec4(result, 1.0);
}
)";

// Generates vertices for a cuboid (size 1x1x1, centered at origin)
std::vector<float> getCubeVertices() {
    return {
        // Positions          // Normals
        // Back face
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        // Front face
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        // Left face
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        // Right face
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
        // Bottom face
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
        // Top face
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
    };
}

// Generates a UV sphere (radius 0.5, centered at origin)
std::vector<float> getSphereVertices(int sectors, int stacks) {
    std::vector<float> vertices;
    float radius = 0.5f;
    for (int i = 0; i <= stacks; ++i) {
        float v = i / (float)stacks;
        float phi = v * glm::pi<float>();
        for (int j = 0; j <= sectors; ++j) {
            float u = j / float(sectors);
            float theta = u * (glm::pi<float>() * 2);

            float x = cos(theta) * sin(phi);
            float y = cos(phi);
            float z = sin(theta) * sin(phi);

            // Positions 
            vertices.push_back(x * radius);
            vertices.push_back(y * radius);
            vertices.push_back(z * radius);
            // Normals
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
        }
    }
    return vertices;
}

void drawGrid(unsigned int shaderProgram) {
    static unsigned int gridVAO = 0, gridVBO = 0;
    static bool initialized = false;
    static GLsizei gridVertexCount = 0;

    if (!initialized) {
        std::vector<float> gridVerts;
        const int half = 50;          // grid extends from -10 to +10
        const float spacing = 1.0f;
        //static GLsizei gridVertexCount = 0;

        for (int i = -half; i <= half; ++i) {
            float p = i * spacing;
            // Line along X axis
            gridVerts.insert(gridVerts.end(), {
                -half * spacing, 0.0f, p,   0,1,0,
                 half * spacing, 0.0f, p,   0,1,0
            });
            // Line along Z axis
            gridVerts.insert(gridVerts.end(), {
                p, 0.0f, -half * spacing,   0,1,0,
                p, 0.0f,  half * spacing,   0,1,0
            });
        }

        gridVertexCount = static_cast<GLsizei>(gridVerts.size() / 6);

        glGenVertexArrays(1, &gridVAO);
        glGenBuffers(1, &gridVBO);
        glBindVertexArray(gridVAO);
        glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
        glBufferData(GL_ARRAY_BUFFER, gridVerts.size() * sizeof(float),
                     gridVerts.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                              (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
        initialized = true;
    }

    glm::mat4 model = glm::mat4(1.0f); // identity, vertices already in world space
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE,
                       glm::value_ptr(model));
    glm::vec3 gridColor(0.25f, 0.25f, 0.25f);
    glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1,
                 glm::value_ptr(gridColor));

    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, gridVertexCount);  // (2*half+1) lines × 2 directions × 2 verts
    glBindVertexArray(0);
}

std::vector<unsigned int> getSphereIndices(int sectors, int stacks) {
    std::vector<unsigned int> indices;
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < sectors; ++j) {
            int first = i * (sectors + 1) + j;
            int second = first + sectors + 1;
            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);
            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }
    return indices;
}

void setupMesh(unsigned int& VAO, unsigned int& VBO, unsigned int& EBO, const std::vector<float>& vertices, const std::vector<unsigned int>& indices = {}) {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    if (!indices.empty()) {
        glGenBuffers(1, &EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    }

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void shaderInfoLog(unsigned int shader,const std::string& type ,unsigned int status) {
    int ok; char info[512];
    if (type == "VERTEX" || type == "FRAGMENT") {
        glGetShaderiv(shader, status, &ok);
        if (!ok) {
            glGetShaderInfoLog(shader, 512, NULL, info);
            std::cout << type << "COMPILATION::ERROR" << info << std::endl;
        }
    }

    if (type == "PROGRAM") {
        glGetProgramiv(shader, status, &ok);
        if (!ok) {
            glGetProgramInfoLog(shader, 512, NULL, info);
            std::cout << type << "LINKING:: ERROR" << info << std::endl;
        }
    } 
}

void framebuffer_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
}

unsigned int createShaderProgram() {

    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSource, NULL);
    glCompileShader(vs);
    shaderInfoLog(vs, "VERTEX", GL_COMPILE_STATUS);

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSource, NULL);
    glCompileShader(fs);
    shaderInfoLog(fs, "FRAGMENT", GL_COMPILE_STATUS);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);
    shaderInfoLog(shaderProgram, "PROGRAM", GL_LINK_STATUS);

    glDeleteShader(vs);
    glDeleteShader(fs);
    return shaderProgram;
}

// Draw cuboid 
void drawCuboid(unsigned int shaderProgram, unsigned int VAO, glm::mat4 parentModel,
        glm::vec3 localPos, glm::vec3 scale, glm::vec3 rotion, glm::vec3 color) {
    glm::mat4 model = glm::translate(parentModel, localPos);
    model = glm::rotate(model, rotion.x, glm::vec3(1,0,0));
    model = glm::rotate(model, rotion.y, glm::vec3(0,1,0));
    model = glm::rotate(model, rotion.z, glm::vec3(0,0,1));
    model = glm::scale(model, scale);

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"),1, glm::value_ptr(color));
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void drawSphere(unsigned int shaderProgram, unsigned int VAO, GLsizei indexCount ,glm::mat4 parentModel,
        glm::vec3 localPos, float radius, glm::vec3 color) {
    std::cout << "Entered drawSphere() and translating the parent model" << std::endl;
    glm::mat4 model = glm::translate(parentModel, localPos);
    std::cout << "Finished translating parentModel and now scaling." << std::endl;
    model = glm::scale(model, glm::vec3(radius * 2.0f)); // Sphere base 
    std::cout << "Finished Scaling the model" << std::endl;

    std::cout << "Setting the model uniform object using glUniformMatrix4fv" << std::endl;
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    std::cout << "Finished setting up model and now setting up object colot" << std::endl;
    glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, glm::value_ptr(color));
    std::cout << "Binding the ArrayBuffer using glBindVertexArray(VAO)" << std::endl;
    glBindVertexArray(VAO);
    std::cout << "Done calling glBindVertexArray and now callinf glDrawElements()" << std::endl;
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    std::cout << "Done calling glDrawElements and niw unbinding." << std::endl; 
    glBindVertexArray(0);
    std::cout << "Done unbinding." << std::endl;
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Articulated Robot", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    glfwSetFramebufferSizeCallback(window, framebuffer_callback);
    
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to load GLAD." << std::endl;
        return -1;
    }

    //GL_ENABLE(GL_DEPTH_TEST);
    //GL_ENABLE(GL_LINE_SMOOTH);

    unsigned int shaderProgram = createShaderProgram();
    glUseProgram(shaderProgram);

    // Setup Geometry 
    unsigned int cubeVAO, cubeVBO, cubeEBO;
    setupMesh(cubeVAO, cubeVBO, cubeEBO, getCubeVertices());

    unsigned int sphereVAO, sphereVBO, sphereEBO;
    std::vector<unsigned int> sIndices = getSphereIndices(32, 16);
    setupMesh(sphereVAO, sphereVBO, sphereEBO, getSphereVertices(32, 16), sIndices);

    // Setup Camera 
    //glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 1.5f, 6.0f), glm::vec3(0.0f, 1.5f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::vec3 camPos = glm::vec3(0.0f, 5.0f, 10.0f);
    glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // lighting
    glm::vec3 lightPos = glm::vec3(5.0f, 5.0f, 5.0f);
    //glm::vec3 viewPos = glm::vec3( 0.0f, 1.5f, 6.0f);
    glm::vec3 viewPos = camPos;
    glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
    glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(viewPos));

    // Animation variables 
    float time = 0.0f;
    glm::vec3 white(0.9f, 0.9f, 0.9f);
    GLsizei sphereIndexCount = (GLsizei)sIndices.size();

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }
        time += 0.016f; // Approx 60 FPS 

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_LINE_SMOOTH);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        drawGrid(shaderProgram);

        //GLsizei sphereIndexCount = (GLsizei)sIndices.size();

              // 1. ROOT (Pelvis/Hip Center)
        // Bobbing motion for "person-like" feel
        float bob = sin(time * 2.0f) * 0.05f;
        glm::mat4 rootTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f + bob, 0.0f));
        
        drawSphere(shaderProgram, sphereVAO, sphereIndexCount,rootTransform, glm::vec3(0,0,0), 0.25f, white);

        // 2. TORSO
        glm::mat4 torso = glm::translate(rootTransform, glm::vec3(0.0f, 0.3f, 0.0f));
        torso = glm::rotate(torso, (float)sin(time * 1.5f) * 0.1f, glm::vec3(0,0,1)); // slight sway
        // Spine
        drawCuboid(shaderProgram, cubeVAO, torso, glm::vec3(0.0f, 0.2f, 0.0f), glm::vec3(0.2f, 0.4f, 0.15f), glm::vec3(0), white);
        drawSphere(shaderProgram, sphereVAO, sphereIndexCount, torso, glm::vec3(0.0f, 0.4f, 0.0f), 0.15f, white); // Neck joint

        // 3. HEAD
        glm::mat4 head = glm::translate(torso, glm::vec3(0.0f, 0.5f, 0.0f));
        head = glm::rotate(head, (float)sin(time) * 0.2f, glm::vec3(0,1,0)); // Look around
        drawCuboid(shaderProgram, cubeVAO, head, glm::vec3(0.0f, 0.1f, 0.0f), glm::vec3(0.2f, 0.2f, 0.2f), glm::vec3(0), white);

        // 4. ARMS
        // Shoulder Joints
        drawSphere(shaderProgram, sphereVAO, sphereIndexCount, torso, glm::vec3(-0.2f, 0.35f, 0.0f), 0.08f, white);
        drawSphere(shaderProgram, sphereVAO, sphereIndexCount, torso, glm::vec3( 0.2f, 0.35f, 0.0f), 0.08f, white);

        // Left Arm
        glm::mat4 leftShoulder = glm::translate(torso, glm::vec3(-0.2f, 0.35f, 0.0f));
        // Arm swing animation
        float armSwingL = sin(time * 2.0f) * 0.5f;
        leftShoulder = glm::rotate(leftShoulder, armSwingL, glm::vec3(1,0,0));
        leftShoulder = glm::rotate(leftShoulder, -0.2f, glm::vec3(0,0,1)); // slight outward tilt
        drawCuboid(shaderProgram, cubeVAO, leftShoulder, glm::vec3(0.0f, -0.2f, 0.0f), glm::vec3(0.1f, 0.4f, 0.1f), glm::vec3(0), white);
        // Elbow
        drawSphere(shaderProgram, sphereVAO, sphereIndexCount, leftShoulder, glm::vec3(0.0f, -0.4f, 0.0f), 0.07f, white);
        glm::mat4 leftElbow = glm::translate(leftShoulder, glm::vec3(0.0f, -0.4f, 0.0f));
        leftElbow = glm::rotate(leftElbow, std::max(0.0f, armSwingL), glm::vec3(1,0,0)); // Forearm follows up
        drawCuboid(shaderProgram, cubeVAO, leftElbow, glm::vec3(0.0f, -0.2f, 0.0f), glm::vec3(0.08f, 0.4f, 0.08f), glm::vec3(0), white);
        // Hand
        drawCuboid(shaderProgram, cubeVAO, leftElbow, glm::vec3(0.0f, -0.45f, 0.0f), glm::vec3(0.1f, 0.1f, 0.1f), glm::vec3(0), white);

        // Right Arm
        glm::mat4 rightShoulder = glm::translate(torso, glm::vec3(0.2f, 0.35f, 0.0f));
        float armSwingR = -sin(time * 2.0f) * 0.5f;
        rightShoulder = glm::rotate(rightShoulder, armSwingR, glm::vec3(1,0,0));
        rightShoulder = glm::rotate(rightShoulder, 0.2f, glm::vec3(0,0,1));
        drawCuboid(shaderProgram, cubeVAO, rightShoulder, glm::vec3(0.0f, -0.2f, 0.0f), glm::vec3(0.1f, 0.4f, 0.1f), glm::vec3(0), white);
        // Elbow
        drawSphere(shaderProgram, sphereVAO, sphereIndexCount,rightShoulder, glm::vec3(0.0f, -0.4f, 0.0f), 0.07f, white);
        glm::mat4 rightElbow = glm::translate(rightShoulder, glm::vec3(0.0f, -0.4f, 0.0f));
        rightElbow = glm::rotate(rightElbow, std::max(0.0f, armSwingR), glm::vec3(1,0,0));
        drawCuboid(shaderProgram, cubeVAO, rightElbow, glm::vec3(0.0f, -0.2f, 0.0f), glm::vec3(0.08f, 0.4f, 0.08f), glm::vec3(0), white);
        // Hand
        drawCuboid(shaderProgram, cubeVAO, rightElbow, glm::vec3(0.0f, -0.45f, 0.0f), glm::vec3(0.1f, 0.1f, 0.1f), glm::vec3(0), white);

        // 5. LEGS
        // Hip Joints
        drawSphere(shaderProgram, sphereVAO, sphereIndexCount, rootTransform, glm::vec3(-0.15f, -0.1f, 0.0f), 0.1f, white);
        drawSphere(shaderProgram, sphereVAO, sphereIndexCount, rootTransform, glm::vec3( 0.15f, -0.1f, 0.0f), 0.1f, white);

        // Left Leg
        glm::mat4 leftHip = glm::translate(rootTransform, glm::vec3(-0.15f, -0.1f, 0.0f));
        float legSwingL = sin(time * 2.0f) * 0.3f;
        leftHip = glm::rotate(leftHip, legSwingL, glm::vec3(1,0,0));
        drawCuboid(shaderProgram, cubeVAO, leftHip, glm::vec3(0.0f, -0.25f, 0.0f), glm::vec3(0.15f, 0.5f, 0.15f), glm::vec3(0), white);
        // Knee
        drawSphere(shaderProgram, sphereVAO, sphereIndexCount, leftHip, glm::vec3(0.0f, -0.5f, 0.0f), 0.1f, white);
        glm::mat4 leftKnee = glm::translate(leftHip, glm::vec3(0.0f, -0.5f, 0.0f));
        leftKnee = glm::rotate(leftKnee, std::max(0.0f, -legSwingL), glm::vec3(1,0,0));
        drawCuboid(shaderProgram, cubeVAO, leftKnee, glm::vec3(0.0f, -0.25f, 0.0f), glm::vec3(0.12f, 0.5f, 0.12f), glm::vec3(0), white);
        // Foot
        drawCuboid(shaderProgram, cubeVAO, leftKnee, glm::vec3(0.0f, -0.55f, 0.05f), glm::vec3(0.15f, 0.1f, 0.25f), glm::vec3(0), white);

        // Right Leg
        glm::mat4 rightHip = glm::translate(rootTransform, glm::vec3(0.15f, -0.1f, 0.0f));
        float legSwingR = -sin(time * 2.0f) * 0.3f;
        rightHip = glm::rotate(rightHip, legSwingR, glm::vec3(1,0,0));
        drawCuboid(shaderProgram, cubeVAO, rightHip, glm::vec3(0.0f, -0.25f, 0.0f), glm::vec3(0.15f, 0.5f, 0.15f), glm::vec3(0), white);
        // Knee
        drawSphere(shaderProgram, sphereVAO, sphereIndexCount, rightHip, glm::vec3(0.0f, -0.5f, 0.0f), 0.1f, white);
        glm::mat4 rightKnee = glm::translate(rightHip, glm::vec3(0.0f, -0.5f, 0.0f));
        rightKnee = glm::rotate(rightKnee, std::max(0.0f, -legSwingR), glm::vec3(1,0,0));
        drawCuboid(shaderProgram, cubeVAO, rightKnee, glm::vec3(0.0f, -0.25f, 0.0f), glm::vec3(0.12f, 0.5f, 0.12f), glm::vec3(0), white);
        // Foot
        drawCuboid(shaderProgram, cubeVAO, rightKnee, glm::vec3(0.0f, -0.55f, 0.05f), glm::vec3(0.15f, 0.1f, 0.25f), glm::vec3(0), white);


        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}


