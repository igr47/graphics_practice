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

// ---------------------------------------------------------------------------
// Mesh helper: one VAO with a triangle index buffer (filled surface)
// and a line index buffer (dark outline / segment lines).
// ---------------------------------------------------------------------------
struct Mesh {
    unsigned int VAO = 0, VBO = 0, triEBO = 0, lineEBO = 0;
    GLsizei triCount = 0, lineCount = 0;
};

Mesh createMesh(const std::vector<float>& vertices,
                const std::vector<unsigned int>& tris,
                const std::vector<unsigned int>& lines) {
    Mesh m;
    m.triCount  = (GLsizei)tris.size();
    m.lineCount = (GLsizei)lines.size();

    glGenVertexArrays(1, &m.VAO);
    glGenBuffers(1, &m.VBO);
    glGenBuffers(1, &m.triEBO);
    glGenBuffers(1, &m.lineEBO);

    glBindVertexArray(m.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.lineEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, lines.size() * sizeof(unsigned int), lines.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.triEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, tris.size() * sizeof(unsigned int), tris.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return m;
}

// Builds triangle + line indices for a grid of (rings x (sectors+1)) vertices
void gridIndices(int rings, int sectors,
                 std::vector<unsigned int>& tris, std::vector<unsigned int>& lines) {
    for (int i = 0; i < rings - 1; ++i) {
        for (int j = 0; j < sectors; ++j) {
            unsigned int first  = i * (sectors + 1) + j;
            unsigned int second = first + sectors + 1;
            tris.insert(tris.end(), { first, second, first + 1, second, second + 1, first + 1 });
            lines.insert(lines.end(), { first, second });   // lengthwise line
        }
    }
    for (int i = 0; i < rings; ++i) {
        for (int j = 0; j < sectors; ++j) {
            unsigned int a = i * (sectors + 1) + j;
            lines.insert(lines.end(), { a, a + 1 });         // ring line
        }
    }
}

// Unit-diameter sphere (radius 0.5). Scale it to get spheres / ellipsoids.
Mesh createSphereMesh(int sectors, int stacks) {
    std::vector<float> v;
    for (int i = 0; i <= stacks; ++i) {
        float phi = (i / (float)stacks) * glm::pi<float>();
        for (int j = 0; j <= sectors; ++j) {
            float theta = (j / (float)sectors) * glm::pi<float>() * 2.0f;
            float x = cos(theta) * sin(phi);
            float y = cos(phi);
            float z = sin(theta) * sin(phi);
            v.insert(v.end(), { x * 0.5f, y * 0.5f, z * 0.5f, x, y, z });
        }
    }
    std::vector<unsigned int> tris, lines;
    gridIndices(stacks + 1, sectors, tris, lines);
    return createMesh(v, tris, lines);
}

// Capsule along the Y axis, centered at origin: a cylinder with rounded ends.
// totalLength includes both end caps.
Mesh createCapsuleMesh(float radius, float totalLength, int sectors = 12, int capStacks = 4) {
    float cylHalf = std::max(0.0f, totalLength * 0.5f - radius);
    std::vector<float> v;
    int rings = 0;
    auto addRing = [&](float phi, float yOffset) {
        for (int j = 0; j <= sectors; ++j) {
            float theta = (j / (float)sectors) * glm::pi<float>() * 2.0f;
            float nx = cos(theta) * sin(phi);
            float ny = cos(phi);
            float nz = sin(theta) * sin(phi);
            v.insert(v.end(), { nx * radius, ny * radius + yOffset, nz * radius, nx, ny, nz });
        }
        rings++;
    };
    const float halfPi = glm::pi<float>() * 0.5f;
    for (int k = 0; k <= capStacks; ++k) addRing(halfPi * k / capStacks,           +cylHalf); // top cap
    for (int k = 0; k <= capStacks; ++k) addRing(halfPi + halfPi * k / capStacks,  -cylHalf); // bottom cap

    std::vector<unsigned int> tris, lines;
    gridIndices(rings, sectors, tris, lines);
    return createMesh(v, tris, lines);
}

// Unit cube (1x1x1, centered at origin) with hard edges
Mesh createCubeMesh() {
    struct Face { glm::vec3 n, u, v; };
    const Face faces[6] = {
        { { 0, 0, 1}, { 1, 0, 0}, {0, 1, 0} },
        { { 0, 0,-1}, {-1, 0, 0}, {0, 1, 0} },
        { { 1, 0, 0}, { 0, 0,-1}, {0, 1, 0} },
        { {-1, 0, 0}, { 0, 0, 1}, {0, 1, 0} },
        { { 0, 1, 0}, { 1, 0, 0}, {0, 0,-1} },
        { { 0,-1, 0}, { 1, 0, 0}, {0, 0, 1} },
    };
    std::vector<float> v;
    std::vector<unsigned int> tris, lines;
    for (int f = 0; f < 6; ++f) {
        const Face& F = faces[f];
        glm::vec3 c[4] = {
            F.n * 0.5f - F.u * 0.5f - F.v * 0.5f,
            F.n * 0.5f + F.u * 0.5f - F.v * 0.5f,
            F.n * 0.5f + F.u * 0.5f + F.v * 0.5f,
            F.n * 0.5f - F.u * 0.5f + F.v * 0.5f,
        };
        unsigned int base = f * 4;
        for (int k = 0; k < 4; ++k)
            v.insert(v.end(), { c[k].x, c[k].y, c[k].z, F.n.x, F.n.y, F.n.z });
        tris.insert(tris.end(), { base, base + 1, base + 2, base + 2, base + 3, base });
        lines.insert(lines.end(), { base, base + 1, base + 1, base + 2, base + 2, base + 3, base + 3, base });
    }
    return createMesh(v, tris, lines);
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

// Draws a mesh as a filled surface plus dark outline lines
void drawMesh(unsigned int shaderProgram, const Mesh& m, const glm::mat4& model,
              glm::vec3 fillColor, glm::vec3 lineColor) {
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glBindVertexArray(m.VAO);

    // Filled surface (pushed back slightly so the outline lines win the depth test)
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, glm::value_ptr(fillColor));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.triEBO);
    glDrawElements(GL_TRIANGLES, m.triCount, GL_UNSIGNED_INT, 0);
    glDisable(GL_POLYGON_OFFSET_FILL);

    // Outline
    glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, glm::value_ptr(lineColor));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.lineEBO);
    glDrawElements(GL_LINES, m.lineCount, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
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

    unsigned int shaderProgram = createShaderProgram();
    glUseProgram(shaderProgram);

    // ---------------- Geometry ----------------
    Mesh sphere = createSphereMesh(16, 10);
    Mesh cube   = createCubeMesh();

    // Capsule segments (radius, total length including rounded ends)
    Mesh thigh    = createCapsuleMesh(0.070f, 0.390f);
    Mesh shin     = createCapsuleMesh(0.055f, 0.370f);
    Mesh upperArm = createCapsuleMesh(0.038f, 0.250f);
    Mesh forearm  = createCapsuleMesh(0.033f, 0.220f);
    Mesh hand     = createCapsuleMesh(0.028f, 0.100f);

    // ---------------- Camera ----------------
    glm::vec3 camPos = glm::vec3(0.0f, 5.0f, 10.0f);
    glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // ---------------- Lighting ----------------
    glm::vec3 lightPos = glm::vec3(5.0f, 5.0f, 5.0f);
    glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
    glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(camPos));

    // ---------------- Colors ----------------
    const glm::vec3 white (0.85f, 0.85f, 0.85f);   // body segments
    const glm::vec3 jointC(0.12f, 0.12f, 0.12f);   // dark joint pieces
    const glm::vec3 lineC (0.03f, 0.03f, 0.03f);   // outline lines

    // Overall figure scale (the model below is built ~1.8 units tall, feet on y = 0)
    const float FIGURE_SCALE = 1.4f;

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        glEnable(GL_DEPTH_TEST);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        drawGrid(shaderProgram);

        glm::mat4 figure = glm::scale(glm::mat4(1.0f), glm::vec3(FIGURE_SCALE));

        // Small helpers --------------------------------------------------
        // Ellipsoid / sphere of the given diameters, centered at p (in parent space)
        auto ellipsoid = [&](const glm::mat4& parent, glm::vec3 p, glm::vec3 diameter,
                             glm::vec3 fill, glm::vec3 line) {
            glm::mat4 m = glm::translate(parent, p);
            m = glm::scale(m, diameter);
            drawMesh(shaderProgram, sphere, m, fill, line);
        };
        // Dark ball joint
        auto joint = [&](const glm::mat4& parent, glm::vec3 p, float diameter) {
            ellipsoid(parent, p, glm::vec3(diameter), jointC, jointC * 2.0f);
        };
        // Capsule segment hanging DOWN from a joint (centered halfway along its length)
        auto segment = [&](const Mesh& cap, const glm::mat4& jointFrame, float jointDistance) {
            drawMesh(shaderProgram, cap, glm::translate(jointFrame, glm::vec3(0.0f, -jointDistance * 0.5f, 0.0f)),
                     white, lineC);
        };

        // ---------------- TORSO (stacked spheres, like the reference) ----------------
        ellipsoid(figure, glm::vec3(0.0f, 1.00f, 0.0f), glm::vec3(0.23f, 0.20f, 0.18f), white, lineC); // pelvis
        ellipsoid(figure, glm::vec3(0.0f, 1.14f, 0.0f), glm::vec3(0.15f, 0.13f, 0.13f), white, lineC); // abdomen
        ellipsoid(figure, glm::vec3(0.0f, 1.39f, 0.0f), glm::vec3(0.34f, 0.30f, 0.22f), white, lineC); // chest

        // ---------------- NECK + HEAD ----------------
        joint(figure, glm::vec3(0.0f, 1.57f, 0.0f), 0.07f);
        ellipsoid(figure, glm::vec3(0.0f, 1.69f, 0.0f), glm::vec3(0.20f, 0.23f, 0.21f), white, lineC);

        // ---------------- ARMS (hanging at the sides) ----------------
        for (int s = -1; s <= 1; s += 2) {
            float side = (float)s;   // -1 = left, +1 = right

            // Shoulder
            glm::mat4 shoulder = glm::translate(figure, glm::vec3(side * 0.215f, 1.47f, 0.0f));
            shoulder = glm::rotate(shoulder, side * 0.08f, glm::vec3(0, 0, 1));  // slightly away from body
            joint(shoulder, glm::vec3(0.0f), 0.075f);
            segment(upperArm, shoulder, 0.28f);

            // Elbow (slight forward bend)
            glm::mat4 elbow = glm::translate(shoulder, glm::vec3(0.0f, -0.28f, 0.0f));
            elbow = glm::rotate(elbow, -0.25f, glm::vec3(1, 0, 0));
            joint(elbow, glm::vec3(0.0f), 0.060f);
            segment(forearm, elbow, 0.25f);

            // Wrist + hand
            glm::mat4 wrist = glm::translate(elbow, glm::vec3(0.0f, -0.25f, 0.0f));
            joint(wrist, glm::vec3(0.0f), 0.050f);
            segment(hand, wrist, 0.11f);
        }

        // ---------------- LEGS (straight, feet flat on the floor) ----------------
        for (int s = -1; s <= 1; s += 2) {
            float side = (float)s;

            // Hip
            glm::mat4 hip = glm::translate(figure, glm::vec3(side * 0.085f, 0.935f, 0.0f));
            joint(hip, glm::vec3(0.0f), 0.080f);
            segment(thigh, hip, 0.42f);

            // Knee
            glm::mat4 knee = glm::translate(hip, glm::vec3(0.0f, -0.42f, 0.0f));
            joint(knee, glm::vec3(0.0f), 0.070f);
            segment(shin, knee, 0.41f);

            // Ankle
            glm::mat4 ankle = glm::translate(knee, glm::vec3(0.0f, -0.41f, 0.0f));
            joint(ankle, glm::vec3(0.0f), 0.060f);

            // Foot: flat box, toes pointing +Z (toward the camera)
            glm::mat4 foot = glm::translate(ankle, glm::vec3(0.0f, -0.07f, 0.04f));
            foot = glm::scale(foot, glm::vec3(0.10f, 0.07f, 0.24f));
            drawMesh(shaderProgram, cube, foot, white, lineC);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}
