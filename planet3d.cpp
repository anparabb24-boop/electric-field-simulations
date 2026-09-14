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
    "layout (location = 1) in vec3 aNormal;\n"
    "uniform vec3 position;\n"
    "uniform vec3 scale;\n"
    "uniform vec2 cameraOffset;\n"
    "uniform float zoom;\n"
    "uniform float azimuth;\n"
    "uniform float elevation;\n"
    "uniform float roll;\n"
    "uniform bool isGrid;\n"
    "out vec3 normal;\n"
    "void main() \n"
    "{\n"
    "   vec3 worldPosition = isGrid\n"
    "       ? aPos\n"
    "       : aPos * scale + position;\n"
    "   vec3 relativePosition = worldPosition - vec3(cameraOffset.x, 0.0, cameraOffset.y);\n"
    "   float azimuthCosine = cos(azimuth);\n"
    "   float azimuthSine = sin(azimuth);\n"
    "   float elevationCosine = cos(elevation);\n"
    "   vec3 cameraForward = vec3(elevationCosine * azimuthSine, -elevationCosine * azimuthCosine, -sin(elevation));\n"
    "   vec3 cameraRight = vec3(azimuthCosine, azimuthSine, 0.0);\n"
    "   vec3 cameraUp = cross(cameraForward, cameraRight);\n"
    "   vec2 orientedPosition = vec2(dot(relativePosition, cameraRight), dot(relativePosition, cameraUp));\n"
    "   float rollCosine = cos(roll);\n"
    "   float rollSine = sin(roll);\n"
    "   vec2 rolledPosition = vec2(\n"
    "       orientedPosition.x * rollCosine - orientedPosition.y * rollSine,\n"
    "       orientedPosition.x * rollSine + orientedPosition.y * rollCosine);\n"
    "   gl_Position = vec4(rolledPosition * zoom, dot(relativePosition, cameraForward) * 0.01, 1.0);\n"
    "   normal = aNormal;\n"
    "}\0";

const char* fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform vec3 sphereColor;\n"
    "in vec3 normal;\n"
    "void main()\n"
    "{\n"
    "   vec3 lightDirection = normalize(vec3(-0.4, 0.8, 0.6));\n"
    "   float light = max(dot(normalize(normal), lightDirection), 0.0);\n"
    "   FragColor = vec4(sphereColor * (0.2 + 0.8 * light), 1.0f);\n"
    "}\n\0";

int screenWidth = 1200;
int screenHeight = 800;

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    screenWidth = width;
    screenHeight = height;
    glViewport(0, 0, width, height);
}

glm::vec2 cameraOffset(0.0f, 0.0f);
float cameraZoom = 1.0f;
float azimuth = 0.0f;
float elevation = 0.0f;
float roll = 0.0f;
bool isDragging = false;
bool simulationStarted = false;
double lastMouseX = 0.0;
double lastMouseY = 0.0;

void mouse_button_callback(GLFWwindow* window, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_RIGHT) return;

    isDragging = action == GLFW_PRESS;
    if (isDragging) {
        glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
    }
}

void cursor_position_callback(GLFWwindow*, double mouseX, double mouseY) {
    if (!isDragging) return;

    cameraOffset.x -= static_cast<float>((mouseX - lastMouseX) * 2.0 / screenWidth) / cameraZoom;
    cameraOffset.y += static_cast<float>((mouseY - lastMouseY) * 2.0 / screenHeight) / cameraZoom;
    lastMouseX = mouseX;
    lastMouseY = mouseY;
}

void scroll_callback(GLFWwindow*, double, double yOffset) {
    cameraZoom *= std::pow(1.1f, static_cast<float>(yOffset));
    cameraZoom = std::clamp(cameraZoom, 0.2f, 5.0f);
}

void key_callback(GLFWwindow*, int key, int, int action, int) {
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        simulationStarted = true;
    }
}

struct Sphere {
    glm::vec3 position;
    glm::vec3 velocity;
    float mass;
    float density;
    glm::vec3 color;

    float Radius() const {
        constexpr float pi = 3.14159265359f;
        return std::cbrt(3.0f * mass / (4.0f * pi * density));
    }
};

std::vector<GLfloat> CreateSphereVertices(int sectors, int stacks) {
    constexpr float pi = 3.14159265359f;
    std::vector<GLfloat> sphereVertices;

    auto appendVertex = [&sphereVertices](float theta, float phi) {
        glm::vec3 normal(
            std::sin(theta) * std::cos(phi),
            std::cos(theta),
            std::sin(theta) * std::sin(phi));
        sphereVertices.insert(sphereVertices.end(), {
            normal.x, normal.y, normal.z,
            normal.x, normal.y, normal.z
        });
    };

    for (int stack = 0; stack < stacks; ++stack) {
        float theta0 = pi * static_cast<float>(stack) / stacks;
        float theta1 = pi * static_cast<float>(stack + 1) / stacks;
        for (int sector = 0; sector < sectors; ++sector) {
            float phi0 = 2.0f * pi * static_cast<float>(sector) / sectors;
            float phi1 = 2.0f * pi * static_cast<float>(sector + 1) / sectors;

            appendVertex(theta0, phi0);
            appendVertex(theta1, phi0);
            appendVertex(theta1, phi1);
            appendVertex(theta0, phi0);
            appendVertex(theta1, phi1);
            appendVertex(theta0, phi1);
        }
    }
    return sphereVertices;
}

std::vector<GLfloat> CreateGridVertices(
    float minimum,
    float maximum,
    float spacing,
    const std::vector<Sphere>& spheres,
    float curvatureStrength,
    float softening) {
    std::vector<GLfloat> gridVertices;
    const glm::vec3 normal(0.0f, 0.0f, 1.0f);

    auto getGridHeight = [&spheres, curvatureStrength, softening](float x, float y) {
        float z = 0.0f;
        for (const Sphere& sphere : spheres) {
            float distanceX = x - sphere.position.x;
            float distanceY = y - sphere.position.y;
            float distance = std::sqrt(
                distanceX * distanceX + distanceY * distanceY + softening * softening);
            z -= curvatureStrength * sphere.mass / distance;
        }
        return z;
    };

    auto appendSegment = [&gridVertices, &normal, &getGridHeight](
        float x1, float y1, float x2, float y2) {
        float z1 = getGridHeight(x1, y1);
        float z2 = getGridHeight(x2, y2);

        gridVertices.insert(gridVertices.end(), {
            x1, y1, z1, normal.x, normal.y, normal.z,
            x2, y2, z2, normal.x, normal.y, normal.z
        });
    };

    for (float coordinate = minimum; coordinate < maximum; coordinate += spacing) {
        for (float segmentCoordinate = minimum;
             segmentCoordinate < maximum;
             segmentCoordinate += spacing) {
            float nextSegmentCoordinate = std::min(segmentCoordinate + spacing, maximum);
            appendSegment(coordinate, segmentCoordinate,
                coordinate, nextSegmentCoordinate);
            appendSegment(segmentCoordinate, coordinate,
                nextSegmentCoordinate, coordinate);
        }
    }
    return gridVertices;
}

double GetProcessCpuSeconds() {
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1000000.0
        + usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1000000.0;
}

void ResolveSphereCollision(Sphere& first, Sphere& second) {
    glm::vec3 distance = second.position - first.position;
    float distanceMagnitude = glm::length(distance);
    float minimumDistance = first.Radius() + second.Radius();

    if (distanceMagnitude <= 0.0f || distanceMagnitude >= minimumDistance) return;

    glm::vec3 normal = distance / distanceMagnitude;
    float overlap = minimumDistance - distanceMagnitude;
    first.position -= normal * (overlap * 0.5f);
    second.position += normal * (overlap * 0.5f);

    glm::vec3 relativeVelocity = second.velocity - first.velocity;
    float velocityAlongNormal = glm::dot(relativeVelocity, normal);
    if (velocityAlongNormal >= 0.0f) return;

    const float restitution = 0.97f;
    float impulseMagnitude = -(1.0f + restitution) * velocityAlongNormal / 2.0f;
    glm::vec3 impulse = impulseMagnitude * normal;
    first.velocity -= impulse;
    second.velocity += impulse;
}

std::vector<glm::vec3> CalculateAccelerations(
    const std::vector<Sphere>& spheres,
    const std::vector<glm::vec3>& positions,
    float gravitationalConstant
) {
    std::vector<glm::vec3> accelerations(spheres.size(), glm::vec3(0.0f));

    for (size_t first = 0; first < spheres.size(); ++first) {
        for (size_t second = 0; second < spheres.size(); ++second) {
            if (first == second) continue;

            glm::vec3 distance = positions[second] - positions[first];
            float distanceMagnitude = glm::length(distance);
            if (distanceMagnitude <= 0.0f) continue;

            glm::vec3 normal = distance / distanceMagnitude;
            glm::vec3 gravity =
                (gravitationalConstant * spheres[second].mass /
                 (distanceMagnitude * distanceMagnitude)) * normal;
            glm::vec3 force = spheres[first].mass * gravity;
            accelerations[first] += force / spheres[first].mass;
        }
    }

    return accelerations;
}

void IntegrateRK4(std::vector<Sphere>& spheres, float dt, float gravitationalConstant) {
    size_t count = spheres.size();
    std::vector<glm::vec3> initialPositions(count);
    std::vector<glm::vec3> initialVelocities(count);
    for (size_t i = 0; i < count; ++i) {
        initialPositions[i] = spheres[i].position;
        initialVelocities[i] = spheres[i].velocity;
    }

    std::vector<glm::vec3> k1Position = initialVelocities;
    std::vector<glm::vec3> k1Velocity = CalculateAccelerations(spheres, initialPositions, gravitationalConstant);

    std::vector<glm::vec3> midpointPositions(count);
    std::vector<glm::vec3> midpointVelocities(count);
    for (size_t i = 0; i < count; ++i) {
        midpointPositions[i] = initialPositions[i] + 0.5f * dt * k1Position[i];
        midpointVelocities[i] = initialVelocities[i] + 0.5f * dt * k1Velocity[i];
    }
    std::vector<glm::vec3> k2Position = midpointVelocities;
    std::vector<glm::vec3> k2Velocity = CalculateAccelerations(spheres, midpointPositions, gravitationalConstant);

    for (size_t i = 0; i < count; ++i) {
        midpointPositions[i] = initialPositions[i] + 0.5f * dt * k2Position[i];
        midpointVelocities[i] = initialVelocities[i] + 0.5f * dt * k2Velocity[i];
    }
    std::vector<glm::vec3> k3Position = midpointVelocities;
    std::vector<glm::vec3> k3Velocity = CalculateAccelerations(spheres, midpointPositions, gravitationalConstant);

    std::vector<glm::vec3> finalPositions(count);
    std::vector<glm::vec3> finalVelocities(count);
    for (size_t i = 0; i < count; ++i) {
        finalPositions[i] = initialPositions[i] + dt * k3Position[i];
        finalVelocities[i] = initialVelocities[i] + dt * k3Velocity[i];
    }
    std::vector<glm::vec3> k4Position = finalVelocities;
    std::vector<glm::vec3> k4Velocity = CalculateAccelerations(spheres, finalPositions, gravitationalConstant);

    for (size_t i = 0; i < count; ++i) {
        spheres[i].position = initialPositions[i] + (dt / 6.0f) *
            (k1Position[i] + 2.0f * k2Position[i] + 2.0f * k3Position[i] + k4Position[i]);
        spheres[i].velocity = initialVelocities[i] + (dt / 6.0f) *
            (k1Velocity[i] + 2.0f * k2Velocity[i] + 2.0f * k3Velocity[i] + k4Velocity[i]);
    }
}

int main() {
    const float t = 100.0f; // simulation time in seconds
    const int sphereResolution = 20;
    const float sphereRadius = 100.0f;
    const float sphereMass = 10.0f;
    const float sphereDensity = 3.0f * sphereMass /
        (4.0f * 3.14159265359f * sphereRadius * sphereRadius * sphereRadius);
    const float G = 1.0f; // Simulation-scaled gravitational constant.
    const glm::vec4 sphereColor(1.0f, 0.0f, 0.0f,1.0f);
    const glm::vec4 moonColor(0.0f, 0.0f, 1.0f,1.0f);
    std::vector<Sphere> spheres = {//position(x,y,z), velocity(x,y,z), mass, density, color
        //{ glm::vec3(0.95f, 0.0f, 0.95f), glm::vec3(0.0f, 0.0f, -0.5f), sphereMass, sphereDensity, sphereColor },//s1
        //{ glm::vec3(1.5f, 0.0f, 0.0f), glm::vec3(0.0f, 3.6f, 0.0f), 0.1f*sphereMass,sphereDensity, moonColor },//s2
        { glm::vec3(4.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 2.97f), 0.5f *sphereMass, sphereDensity, sphereColor },//s3
        { glm::vec3(-4.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -2.97f),0.5f *sphereMass,sphereDensity, sphereColor },//s4
        { glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f),5.0f*sphereMass, sphereDensity, moonColor },//s5
        { glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(3.97f, 0.0f, 0.0f), 0.01f *sphereMass, sphereDensity, sphereColor },//s3
        { glm::vec3(0.0f, -2.0f, 0.0f), glm::vec3(-3.97f, 0.0f, 0.0f),0.01f *sphereMass,sphereDensity, sphereColor },//s4
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
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
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


    std::vector<GLfloat> sphereVertices = CreateSphereVertices(sphereResolution, sphereResolution / 2);
    const float gridHalfExtent =  6.0f;
    const float gridSpacing = 0.15f;
    const float curvatureStrength = 0.1f;
    const float curvatureSoftening = 0.5f;
    std::vector<GLfloat> gridVertices = CreateGridVertices(
        -gridHalfExtent, gridHalfExtent, gridSpacing,
        spheres, curvatureStrength, curvatureSoftening);

    GLuint sphereVAO, sphereVBO;
    glGenVertexArrays(1, &sphereVAO);
    glGenBuffers(1, &sphereVBO);
    glBindVertexArray(sphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, sphereVertices.size() * sizeof(GLfloat), sphereVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    GLuint gridVAO, gridVBO;
    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);
    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(GLfloat), gridVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    GLint positionLocation = glGetUniformLocation(shaderProgram, "position");
    const float restitution = 0.97f;
    GLint scaleLocation = glGetUniformLocation(shaderProgram, "scale");
    GLint cameraOffsetLocation = glGetUniformLocation(shaderProgram, "cameraOffset");
    GLint zoomLocation = glGetUniformLocation(shaderProgram, "zoom");
    GLint azimuthLocation = glGetUniformLocation(shaderProgram, "azimuth");
    GLint elevationLocation = glGetUniformLocation(shaderProgram, "elevation");
    GLint rollLocation = glGetUniformLocation(shaderProgram, "roll");
    GLint gridModeLocation = glGetUniformLocation(shaderProgram, "isGrid");
    GLint colorLocation = glGetUniformLocation(shaderProgram, "sphereColor");
    float previousTime = static_cast<float>(glfwGetTime());
    auto simulationStart = std::chrono::steady_clock::now();
    auto metricsTime = simulationStart;
    double metricsCpuSeconds = GetProcessCpuSeconds();
    double gpuEstimatePercent = 0.0;

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0f,0.0f,0.0f,1.0f); // set background color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glfwSwapBuffers(window);

//------------------------------------------------- MAIN LOOP -------------------------------------------------//
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
        glClearColor(0.0f,0.0f,0.0f,1.0f); // set background color
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float currentTime = static_cast<float>(glfwGetTime());
        float dt = currentTime - previousTime;
        previousTime = currentTime;
        if (dt > 0.05f) dt = 0.05f;

        const float cameraSpeed = 1.5f * dt / cameraZoom;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraOffset.y += cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraOffset.y -= cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cameraOffset.x -= cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cameraOffset.x += cameraSpeed;
        const float angleSpeed = 1.5f * dt;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) azimuth += angleSpeed;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) azimuth -= angleSpeed;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
            elevation = std::min(elevation + angleSpeed, 1.4f);
        }
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            elevation = std::max(elevation - angleSpeed, -1.4f);
        }
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) roll += angleSpeed;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) roll -= angleSpeed;

        if (simulationStarted) {
            IntegrateRK4(spheres, dt, G);
        }

        gridVertices = CreateGridVertices(
            -gridHalfExtent, gridHalfExtent, gridSpacing,
            spheres, curvatureStrength, curvatureSoftening);
        glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
        glBufferSubData(
            GL_ARRAY_BUFFER, 0,
            gridVertices.size() * sizeof(GLfloat), gridVertices.data());

        glUseProgram(shaderProgram);
        glBindVertexArray(sphereVAO);
        glUniform2f(cameraOffsetLocation, cameraOffset.x, cameraOffset.y);
        glUniform1f(zoomLocation, cameraZoom);
        glUniform1f(azimuthLocation, azimuth);
        glUniform1f(elevationLocation, elevation);
        glUniform1f(rollLocation, roll);

        glBindVertexArray(gridVAO);
        glUniform1i(gridModeLocation, GL_TRUE);
        glUniform3f(colorLocation, 0.5f, 0.5f, 0.5f);
        glLineWidth(10.0f);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(gridVertices.size() / 6));

        glBindVertexArray(sphereVAO);
        glUniform1i(gridModeLocation, GL_FALSE);
        for (Sphere& sphere : spheres) {
            float sphereRadius = sphere.Radius() / (screenWidth / 2.0f);

            if (sphere.position.x + sphereRadius > gridHalfExtent) {
                sphere.position.x = gridHalfExtent - sphereRadius;
                sphere.velocity.x = -std::abs(sphere.velocity.x) * restitution;
            } else if (sphere.position.x - sphereRadius < -gridHalfExtent) {
                sphere.position.x = -gridHalfExtent + sphereRadius;
                sphere.velocity.x = std::abs(sphere.velocity.x) * restitution;
            }

            if (sphere.position.y + sphereRadius > gridHalfExtent) {
                sphere.position.y = gridHalfExtent - sphereRadius;
                sphere.velocity.y = -std::abs(sphere.velocity.y) * restitution;
            } else if (sphere.position.y - sphereRadius < -gridHalfExtent) {
                sphere.position.y = -gridHalfExtent + sphereRadius;
                sphere.velocity.y = std::abs(sphere.velocity.y) * restitution;
            }
        }

        for (size_t first = 0; first < spheres.size(); ++first) {
            for (size_t second = first + 1; second < spheres.size(); ++second) {
                glm::vec3 firstPositionPixels(
                    spheres[first].position.x * screenWidth / 2.0f,
                    spheres[first].position.y * screenWidth / 2.0f,
                    spheres[first].position.z * screenHeight / 2.0f);
                glm::vec3 secondPositionPixels(
                    spheres[second].position.x * screenWidth / 2.0f,
                    spheres[second].position.y * screenWidth / 2.0f,
                    spheres[second].position.z * screenHeight / 2.0f);
                glm::vec3 firstVelocityPixels(
                    spheres[first].velocity.x * screenWidth / 2.0f,
                    spheres[first].velocity.y * screenWidth / 2.0f,
                    spheres[first].velocity.z * screenHeight / 2.0f);
                glm::vec3 secondVelocityPixels(
                    spheres[second].velocity.x * screenWidth / 2.0f,
                    spheres[second].velocity.y * screenWidth / 2.0f,
                    spheres[second].velocity.z * screenHeight / 2.0f);

                spheres[first].position = firstPositionPixels;
                spheres[second].position = secondPositionPixels;
                spheres[first].velocity = firstVelocityPixels;
                spheres[second].velocity = secondVelocityPixels;
                ResolveSphereCollision(spheres[first], spheres[second]);
                spheres[first].position = glm::vec3(
                    spheres[first].position.x / (screenWidth / 2.0f),
                    spheres[first].position.y / (screenWidth / 2.0f),
                    spheres[first].position.z / (screenHeight / 2.0f));
                spheres[second].position = glm::vec3(
                    spheres[second].position.x / (screenWidth / 2.0f),
                    spheres[second].position.y / (screenWidth / 2.0f),
                    spheres[second].position.z / (screenHeight / 2.0f));
                spheres[first].velocity = glm::vec3(
                    spheres[first].velocity.x / (screenWidth / 2.0f),
                    spheres[first].velocity.y / (screenWidth / 2.0f),
                    spheres[first].velocity.z / (screenHeight / 2.0f));
                spheres[second].velocity = glm::vec3(
                    spheres[second].velocity.x / (screenWidth / 2.0f),
                    spheres[second].velocity.y / (screenWidth / 2.0f),
                    spheres[second].velocity.z / (screenHeight / 2.0f));
            }
        }

        auto renderStart = std::chrono::steady_clock::now();
        for (const Sphere& sphere : spheres) {
            float radiusX = sphere.Radius() / (screenWidth / 2.0f);
            float radiusZ = sphere.Radius() / (screenHeight / 2.0f);
            glUniform3f(positionLocation, sphere.position.x, sphere.position.y, sphere.position.z);
            glUniform3f(scaleLocation, radiusX, radiusX, radiusZ);
            glUniform3f(colorLocation, sphere.color.r, sphere.color.g, sphere.color.b);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(sphereVertices.size() / 6));
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

    glDeleteVertexArrays(1, &sphereVAO);
    glDeleteBuffers(1, &sphereVBO);
    glDeleteVertexArrays(1, &gridVAO);
    glDeleteBuffers(1, &gridVBO);
    glDeleteProgram(shaderProgram);


    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}