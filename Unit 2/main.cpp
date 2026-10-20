#include <iostream>
#include <vector>
#include <cmath>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

const unsigned int SCR_WIDTH = 600;
const unsigned int SCR_HEIGHT = 600;

struct Particle {
    float x, y;
    float oldX, oldY;
    bool pinned;
    bool isWheel; // Used to prevent chain dragging
};

struct Stick {
    int p1, p2;
    float length;
};

// Global state for mouse interactions
int draggedIdx = -1;
double mouseNDC_X = 0.0, mouseNDC_Y = 0.0;

// Shaders
const char* vertexShaderSource = R"glsl(
    #version 330 core
    layout (location = 0) in vec2 aPos;
    void main() {
        gl_Position = vec4(aPos, 0.0, 1.0);
    }
)glsl";

const char* fragmentShaderSource = R"glsl(
    #version 330 core
    out vec4 FragColor;
    void main() {
        vec2 circCoord = gl_PointCoord - vec2(0.5);
        if (dot(circCoord, circCoord) > 0.25) {
            discard;
        }
        FragColor = vec4(1.0, 1.0, 0.0, 1.0); // Yellow
    }
)glsl";

const char* lineFragmentShaderSource = R"glsl(
    #version 330 core
    out vec4 FragColor;
    void main() {
        FragColor = vec4(0.31, 0.76, 0.97, 1.0); // Light Blue
    }
)glsl";

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// Track and transform screen mouse positions into NDC (-1.0 to 1.0)
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    mouseNDC_X = (2.0 * xpos) / SCR_WIDTH - 1.0;
    mouseNDC_Y = 1.0 - (2.0 * ypos) / SCR_HEIGHT; // Flip Y axis
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Unit II - Bartulin", NULL, NULL);
    if (!window) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_PROGRAM_POINT_SIZE);
    glLineWidth(2.0f);

    // Compile Shaders
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    unsigned int pointProgram = glCreateProgram();
    glAttachShader(pointProgram, vertexShader);
    glAttachShader(pointProgram, fragmentShader);
    glLinkProgram(pointProgram);

    unsigned int lineFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(lineFragmentShader, 1, &lineFragmentShaderSource, NULL);
    glCompileShader(lineFragmentShader);

    unsigned int lineProgram = glCreateProgram();
    glAttachShader(lineProgram, vertexShader);
    glAttachShader(lineProgram, lineFragmentShader);
    glLinkProgram(lineProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glDeleteShader(lineFragmentShader);

    // --- Simulation Setup ---
    const float gravity = -1.8f;
    const float dt = 1.0f / 60.0f;
    const float radius = 0.03f;
    const float floorY = -1.0f + radius;

    std::vector<Particle> particles;
    std::vector<Stick> sticks;

    // 1. Chain: 5 points (Indices: 0 - 4)
    for (int i = 0; i < 5; ++i) {
        float x = -0.5f + 0.1f * i;
        float y = 0.8f;
        particles.push_back({ x, y, x, y, (i == 0), false });
    }
    for (int i = 0; i < 4; ++i) {
        sticks.push_back({ i, i + 1, 0.1f });
    }

    // 2. Wheel Hub: 1 point (Index: 5)
    float hubX = 0.5f;
    float hubY = 0.2f;
    particles.push_back({ hubX, hubY, hubX, hubY, false, true });
    int hubIndex = 5;

    // 3. Wheel Rim: 8 points (Indices: 6 - 13)
    const float PI = 3.14159265359f;
    float rimDistance = 0.15f;
    for (int i = 0; i < 8; ++i) {
        float angleRad = (i * 45.0f) * (PI / 180.0f);
        float x = hubX + rimDistance * std::cos(angleRad);
        float y = hubY + rimDistance * std::sin(angleRad);
        particles.push_back({ x, y, x, y, false, true });
    }

    float rimStickLength = 2.0f * rimDistance * std::sin((45.0f * (PI / 180.0f)) / 2.0f);

    // Wheel Structural Sticks
    for (int i = 0; i < 8; ++i) {
        int currentRimIndex = 6 + i;
        int nextRimIndex = 6 + ((i + 1) % 8);
        sticks.push_back({ currentRimIndex, nextRimIndex, rimStickLength });
        sticks.push_back({ hubIndex, currentRimIndex, rimDistance });
    }

    // --- GPU Buffer Management ---
    unsigned int pointVAO, pointVBO;
    glGenVertexArrays(1, &pointVAO);
    glGenBuffers(1, &pointVBO);
    glBindVertexArray(pointVAO);
    glBindBuffer(GL_ARRAY_BUFFER, pointVBO);
    glBufferData(GL_ARRAY_BUFFER, particles.size() * 2 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    unsigned int lineVAO, lineVBO;
    glGenVertexArrays(1, &lineVAO);
    glGenBuffers(1, &lineVBO);
    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    glBufferData(GL_ARRAY_BUFFER, sticks.size() * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    float pointSize = 0.03f * SCR_WIDTH;

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // --- Mouse Input Processing ---
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            if (draggedIdx == -1) {
                // Find nearest wheel point inside selection radius
                float nearestDist = 0.15f;
                for (size_t i = 0; i < particles.size(); ++i) {
                    if (!particles[i].isWheel) continue; // Skip chain points completely

                    float dx = particles[i].x - (float)mouseNDC_X;
                    float dy = particles[i].y - (float)mouseNDC_Y;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist < nearestDist) {
                        nearestDist = dist;
                        draggedIdx = (int)i;
                    }
                }
            }
            // Bind the active element directly to cursor tracking data
            if (draggedIdx != -1) {
                particles[draggedIdx].x = (float)mouseNDC_X;
                particles[draggedIdx].y = (float)mouseNDC_Y;
                particles[draggedIdx].oldX = (float)mouseNDC_X;
                particles[draggedIdx].oldY = (float)mouseNDC_Y;
            }
        }
        else {
            draggedIdx = -1; // Release lock when button is let go
        }

        // --- 1. Physics Step (Verlet Integration) ---
        for (size_t i = 0; i < particles.size(); ++i) {
            if (particles[i].pinned || (int)i == draggedIdx) continue;

            float tempX = particles[i].x;
            float tempY = particles[i].y;

            particles[i].x = 2.0f * particles[i].x - particles[i].oldX;
            particles[i].y = 2.0f * particles[i].y - particles[i].oldY + gravity * dt * dt;

            particles[i].oldX = tempX;
            particles[i].oldY = tempY;
        }

        // --- 2. Constraint Relaxation (5 passes) ---
        for (int step = 0; step < 5; ++step) {
            for (const auto& s : sticks) {
                Particle& p1 = particles[s.p1];
                Particle& p2 = particles[s.p2];

                float dx = p2.x - p1.x;
                float dy = p2.y - p1.y;
                float distance = std::sqrt(dx * dx + dy * dy);
                if (distance == 0.0f) distance = 0.0001f;

                float difference = s.length - distance;
                float percent = (difference / distance) * 0.5f;
                float offsetX = dx * percent;
                float offsetY = dy * percent;

                // Adjust points if they aren't directly locked down by pinning or dragging actions
                if (!p1.pinned && s.p1 != draggedIdx) { p1.x -= offsetX; p1.y -= offsetY; }
                if (!p2.pinned && s.p2 != draggedIdx) { p2.x += offsetX; p2.y += offsetY; }
            }

            // Floor Clamp
            for (size_t i = 0; i < particles.size(); ++i) {
                if (particles[i].pinned || (int)i == draggedIdx) continue;
                if (particles[i].y < floorY) {
                    particles[i].y = floorY;
                    particles[i].oldY = floorY;
                }
            }
        }

        // --- 3. Update Graphics Buffer Maps ---
        std::vector<float> rawPoints;
        for (const auto& p : particles) {
            rawPoints.push_back(p.x);
            rawPoints.push_back(p.y);
        }

        std::vector<float> rawLines;
        for (const auto& s : sticks) {
            rawLines.push_back(particles[s.p1].x);
            rawLines.push_back(particles[s.p1].y);
            rawLines.push_back(particles[s.p2].x);
            rawLines.push_back(particles[s.p2].y);
        }

        glBindBuffer(GL_ARRAY_BUFFER, pointVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, rawPoints.size() * sizeof(float), rawPoints.data());

        glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, rawLines.size() * sizeof(float), rawLines.data());

        // --- 4. Draw Command Pipeline ---
        glClearColor(0.20f, 0.10f, 0.35f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(lineProgram);
        glBindVertexArray(lineVAO);
        glDrawArrays(GL_LINES, 0, (int)sticks.size() * 2);
        glUseProgram(pointProgram);
        glPointSize(pointSize);
        glBindVertexArray(pointVAO);
        glDrawArrays(GL_POINTS, 0, (int)particles.size());
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glDeleteVertexArrays(1, &pointVAO);
    glDeleteBuffers(1, &pointVBO);
    glDeleteVertexArrays(1, &lineVAO);
    glDeleteBuffers(1, &lineVBO);
    glDeleteProgram(pointProgram);
    glDeleteProgram(lineProgram);
    glfwTerminate();
    return 0;
}