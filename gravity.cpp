#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>
#include <iostream>
#include <cmath>

GLFWwindow* StartGLFW();
int screenWidth = 800;
int screenHeight = 600;
float aspect = (float)screenWidth / (float)screenHeight;

// --- Shaders take center/radius/color as uniforms, geometry is a UNIT circle ---
const char* vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"       // unit circle vertex, radius 1, centered at origin
    "uniform float aspect;\n"
    "uniform vec2 center;\n"
    "uniform float radius;\n"
    "void main() {\n"
    "   vec2 worldPos = aPos * radius + center;\n"
    "   gl_Position = vec4(worldPos.x / aspect, worldPos.y, 0.0, 1.0);\n"
    "}\0";

const char* fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform vec3 circleColor;\n"
    "void main() {\n"
    "   FragColor = vec4(circleColor, 1.0);\n"
    "}\n\0";

struct Ball {
    glm::vec2 position;
    glm::vec2 velocity;
    float radius;
    glm::vec3 color;
    bool settled = false;
};

void ResolveCollision(Ball& a, Ball& b) {
    glm::vec2 delta = a.position - b.position;
    float dist = glm::length(delta);
    float minDist = a.radius + b.radius;

    if (dist < minDist && dist > 0.0f) {
        glm::vec2 normal = delta / dist;

        float overlap = minDist - dist;
        a.position += normal * (overlap * 0.5f);
        b.position -= normal * (overlap * 0.5f);

        glm::vec2 relativeVelocity = a.velocity - b.velocity;
        float velAlongNormal = glm::dot(relativeVelocity, normal);

        if (velAlongNormal < 0.0f) {
            float restitution = 1.0f;
            float impulse = -(1.0f + restitution) * velAlongNormal / 2.0f;

            a.velocity += normal * impulse;
            b.velocity -= normal * impulse;

            if (std::abs(impulse) > 0.01f) {
                a.settled = false;
                b.settled = false;
            }
        }
    }
}

// --- Builds ONE unit circle (radius 1, centered at origin), uploads once ---
unsigned int CreateUnitCircleVAO(int res, int& vertexCount) {
    std::vector<float> vertices;
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    for (int i = 0; i <= res; i++) {
        float angle = 2.0f * glm::pi<float>() * (static_cast<float>(i) / res);
        vertices.push_back(cos(angle));
        vertices.push_back(sin(angle));
    }
    vertexCount = vertices.size() / 2;

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    return VAO;
}

int main() {
    GLFWwindow* window = StartGLFW();
    if (!window) return -1;

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glUseProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // --- Cache uniform locations ONCE, not every frame ---
    int aspectLoc = glGetUniformLocation(shaderProgram, "aspect");
    int centerLoc = glGetUniformLocation(shaderProgram, "center");
    int radiusLoc = glGetUniformLocation(shaderProgram, "radius");
    int colorLoc  = glGetUniformLocation(shaderProgram, "circleColor");

    glUniform1f(aspectLoc, aspect); // constant, set once

    // --- Build the shared unit-circle geometry ONCE ---
    int res = 50;
    int vertexCount;
    unsigned int circleVAO = CreateUnitCircleVAO(res, vertexCount);

    std::vector<Ball> balls = {
        { glm::vec2(0.0f, 1.0f),  glm::vec2(2.0f, 0.0f),  0.05f, glm::vec3(1.0f, 0.3f, 0.3f) },
        { glm::vec2(-0.5f, 0.8f), glm::vec2(0.2f, 1.0f),  0.05f, glm::vec3(0.3f, 1.0f, 0.3f) },
        { glm::vec2(0.5f, 0.9f),  glm::vec2(-0.1f, 0.0f), 0.03f, glm::vec3(0.3f, 0.5f, 1.0f) },
    };

    float g_acc = 0.0f;
    float g_x = 0.0f;

    // Bind VAO and shader ONCE outside the loop — they don't change per-ball anymore
    glUseProgram(shaderProgram);
    glBindVertexArray(circleVAO);

    float lastTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        float currentTime = (float)glfwGetTime();
        float dt = currentTime - lastTime;
        lastTime = currentTime;

        // Clamp dt to avoid huge jumps if the window was dragged/paused
        if (dt > 0.05f) dt = 0.05f;

        glClear(GL_COLOR_BUFFER_BIT);

        for (auto& ball : balls) {
            if (!ball.settled) {
                ball.velocity.y += g_acc * dt;
                ball.position.y -= ball.velocity.y * dt;
                ball.velocity.x += g_x * dt;
                ball.position.x -= ball.velocity.x * dt;

                if (ball.position.y < -1.0f) {
                    ball.position.y = -1.0f;
                    ball.velocity.y *= -0.8f;
                    if (std::abs(ball.velocity.y) < 0.01f) {
                        ball.settled = true;
                        ball.velocity.y = 0.0f;
                    }
                }
                if (ball.position.x < -1.0f) {
                    ball.position.x = -1.0f;
                    ball.velocity.x *= -0.8f;
                }
                if (ball.position.x > 1.0f) {
                    ball.position.x = 1.0f;
                    ball.velocity.x *= -0.8f;
                }
            }
        }

        for (int iter = 0; iter < 4; iter++) {
            for (size_t i = 0; i < balls.size(); i++) {
                for (size_t j = i + 1; j < balls.size(); j++) {
                    ResolveCollision(balls[i], balls[j]);
                }
            }
        }

        for (auto& ball : balls) {
            glUniform2f(centerLoc, ball.position.x, ball.position.y);
            glUniform1f(radiusLoc, ball.radius);
            glUniform3f(colorLoc, ball.color.r, ball.color.g, ball.color.b);
            glDrawArrays(GL_TRIANGLE_FAN, 0, vertexCount);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

GLFWwindow* StartGLFW(){
    if (!glfwInit()) {
        std::cerr << "failed to initialize" << std::endl;
        return nullptr;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "gravity_sim", NULL, NULL);
    if (!window) {
        std::cerr << "failed to create window" << std::endl;
        return nullptr;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "failed to init GLAD" << std::endl;
        return nullptr;
    }
    return window;
}