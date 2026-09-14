#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
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

int screenWidth = 1200;
int screenHeight = 800;

struct Circle {
    glm::vec2 position;
    glm::vec2 velocity;
    float radius;
    float mass;
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
    glm::vec2 distance = second.position - first.position;
    float distanceMagnitude = glm::length(distance);
    float minimumDistance = first.radius + second.radius;

    if (distanceMagnitude <= 0.0f || distanceMagnitude >= minimumDistance) return;

    glm::vec2 normal = distance / distanceMagnitude;
    float overlap = minimumDistance - distanceMagnitude;
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

std::vector<glm::vec2> CalculateAccelerations(
    const std::vector<Circle>& circles,
    const std::vector<glm::vec2>& positions,
    float gravitationalConstant
) {
    std::vector<glm::vec2> accelerations(circles.size(), glm::vec2(0.0f));

    for (size_t first = 0; first < circles.size(); ++first) {
        for (size_t second = 0; second < circles.size(); ++second) {
            if (first == second) continue;

            glm::vec2 distance = positions[second] - positions[first];
            float distanceMagnitude = glm::length(distance);
            if (distanceMagnitude <= 0.0f) continue;

            glm::vec2 normal = distance / distanceMagnitude;
            glm::vec2 gravity =
                (gravitationalConstant * circles[second].mass /
                 (distanceMagnitude * distanceMagnitude)) * normal;
            glm::vec2 force = circles[first].mass * gravity;
            accelerations[first] += force / circles[first].mass;
        }
    }

    return accelerations;
}

void IntegrateRK4(std::vector<Circle>& circles, float dt, float gravitationalConstant) {
    size_t count = circles.size();
    std::vector<glm::vec2> initialPositions(count);
    std::vector<glm::vec2> initialVelocities(count);
    for (size_t i = 0; i < count; ++i) {
        initialPositions[i] = circles[i].position;
        initialVelocities[i] = circles[i].velocity;
    }

    std::vector<glm::vec2> k1Position = initialVelocities;
    std::vector<glm::vec2> k1Velocity = CalculateAccelerations(circles, initialPositions, gravitationalConstant);

    std::vector<glm::vec2> midpointPositions(count);
    std::vector<glm::vec2> midpointVelocities(count);
    for (size_t i = 0; i < count; ++i) {
        midpointPositions[i] = initialPositions[i] + 0.5f * dt * k1Position[i];
        midpointVelocities[i] = initialVelocities[i] + 0.5f * dt * k1Velocity[i];
    }
    std::vector<glm::vec2> k2Position = midpointVelocities;
    std::vector<glm::vec2> k2Velocity = CalculateAccelerations(circles, midpointPositions, gravitationalConstant);

    for (size_t i = 0; i < count; ++i) {
        midpointPositions[i] = initialPositions[i] + 0.5f * dt * k2Position[i];
        midpointVelocities[i] = initialVelocities[i] + 0.5f * dt * k2Velocity[i];
    }
    std::vector<glm::vec2> k3Position = midpointVelocities;
    std::vector<glm::vec2> k3Velocity = CalculateAccelerations(circles, midpointPositions, gravitationalConstant);

    std::vector<glm::vec2> finalPositions(count);
    std::vector<glm::vec2> finalVelocities(count);
    for (size_t i = 0; i < count; ++i) {
        finalPositions[i] = initialPositions[i] + dt * k3Position[i];
        finalVelocities[i] = initialVelocities[i] + dt * k3Velocity[i];
    }
    std::vector<glm::vec2> k4Position = finalVelocities;
    std::vector<glm::vec2> k4Velocity = CalculateAccelerations(circles, finalPositions, gravitationalConstant);

    for (size_t i = 0; i < count; ++i) {
        circles[i].position = initialPositions[i] + (dt / 6.0f) *
            (k1Position[i] + 2.0f * k2Position[i] + 2.0f * k3Position[i] + k4Position[i]);
        circles[i].velocity = initialVelocities[i] + (dt / 6.0f) *
            (k1Velocity[i] + 2.0f * k2Velocity[i] + 2.0f * k3Velocity[i] + k4Velocity[i]);
    }
}

int main() {
    const float t = 50.0f; // simulation time in seconds
    const int circleResolution = 10;
    const float circleRadius = 15.0f;
    const float G = 1.0f; // Simulation-scaled gravitational constant.
    const glm::vec3 circleColor(1.0f, 1.0f, 1.0f);
    std::vector<Circle> circles = {//pos,vel,rad,mass,color
        { glm::vec2(0.95f, 0.95f), glm::vec2(0.0f, -0.5f), circleRadius, 1.0f, circleColor },//m1
        { glm::vec2(0.95f, -0.95f), glm::vec2(-0.5f, 0.0f), circleRadius, 1.0f, circleColor },//m2
        { glm::vec2(0.0f, 0.0f), glm::vec2(0.0f, 0.0f), circleRadius, 1.0f, circleColor },//m3
        { glm::vec2(-0.95f, 0.95f), glm::vec2(0.5f, 0.0f), circleRadius, 1.0f, circleColor },//m4
        { glm::vec2(-0.95f, -0.95f), glm::vec2(0.0f, 0.5f), circleRadius, 1.0f, circleColor },//m5
    };
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

    GLuint circleVAO, circleVBO;
    glGenVertexArrays(1, &circleVAO);
    glGenBuffers(1, &circleVBO);
    glBindVertexArray(circleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
    glBufferData(GL_ARRAY_BUFFER, circleVertices.size() * sizeof(GLfloat), circleVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);

    GLint positionLocation = glGetUniformLocation(shaderProgram, "position");
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

        IntegrateRK4(circles, dt, G);

        glUseProgram(shaderProgram);
        glBindVertexArray(circleVAO);
        for (Circle& circle : circles) {
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