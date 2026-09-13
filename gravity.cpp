#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <random>
#include <sys/resource.h>
#include <vector>

//shaders
const char* vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "uniform vec2 position;\n"
    "uniform vec2 scale;\n"
    "void main() \n"
    "{\n"
    "   gl_Position = vec4(aPos.x * scale.x + position.x, aPos.y * scale.y + position.y, aPos.z, 1.0);\n"
    "}\0";

const char* fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform vec3 circleColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = vec4(circleColor, 1.0f);\n"
    "}\n\0";

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

int screenWidth = 800;
int screenHeight = 600;

struct Circle {
    glm::vec2 position;
    glm::vec2 velocity;
    float radius;
    glm::vec3 color;
};

std::vector<GLfloat> CreateCircleVertices(int res){
    std::vector<GLfloat> circleVertices = { 0.0f, 0.0f, 0.0f };
    for (int i = 0; i <= res; ++i) {
        float angle = 2.0f * 3.1415926f * (static_cast<float>(i) / res);
        float x = std::cos(angle);
        float y = std::sin(angle);
        circleVertices.push_back(x);
        circleVertices.push_back(y);
        circleVertices.push_back(0.0f);
    }
    return circleVertices;
}

double GetProcessCpuSeconds() {
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1000000.0
        + usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1000000.0;
}

void ResolveCircleCollision(Circle& first, Circle& second) {
    glm::vec2 delta = second.position - first.position;
    float distance = glm::length(delta);
    float minimumDistance = first.radius + second.radius;

    if (distance <= 0.0f || distance >= minimumDistance) return;

    glm::vec2 normal = delta / distance;
    float overlap = minimumDistance - distance;
    first.position -= normal * (overlap * 0.5f);
    second.position += normal * (overlap * 0.5f);

    glm::vec2 relativeVelocity = second.velocity - first.velocity;
    float velocityAlongNormal = glm::dot(relativeVelocity, normal);
    if (velocityAlongNormal >= 0.0f) return;

    const float restitution = 0.97f;
    float impulseMagnitude = -(1.0f + restitution) * velocityAlongNormal / 2.0f;
    glm::vec2 impulse = impulseMagnitude * normal;
    first.velocity -= impulse;
    second.velocity += impulse;
}

int main() {
    const float t = 50.0f; // simulation time in seconds
    const int n = 10; // number of circles
    const int circleResolution = 8;
    const float circleRadius = 15.0f;
    const glm::vec3 circleColor(1.0f, 1.0f, 1.0f);
    std::mt19937 randomEngine(std::random_device{}());
    std::uniform_real_distribution<float> velocityDistribution(-1.0f, 1.0f);
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // set OpenGL version to 3.3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); // set OpenGL version to 3.3
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // set OpenGL profile to core

    GLFWwindow* window = glfwCreateWindow(screenWidth,screenHeight,"physics",NULL,NULL); // create a window with width 800, height 800, title "physics", no monitor, no shared context
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    gladLoadGL();
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    int framebufferWidth;
    int framebufferHeight;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER); // create a vertex shader
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL); // attach
    glCompileShader(vertexShader); // compile

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER); // create a fragment shader
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL); // attach
    glCompileShader(fragmentShader); // compile

    GLuint shaderProgram = glCreateProgram(); // create a shader program
    glAttachShader(shaderProgram, vertexShader); // attach vertex shader
    glAttachShader(shaderProgram, fragmentShader); // attach fragment shader
    glLinkProgram(shaderProgram); // link the program

    glDeleteShader(vertexShader); // delete the vertex shader
    glDeleteShader(fragmentShader); // delete the fragment shader


    std::vector<GLfloat> circleVertices = CreateCircleVertices(circleResolution);

        std::vector<Circle> circles;
        circles.reserve(n);
        for (int i = 0; i < n; ++i) {
            float startX = -0.5f + i * 0.5f;
            circles.push_back({
                glm::vec2(startX, 0.8f),
                glm::vec2(velocityDistribution(randomEngine), velocityDistribution(randomEngine)),
                circleRadius,
                circleColor
            });
        }

    GLuint circleVAO, circleVBO;
    glGenVertexArrays(1, &circleVAO);
    glGenBuffers(1, &circleVBO);
    glBindVertexArray(circleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
    glBufferData(GL_ARRAY_BUFFER, circleVertices.size() * sizeof(GLfloat), circleVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);

    GLint positionLocation = glGetUniformLocation(shaderProgram, "position");
    const glm::vec2 acceleration(0.0f, -9.81f);
    const float restitution = 0.97f;
    GLint scaleLocation = glGetUniformLocation(shaderProgram, "scale");
    GLint colorLocation = glGetUniformLocation(shaderProgram, "circleColor");
    float previousTime = static_cast<float>(glfwGetTime());
    auto simulationStart = std::chrono::steady_clock::now();
    auto metricsTime = simulationStart;
    double metricsCpuSeconds = GetProcessCpuSeconds();
    double gpuEstimatePercent = 0.0;

    glClearColor(0.0f,0.0f,0.0f,1.0f); // set background color
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(window);

//------------------------------------------------- MAIN LOOP -------------------------------------------------//
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
        glClearColor(0.0f,0.0f,0.0f,1.0f); // set background color
        glClear(GL_COLOR_BUFFER_BIT);

        float currentTime = static_cast<float>(glfwGetTime());
        float dt = currentTime - previousTime;
        previousTime = currentTime;
        if (dt > 0.05f) dt = 0.05f;

        glUseProgram(shaderProgram);
        glBindVertexArray(circleVAO);
        for (Circle& circle : circles) {
            circle.velocity += acceleration * dt;
            circle.position += circle.velocity * dt;

            float radiusX = circle.radius / (screenWidth / 2.0f);
            float radiusY = circle.radius / (screenHeight / 2.0f);

            if (circle.position.x + radiusX > 1.0f) {
                circle.position.x = 1.0f - radiusX;
                circle.velocity.x = -std::abs(circle.velocity.x) * restitution;
            } else if (circle.position.x - radiusX < -1.0f) {
                circle.position.x = -1.0f + radiusX;
                circle.velocity.x = std::abs(circle.velocity.x) * restitution;
            }

            if (circle.position.y + radiusY > 1.0f) {
                circle.position.y = 1.0f - radiusY;
                circle.velocity.y = -std::abs(circle.velocity.y) * restitution;
            } else if (circle.position.y - radiusY < -1.0f) {
                circle.position.y = -1.0f + radiusY;
                circle.velocity.y = std::abs(circle.velocity.y) * restitution;
            }
        }

        for (size_t first = 0; first < circles.size(); ++first) {
            for (size_t second = first + 1; second < circles.size(); ++second) {
                glm::vec2 firstPositionPixels(
                    circles[first].position.x * screenWidth / 2.0f,
                    circles[first].position.y * screenHeight / 2.0f);
                glm::vec2 secondPositionPixels(
                    circles[second].position.x * screenWidth / 2.0f,
                    circles[second].position.y * screenHeight / 2.0f);
                glm::vec2 firstVelocityPixels(
                    circles[first].velocity.x * screenWidth / 2.0f,
                    circles[first].velocity.y * screenHeight / 2.0f);
                glm::vec2 secondVelocityPixels(
                    circles[second].velocity.x * screenWidth / 2.0f,
                    circles[second].velocity.y * screenHeight / 2.0f);

                circles[first].position = firstPositionPixels;
                circles[second].position = secondPositionPixels;
                circles[first].velocity = firstVelocityPixels;
                circles[second].velocity = secondVelocityPixels;
                ResolveCircleCollision(circles[first], circles[second]);
                circles[first].position = glm::vec2(
                    circles[first].position.x / (screenWidth / 2.0f),
                    circles[first].position.y / (screenHeight / 2.0f));
                circles[second].position = glm::vec2(
                    circles[second].position.x / (screenWidth / 2.0f),
                    circles[second].position.y / (screenHeight / 2.0f));
                circles[first].velocity = glm::vec2(
                    circles[first].velocity.x / (screenWidth / 2.0f),
                    circles[first].velocity.y / (screenHeight / 2.0f));
                circles[second].velocity = glm::vec2(
                    circles[second].velocity.x / (screenWidth / 2.0f),
                    circles[second].velocity.y / (screenHeight / 2.0f));
            }
        }

        auto renderStart = std::chrono::steady_clock::now();
        for (const Circle& circle : circles) {
            float radiusX = circle.radius / (screenWidth / 2.0f);
            float radiusY = circle.radius / (screenHeight / 2.0f);
            glUniform2f(positionLocation, circle.position.x, circle.position.y);
            glUniform2f(scaleLocation, radiusX, radiusY);
            glUniform3f(colorLocation, circle.color.r, circle.color.g, circle.color.b);
            glDrawArrays(GL_TRIANGLE_FAN, 0, static_cast<GLsizei>(circleVertices.size() / 3));
        }
        glfwSwapBuffers(window);

        auto now = std::chrono::steady_clock::now();
        float renderMilliseconds = std::chrono::duration<float, std::milli>(now - renderStart).count();
        gpuEstimatePercent = std::min(100.0, static_cast<double>(renderMilliseconds / 16.67f * 100.0f));
        if (std::chrono::duration<float>(now - metricsTime).count() >= 0.25f) {
            double cpuSeconds = GetProcessCpuSeconds();
            double wallSeconds = std::chrono::duration<double>(now - metricsTime).count();
            double cpuPercent = (cpuSeconds - metricsCpuSeconds) / wallSeconds * 100.0;
            metricsCpuSeconds = cpuSeconds;
            metricsTime = now;
            float elapsed = std::chrono::duration<float>(now - simulationStart).count();
            char title[160];
            std::snprintf(title, sizeof(title), "Physics | %.1f/%.1f s | CPU %.1f%% | GPU estimate %.1f%%",
                elapsed, t, cpuPercent, gpuEstimatePercent);
            glfwSetWindowTitle(window, title);
        }

        // Stop the simulation after t seconds.
        if (std::chrono::duration<float>(now - simulationStart).count() >= t) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

    }
//-----------------------------------------------------------------------------------------------------------//

    glDeleteVertexArrays(1, &circleVAO);
    glDeleteBuffers(1, &circleVBO);
    glDeleteProgram(shaderProgram);


    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}