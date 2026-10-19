// 2D Smoothed Particle Hydrodynamics (SPH) liquid with OpenGL + ImGui.
//
// Build tips:  -O2 (or Release) is essential.  Add -fopenmp (GCC/Clang) or /openmp (MSVC)
// to use all CPU cores; the #pragma omp lines are ignored if OpenMP is not enabled.
//
// Method: weakly-compressible SPH (Muller et al. 2003 style)
//   density    : rho_i = sum_j m W_poly6(r_ij)
//   pressure   : p_i   = max(0, k (rho_i - rho0))        (equation of state, stiff => ~incompressible)
//   pressure F : -m (p_i + p_j) / (2 rho_j) * grad W_spiky
//   viscosity F: mu m (v_j - v_i) / rho_j * laplacian W_viscosity
//   neighbours : uniform grid (cell size = h) built with a counting sort => O(N)

#include <cstddef>
#include <cstdlib>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

static const float PI = 3.14159265359f;

// ----------------------------------------------------------------------------
// Parameters (all exposed in the ImGui panel)
// ----------------------------------------------------------------------------
struct Params {
    int   particleCount   = 1500;
    float restDensity     = 1000.0f;  // rho0
    float stiffness       = 100.0f;   // k  (speed of sound ~ sqrt(k)); higher = less compressible
    float viscosity       = 0.5f;     // mu
    float smoothingRadius = 0.05f;    // h
    float gravity         = -2.0f;
    float restitution     = 0.3f;     // wall bounciness
    float timeStep        = 0.002f;   // fixed physics dt (keep <= ~0.4*h/sqrt(k))
    int   maxSubSteps     = 8;        // cap per frame so the app never "spirals"
    float timeScale       = 1.0f;
    float colorMaxSpeed   = 2.0f;     // speed that maps to full red
    float renderRadius    = 0.012f;   // world units
    float grabRadius      = 0.15f;
    float grabStiffness   = 400.0f;
};

// Spacing of particles at rest relative to h. ~15 neighbours inside h in 2D.
static const float kSpacingRatio = 0.45f;

// ----------------------------------------------------------------------------
// Fluid state (structure of arrays-ish, plus a uniform grid for neighbour search)
// ----------------------------------------------------------------------------
struct Fluid {
    std::vector<glm::vec2> pos, vel, acc;
    std::vector<float> rho, p;
    float mass = 1.0f;

    // grid
    std::vector<int> cellStart, cursor, cellOf, sorted;
    int gx = 1, gy = 1;

    int count() const { return (int)pos.size(); }
};

struct GrabState {
    bool active = false;
    std::vector<int> idx;
    std::vector<glm::vec2> offset;   // particle position relative to cursor at grab time
};

struct GPUParticle {
    glm::vec2 pos;
    glm::vec3 color;
};

void framebuffer_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

// ----------------------------------------------------------------------------
// SPH helpers
// ----------------------------------------------------------------------------

// Choose particle mass so that a perfect square lattice has exactly rho0 density.
// (Avoids the classic "fluid slowly expands/compresses at rest" problem.)
float computeMass(const Params& P) {
    const float h = P.smoothingRadius, h2 = h * h;
    const float s = h * kSpacingRatio;
    const float poly6 = 4.0f / (PI * std::pow(h, 8));
    float sum = 0.0f;
    for (int i = -6; i <= 6; ++i)
        for (int j = -6; j <= 6; ++j) {
            float r2 = (i * s) * (i * s) + (j * s) * (j * s);
            if (r2 < h2) { float t = h2 - r2; sum += t * t * t; }
        }
    return P.restDensity / (sum * poly6);
}

void spawnFluid(Fluid& f, GrabState& g, const Params& P, float aspect) {
    g.active = false; g.idx.clear(); g.offset.clear();

    const int n = P.particleCount;
    const float s = P.smoothingRadius * kSpacingRatio;

    int cols = (int)std::sqrt(n * 1.5f);
    cols = std::min(cols, (int)((2.0f * aspect - 0.1f) / s));
    cols = std::max(cols, 1);

    f.pos.assign(n, glm::vec2(0.0f));
    f.vel.assign(n, glm::vec2(0.0f));
    f.acc.assign(n, glm::vec2(0.0f));
    f.rho.assign(n, P.restDensity);
    f.p.assign(n, 0.0f);

    const float x0 = -aspect + 0.05f + s;
    const float y0 = -1.0f + 0.05f + s;
    for (int i = 0; i < n; ++i) {
        int cx = i % cols, cy = i / cols;
        float jx = ((float)rand() / RAND_MAX - 0.5f) * 0.05f * s;  // tiny jitter breaks symmetry
        float jy = ((float)rand() / RAND_MAX - 0.5f) * 0.05f * s;
        f.pos[i] = glm::vec2(x0 + cx * s + jx, y0 + cy * s + jy);
    }
}

// Counting-sort particles into grid cells (cell size = h). O(N), cache friendly.
void buildGrid(Fluid& f, float h, float aspect) {
    f.gx = (int)std::ceil(2.0f * aspect / h) + 1;
    f.gy = (int)std::ceil(2.0f / h) + 1;
    const int cells = f.gx * f.gy;
    const int n = f.count();

    f.cellStart.assign(cells + 1, 0);
    f.cellOf.resize(n);
    f.sorted.resize(n);

    for (int i = 0; i < n; ++i) {
        int cx = std::clamp((int)((f.pos[i].x + aspect) / h), 0, f.gx - 1);
        int cy = std::clamp((int)((f.pos[i].y + 1.0f) / h), 0, f.gy - 1);
        int c = cy * f.gx + cx;
        f.cellOf[i] = c;
        f.cellStart[c + 1]++;
    }
    for (int c = 0; c < cells; ++c) f.cellStart[c + 1] += f.cellStart[c];

    f.cursor.assign(f.cellStart.begin(), f.cellStart.end() - 1);
    for (int i = 0; i < n; ++i) f.sorted[f.cursor[f.cellOf[i]]++] = i;
}

template <class F>
inline void forNeighbors(const Fluid& f, int i, F&& fn) {
    const int cx = f.cellOf[i] % f.gx;
    const int cy = f.cellOf[i] / f.gx;
    for (int dy = -1; dy <= 1; ++dy) {
        int ny = cy + dy;
        if (ny < 0 || ny >= f.gy) continue;
        for (int dx = -1; dx <= 1; ++dx) {
            int nx = cx + dx;
            if (nx < 0 || nx >= f.gx) continue;
            int c = ny * f.gx + nx;
            for (int k = f.cellStart[c]; k < f.cellStart[c + 1]; ++k) fn(f.sorted[k]);
        }
    }
}

void stepSim(Fluid& f, const Params& P, float aspect, float dt,
             const GrabState& grab, glm::vec2 mouse) {
    const int n = f.count();
    if (n == 0) return;

    const float h = P.smoothingRadius, h2 = h * h;
    const float poly6   = 4.0f / (PI * std::pow(h, 8));
    const float spikyG  = -30.0f / (PI * std::pow(h, 5));   // gradient coefficient (negative)
    const float viscLap = 40.0f / (PI * std::pow(h, 5));    // laplacian coefficient
    const float m = f.mass;

    buildGrid(f, h, aspect);

    // 1) density + pressure (equation of state). Negative pressure is clamped to 0
    //    so particles don't clump together at the free surface.
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; ++i) {
        float d = 0.0f;
        forNeighbors(f, i, [&](int j) {
            glm::vec2 r = f.pos[i] - f.pos[j];
            float r2 = r.x * r.x + r.y * r.y;
            if (r2 < h2) { float t = h2 - r2; d += t * t * t; }   // includes self (j == i)
        });
        f.rho[i] = m * poly6 * d;
        f.p[i]   = std::max(0.0f, P.stiffness * (f.rho[i] - P.restDensity));
    }

    // 2) forces -> acceleration. Particles with more neighbours (higher pressure) push
    //    outward toward low-pressure regions, which is what equalises density.
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; ++i) {
        glm::vec2 force(0.0f);
        forNeighbors(f, i, [&](int j) {
            if (j == i) return;
            glm::vec2 rv = f.pos[i] - f.pos[j];
            float r2 = rv.x * rv.x + rv.y * rv.y;
            if (r2 >= h2 || r2 < 1e-12f) return;
            float r = std::sqrt(r2);
            float q = h - r;
            glm::vec2 dir = rv / r;

            // pressure (symmetrised)
            float pc = m * (f.p[i] + f.p[j]) / (2.0f * f.rho[j]) * spikyG * q * q;
            force -= dir * pc;

            // viscosity
            force += P.viscosity * m * (f.vel[j] - f.vel[i]) / f.rho[j] * viscLap * q;
        });
        f.acc[i] = force / f.rho[i] + glm::vec2(0.0f, P.gravity);
    }

    // 3) mouse grab: critically-damped spring pulling the picked "drop" to the cursor.
    if (grab.active) {
        const float damp = 2.0f * std::sqrt(P.grabStiffness) * 0.8f;
        for (size_t k = 0; k < grab.idx.size(); ++k) {
            int i = grab.idx[k];
            if (i >= n) continue;
            glm::vec2 target = mouse + grab.offset[k];
            f.acc[i] += P.grabStiffness * (target - f.pos[i]) - damp * f.vel[i];
        }
    }

    // 4) integrate (semi-implicit Euler) + walls
    const float pad = 0.02f;
    const float xmin = -aspect + pad, xmax = aspect - pad;
    const float ymin = -1.0f + pad,   ymax = 1.0f - pad;
    const float maxSpeed = 3.0f * std::sqrt(P.stiffness);   // safety clamp

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; ++i) {
        glm::vec2 v = f.vel[i] + f.acc[i] * dt;
        float sp = glm::length(v);
        if (sp > maxSpeed) v *= maxSpeed / sp;
        glm::vec2 x = f.pos[i] + v * dt;

        if (x.x < xmin) { x.x = xmin; v.x = -v.x * P.restitution; }
        if (x.x > xmax) { x.x = xmax; v.x = -v.x * P.restitution; }
        if (x.y < ymin) { x.y = ymin; v.y = -v.y * P.restitution; }
        if (x.y > ymax) { x.y = ymax; v.y = -v.y * P.restitution; }

        f.pos[i] = x;
        f.vel[i] = v;
    }
}

// blue (slow) -> green (middle) -> red (fast)
glm::vec3 speedColor(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const glm::vec3 blue(0.1f, 0.3f, 1.0f), green(0.1f, 0.95f, 0.4f), red(1.0f, 0.2f, 0.1f);
    if (t < 0.5f) return glm::mix(blue, green, t * 2.0f);
    return glm::mix(green, red, (t - 0.5f) * 2.0f);
}

// ----------------------------------------------------------------------------
// Shaders: particles are drawn as point sprites (one draw call for all particles)
// ----------------------------------------------------------------------------
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;

uniform mat4 projection;
uniform float pointSize;
out vec3 vColor;

void main() {
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
    gl_PointSize = pointSize;
    vColor = aColor;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;

void main() {
    vec2 c = gl_PointCoord * 2.0 - 1.0;
    float d = dot(c, c);
    if (d > 1.0) discard;
    FragColor = vec4(vColor * (1.0 - 0.25 * d), 1.0);
}
)";

static unsigned int compile(GLenum type, const char* src, const char* name) {
    unsigned int s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    int ok; char log[512];
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(s, 512, NULL, log); std::cout << name << " COMPILATION ERROR: " << log << std::endl; }
    return s;
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1000, 700, "SPH Fluid", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
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

    unsigned int vs = compile(GL_VERTEX_SHADER, vertexShaderSource, "VERTEX");
    unsigned int fs = compile(GL_FRAGMENT_SHADER, fragmentShaderSource, "FRAGMENT");
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);
    {
        int ok; char log[512];
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &ok);
        if (!ok) { glGetProgramInfoLog(shaderProgram, 512, NULL, log); std::cout << "LINK ERROR: " << log << std::endl; }
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    const GLint projLoc = glGetUniformLocation(shaderProgram, "projection");
    const GLint sizeLoc = glGetUniformLocation(shaderProgram, "pointSize");

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GPUParticle), (void*)offsetof(GPUParticle, pos));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GPUParticle), (void*)offsetof(GPUParticle, color));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    glEnable(GL_PROGRAM_POINT_SIZE);

    Params P;
    Fluid fluid;
    GrabState grab;
    std::vector<GPUParticle> gpu;

    {
        int w, h; glfwGetFramebufferSize(window, &w, &h);
        spawnFluid(fluid, grab, P, (float)w / (float)h);
    }

    float lastTime = (float)glfwGetTime();
    float accumulator = 0.0f;
    bool mouseWasDown = false;
    float simMs = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        processInput(window);

        int fbWidth, fbHeight, winW, winH;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        glfwGetWindowSize(window, &winW, &winH);
        if (fbHeight == 0 || winH == 0) { glfwWaitEvents(); continue; }
        const float aspect = (float)fbWidth / (float)fbHeight;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ---------------- UI ----------------
        bool respawn = false;
        ImGui::Begin("Controls");
        ImGui::Text("FPS: %.0f   sim: %.2f ms/frame", io.Framerate, simMs);

        ImGui::SeparatorText("Particles");
        ImGui::SliderInt("Particle count", &P.particleCount, 100, 6000);
        if (ImGui::IsItemDeactivatedAfterEdit()) respawn = true;
        ImGui::SliderFloat("Smoothing radius (h)", &P.smoothingRadius, 0.03f, 0.10f, "%.3f");
        if (ImGui::IsItemDeactivatedAfterEdit()) respawn = true;
        ImGui::SliderFloat("Render radius", &P.renderRadius, 0.004f, 0.03f, "%.3f");
        if (ImGui::Button("Reset")) respawn = true;

        ImGui::SeparatorText("Fluid");
        ImGui::SliderFloat("Rest density", &P.restDensity, 100.0f, 3000.0f, "%.0f");
        ImGui::SliderFloat("Stiffness (pressure)", &P.stiffness, 20.0f, 400.0f, "%.0f");
        ImGui::SliderFloat("Viscosity", &P.viscosity, 0.0f, 5.0f, "%.2f");
        ImGui::SliderFloat("Gravity", &P.gravity, -10.0f, 0.0f, "%.2f");
        ImGui::SliderFloat("Wall restitution", &P.restitution, 0.0f, 1.0f, "%.2f");

        ImGui::SeparatorText("Solver");
        ImGui::SliderFloat("Time step", &P.timeStep, 0.0005f, 0.004f, "%.4f");
        ImGui::SliderInt("Max substeps / frame", &P.maxSubSteps, 1, 20);
        ImGui::SliderFloat("Time scale", &P.timeScale, 0.1f, 2.0f, "%.2f");
        float cflDt = 0.4f * P.smoothingRadius / std::sqrt(P.stiffness);
        if (P.timeStep > cflDt) {
            ImGui::TextColored(ImVec4(1, 0.5f, 0.2f, 1), "dt > %.4f may be unstable (CFL)", cflDt);
        }

        ImGui::SeparatorText("Look / Mouse");
        ImGui::SliderFloat("Speed for full red", &P.colorMaxSpeed, 0.5f, 6.0f, "%.2f");
        ImGui::SliderFloat("Grab radius", &P.grabRadius, 0.05f, 0.4f, "%.2f");
        ImGui::SliderFloat("Grab strength", &P.grabStiffness, 50.0f, 1500.0f, "%.0f");

        // stats
        {
            double sumRho = 0.0; float maxRho = 0.0f, maxV = 0.0f;
            for (int i = 0; i < fluid.count(); ++i) {
                sumRho += fluid.rho[i];
                maxRho = std::max(maxRho, fluid.rho[i]);
                maxV = std::max(maxV, glm::length(fluid.vel[i]));
            }
            if (fluid.count() > 0) {
                ImGui::SeparatorText("Stats");
                ImGui::Text("Active: %d", fluid.count());
                ImGui::Text("Avg density / rest: %.3f", (float)(sumRho / fluid.count()) / P.restDensity);
                ImGui::Text("Max compression: %.1f %%", (maxRho / P.restDensity - 1.0f) * 100.0f);
                ImGui::Text("Max speed: %.2f", maxV);
            }
        }
        ImGui::End();

        if (respawn) { spawnFluid(fluid, grab, P, aspect); accumulator = 0.0f; }

        // ---------------- Mouse grab ----------------
        double mxd, myd;
        glfwGetCursorPos(window, &mxd, &myd);
        glm::vec2 mouseWorld(((float)mxd / winW * 2.0f - 1.0f) * aspect, 1.0f - (float)myd / winH * 2.0f);

        bool mouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (mouseDown && !mouseWasDown && !io.WantCaptureMouse) {
            grab.active = true;
            grab.idx.clear();
            grab.offset.clear();
            for (int i = 0; i < fluid.count(); ++i) {
                glm::vec2 d = fluid.pos[i] - mouseWorld;
                if (glm::length(d) < P.grabRadius) { grab.idx.push_back(i); grab.offset.push_back(d); }
            }
        }
        if (!mouseDown && grab.active) {      // released -> the drop falls and splashes
            grab.active = false;
            grab.idx.clear();
            grab.offset.clear();
        }
        mouseWasDown = mouseDown;

        if (!io.WantCaptureMouse) {           // ring showing the pick-up area
            float rPx = P.grabRadius * winH * 0.5f;
            ImGui::GetForegroundDrawList()->AddCircle(ImVec2((float)mxd, (float)myd), rPx,
                grab.active ? IM_COL32(255, 255, 255, 220) : IM_COL32(255, 255, 255, 80), 48, 2.0f);
        }

        // ---------------- Physics (fixed time step with accumulator) ----------------
        float now = (float)glfwGetTime();
        float frameDt = std::min(now - lastTime, 0.05f);
        lastTime = now;

        double t0 = glfwGetTime();
        fluid.mass = computeMass(P);
        accumulator += frameDt * P.timeScale;
        int steps = 0;
        while (accumulator >= P.timeStep && steps < P.maxSubSteps) {
            stepSim(fluid, P, aspect, P.timeStep, grab, mouseWorld);
            accumulator -= P.timeStep;
            ++steps;
        }
        if (steps == P.maxSubSteps) accumulator = 0.0f;   // can't keep up: slow down instead of spiralling
        simMs = (float)((glfwGetTime() - t0) * 1000.0);

        // ---------------- Render ----------------
        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        const int n = fluid.count();
        gpu.resize(n);
        const float invMax = 1.0f / std::max(P.colorMaxSpeed, 1e-4f);
        for (int i = 0; i < n; ++i) {
            gpu[i].pos = fluid.pos[i];
            gpu[i].color = speedColor(glm::length(fluid.vel[i]) * invMax);
        }

        glUseProgram(shaderProgram);
        glm::mat4 projection = glm::ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform1f(sizeLoc, std::max(2.0f, P.renderRadius * (float)fbHeight));  // diameter in pixels

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, n * sizeof(GPUParticle), gpu.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_POINTS, 0, n);
        glBindVertexArray(0);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}

void framebuffer_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

