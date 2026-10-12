#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <iostream>
#include <cstdlib>
#include <vector>
using namespace std;

int width = 1130;
int* pwidth = &width;
int height = 700;
int* pheight = &height;

const char* vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "uniform vec2 positionOffset;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(aPos.xy + positionOffset, aPos.z, 1.0);\n"
    "}\0";

const char* fragmentShaderSource = "#version 330 core\n"
    "uniform vec3 objectColor;\n"
    "out vec4 FragColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = vec4(objectColor, 1.0f);\n"
    "}\n\0";

struct Acceleration {
    // SI units: meters per second squared.
    float ax;
    float ay;
};

class Circle {
public:
float pixelsPerMeter;
int width;
int height;
float radius;
float vx;
float vy;
float q;
int k = 1;
    Circle(float x, float y, float vx, float vy, float radius,
           int resolution, int width, int height, float pixelsPerMeter, float charge)
        : pixelsPerMeter(pixelsPerMeter), width(width), height(height),
          radius(radius), vx(vx), vy(vy), q(charge), x(x), y(y) {
        constexpr float pi = 3.14159265358979323846f;
        resolution = max(resolution, 3);

        // Keep the circle centered at the origin; position is supplied to the shader.
        vector<float> vertices;
        vertices.reserve(static_cast<size_t>(resolution + 2) * 3);
        vertices.insert(vertices.end(), {0.0f, 0.0f, 0.0f});
        for (int i = 0; i <= resolution; ++i) {
            const float angle = 2.0f * pi * static_cast<float>(i) / resolution;
            vertices.insert(vertices.end(), {
                2.0f * radius * pixelsPerMeter * cos(angle) / width,
                2.0f * radius * pixelsPerMeter * sin(angle) / height,
                0.0f
            });
        }

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
                     vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        vertexCount = static_cast<GLsizei>(resolution + 2);
    }

    ~Circle() {
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
    }

    void draw(GLint positionLocation) const {
        glUniform2f(positionLocation,
                    2.0f * x * pixelsPerMeter / width,
                    2.0f * y * pixelsPerMeter / height);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, vertexCount);
        glBindVertexArray(0);
    }

    void setPos(float a, float b) {
        x = a;
        y = b;
    }
    float getX() const {
        return x;
    }

    float getY() const {
        return y;
    }

    void update(Acceleration a, float dt) {
        // Position is in meters, velocity in m/s, acceleration in m/s².
        vx += a.ax * dt;
        vy += a.ay * dt;
        x += vx * dt;
        y += vy * dt;

        // Convert the pixel-sized window bounds into meters.
        const float halfWidthMeters = width / (2.0f * pixelsPerMeter);
        const float halfHeightMeters = height / (2.0f * pixelsPerMeter);
        const float left = -halfWidthMeters + radius;
        const float right = halfWidthMeters - radius;
        const float bottom = -halfHeightMeters + radius;
        const float top = halfHeightMeters - radius;

        if (x < left) {
            x = left;
            if (vx < 0.0f) vx = -0.95f*vx;
        } else if (x > right) {
            x = right;
            if (vx > 0.0f) vx = -0.95f*vx;
        }

        if (y < bottom) {
            y = bottom;
            if (vy < 0.0f) vy = -0.95f*vy;
        } else if (y > top) {
            y = top;
            if (vy > 0.0f) vy = -0.95f*vy;
        }
    }

    //void Field(float a,float b){
        //float Sr = hypot(a-x,b-y);
        //if (Sr <= radius) {
            //std::cout << "Point inside circle; field not calculated.\n";
            //return;
        //}
        //float phi = atan2(b-y,a-x);
        //float Ex = k*q*cos(phi)/(Sr*Sr);
        //float Ey = k*q*sin(phi)/(Sr*Sr);
        //std::cout<<"Ex = "<<Ex<<"\nEy = "<<Ey<<std::endl;
    //}
private:
    float x;
    float y;
    
    GLuint vao = 0;
    GLuint vbo = 0;
    GLsizei vertexCount = 0;
};

struct GridPoint {
    float x; // meters, in the same coordinate system as Circle::x
    float y; // meters, in the same coordinate system as Circle::y
};

class Grid {
public:
    Grid(float spacing, const int* pwidth, const int* pheight,
         float pixelsPerMeter) {
        if (spacing <= 0.0f || pwidth == nullptr || pheight == nullptr ||
            pixelsPerMeter <= 0.0f) {
            return;
        }

        const float halfWidthMeters = *pwidth / (2.0f * pixelsPerMeter);
        const float halfHeightMeters = *pheight / (2.0f * pixelsPerMeter);

        for (float y = -halfHeightMeters; y <= halfHeightMeters; y += spacing) {
            for (float x = -halfWidthMeters; x <= halfWidthMeters; x += spacing) {
                points.push_back({x, y});
            }
        }
    }

    const std::vector<GridPoint>& getPoints() const {
        return points;
    }

private:
    std::vector<GridPoint> points;
};

class FieldVector {
public:
    FieldVector(const GridPoint& point, const std::vector<const Circle*>& charges)
        : x(point.x), y(point.y) {
        for (const Circle* charge : charges) {
            if (charge == nullptr) continue;

            const float dx = x - charge->getX();
            const float dy = y - charge->getY();
            const float r2 = dx * dx + dy * dy;

            if (r2 <= charge->radius * charge->radius) {
                return; // Suppress the entire field vector inside any source circle.
            }

            const float r = std::sqrt(r2);
            ex += charge->k * charge->q * dx / (r2 * r);
            ey += charge->k * charge->q * dy / (r2 * r);
        }

        magnitude = std::hypot(ex, ey);
    }

    float x, y;                 // Arrow tail: grid point
    float ex = 0.0f, ey = 0.0f; // Total field components
    float magnitude = 0.0f;

    // Draw this field vector as a shaft and two arrowhead lines.
    // The VAO/VBO are shared by all FieldVector instances and created in main.
    void draw(GLuint arrowVao, GLuint arrowVbo,
              GLint positionLocation, GLint colorLocation,
              int windowWidth, int windowHeight, float pixelsPerMeter,
              float maxMagnitude, float arrowLength) const {
        if (magnitude <= 0.0f || windowWidth <= 0 || windowHeight <= 0 ||
            pixelsPerMeter <= 0.0f || maxMagnitude <= 0.0f || arrowLength <= 0.0f) {
            return;
        }

        const float ux = ex / magnitude;
        const float uy = ey / magnitude;
        const float tipX = x + ux * arrowLength;
        const float tipY = y + uy * arrowLength;
        const float headLength = arrowLength * 0.25f;
        const float angle = std::atan2(uy, ux);
        constexpr float headAngle = 0.5f;
        const float leftX = tipX - headLength * std::cos(angle - headAngle);
        const float leftY = tipY - headLength * std::sin(angle - headAngle);
        const float rightX = tipX - headLength * std::cos(angle + headAngle);
        const float rightY = tipY - headLength * std::sin(angle + headAngle);

        const auto clipX = [=](float worldX) {
            return 2.0f * worldX * pixelsPerMeter / windowWidth;
        };
        const auto clipY = [=](float worldY) {
            return 2.0f * worldY * pixelsPerMeter / windowHeight;
        };
        const float vertices[] = {
            clipX(x),    clipY(y),    0.0f, clipX(tipX),   clipY(tipY),   0.0f,
            clipX(tipX), clipY(tipY), 0.0f, clipX(leftX),  clipY(leftY),  0.0f,
            clipX(tipX), clipY(tipY), 0.0f, clipX(rightX), clipY(rightY), 0.0f
        };

        const float strength = std::clamp(magnitude / maxMagnitude, 0.0f, 1.0f);
        glUniform2f(positionLocation, 0.0f, 0.0f);
        glUniform3f(colorLocation, strength, 0.15f, 1.0f - strength);
        glBindVertexArray(arrowVao);
        glBindBuffer(GL_ARRAY_BUFFER, arrowVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
        glDrawArrays(GL_LINES, 0, 6);
        glBindVertexArray(0);
    }
};

class AnimatedStreaks {
public:
    AnimatedStreaks(const std::vector<GridPoint>& seedPoints,
                    const std::vector<const Circle*>& charges,
                    int windowWidth, int windowHeight, float pixelsPerMeter,
                    float flowSpeed, float sampleSpacing, float halfWidth)
        : charges(charges), windowWidth(windowWidth), windowHeight(windowHeight),
          pixelsPerMeter(pixelsPerMeter), flowSpeed(flowSpeed),
          sampleSpacing(sampleSpacing), halfWidth(halfWidth) {
        if (windowWidth <= 0 || windowHeight <= 0 || pixelsPerMeter <= 0.0f ||
            flowSpeed <= 0.0f || sampleSpacing <= 0.0f || halfWidth <= 0.0f) {
            return;
        }

        for (const GridPoint& seed : seedPoints) {
            if (!insideAnyCharge(seed)) {
                emitters.push_back({seed, emissionInterval});
                particles.push_back(makeParticle(seed)); // Show one immediately.
            }
        }

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STREAM_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    ~AnimatedStreaks() {
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
    }

    AnimatedStreaks(const AnimatedStreaks&) = delete;
    AnimatedStreaks& operator=(const AnimatedStreaks&) = delete;

    void update(float dt) {
        dt = std::clamp(dt, 0.0f, 0.05f);
        const float halfWidthMeters = windowWidth / (2.0f * pixelsPerMeter);
        const float halfHeightMeters = windowHeight / (2.0f * pixelsPerMeter);

        for (size_t i = 0; i < particles.size();) {
            Particle& particle = particles[i];
            particle.age += dt;
            const GridPoint current{particle.x, particle.y};
            const FieldVector field(current, charges);
            if (field.magnitude <= 1.0e-6f || particle.age >= particleLifetime) {
                removeParticle(i);
                continue;
            }

            const float dx = field.ex / field.magnitude * flowSpeed * dt;
            const float dy = field.ey / field.magnitude * flowSpeed * dt;
            particle.x += dx;
            particle.y += dy;
            particle.distanceSinceSample += std::hypot(dx, dy);

            if (particle.x < -halfWidthMeters || particle.x > halfWidthMeters ||
                particle.y < -halfHeightMeters || particle.y > halfHeightMeters ||
                insideAnyCharge({particle.x, particle.y})) {
                removeParticle(i);
                continue;
            }

            if (particle.distanceSinceSample >= sampleSpacing) {
                particle.distanceSinceSample = 0.0f;
                particle.history[particle.historyHead] = {particle.x, particle.y};
                particle.historyHead = (particle.historyHead + 1) % trailPointCount;
                particle.historySize = std::min(particle.historySize + 1, trailPointCount);
            }
            ++i;
        }

        // Each seed point emits another streak at a steady interval.
        for (Emitter& emitter : emitters) {
            emitter.timeUntilNext -= dt;
            if (emitter.timeUntilNext <= 0.0f) {
                particles.push_back(makeParticle(emitter.seed));
                emitter.timeUntilNext += emissionInterval;
            }
        }
    }

    void draw(GLint positionLocation, GLint colorLocation) {
        std::vector<float> mesh;
        mesh.reserve(particles.size() * (trailPointCount - 1) * 18);

        for (const Particle& particle : particles) {
            if (particle.historySize < 3) continue;

            std::array<GridPoint, trailPointCount> ordered{};
            const int oldest = (particle.historyHead - particle.historySize + trailPointCount)
                             % trailPointCount;
            for (int i = 0; i < particle.historySize; ++i) {
                ordered[i] = particle.history[(oldest + i) % trailPointCount];
            }

            std::array<GridPoint, trailPointCount> left{};
            std::array<GridPoint, trailPointCount> right{};
            for (int i = 0; i < particle.historySize; ++i) {
                const GridPoint& previous = ordered[std::max(i - 1, 0)];
                const GridPoint& next = ordered[std::min(i + 1, particle.historySize - 1)];
                const float tx = next.x - previous.x;
                const float ty = next.y - previous.y;
                float tangentX = tx;
                float tangentY = ty;
                float tangentLength = std::hypot(tangentX, tangentY);
                if (tangentLength <= 1.0e-6f) {
                    tangentX = next.x - ordered[i].x;
                    tangentY = next.y - ordered[i].y;
                    tangentLength = std::hypot(tangentX, tangentY);
                }
                if (tangentLength <= 1.0e-6f) {
                    tangentX = ordered[i].x - previous.x;
                    tangentY = ordered[i].y - previous.y;
                    tangentLength = std::hypot(tangentX, tangentY);
                }
                if (tangentLength <= 1.0e-6f) {
                    tangentX = 1.0f;
                    tangentY = 0.0f;
                    tangentLength = 1.0f;
                }

                const float taper = std::sin(3.14159265358979323846f * i /
                                             (particle.historySize - 1));
                const float halfThickness = halfWidth * taper;
                const float nx = -tangentY / tangentLength;
                const float ny = tangentX / tangentLength;
                left[i] = {ordered[i].x + nx * halfThickness,
                           ordered[i].y + ny * halfThickness};
                right[i] = {ordered[i].x - nx * halfThickness,
                            ordered[i].y - ny * halfThickness};
            }

            for (int i = 0; i + 1 < particle.historySize; ++i) {
                appendVertex(mesh, left[i]);
                appendVertex(mesh, right[i]);
                appendVertex(mesh, left[i + 1]);
                appendVertex(mesh, right[i]);
                appendVertex(mesh, right[i + 1]);
                appendVertex(mesh, left[i + 1]);
            }
        }

        if (mesh.empty()) return;
        glUniform2f(positionLocation, 0.0f, 0.0f);
        glUniform3f(colorLocation, 0.5f, 0.5f, 0.5f);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, mesh.size() * sizeof(float), mesh.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mesh.size() / 3));
        glBindVertexArray(0);
    }

private:
    static constexpr int trailPointCount = 12;

    struct Particle {
        float x = 0.0f;
        float y = 0.0f;
        float age = 0.0f;
        float distanceSinceSample = 0.0f;
        std::array<GridPoint, trailPointCount> history{};
        int historyHead = 0;
        int historySize = 0;
    };

    struct Emitter {
        GridPoint seed;
        float timeUntilNext;
    };

    bool insideAnyCharge(const GridPoint& point) const {
        for (const Circle* charge : charges) {
            if (charge == nullptr) continue;
            const float dx = point.x - charge->getX();
            const float dy = point.y - charge->getY();
            if (dx * dx + dy * dy <= charge->radius * charge->radius) return true;
        }
        return false;
    }

    Particle makeParticle(const GridPoint& seed) const {
        Particle particle;
        particle.x = seed.x;
        particle.y = seed.y;
        particle.historyHead = 1;
        particle.historySize = 1;
        particle.history[0] = seed;
        return particle;
    }

    void removeParticle(size_t index) {
        particles[index] = particles.back();
        particles.pop_back();
    }

    void appendVertex(std::vector<float>& mesh, const GridPoint& point) const {
        mesh.push_back(2.0f * point.x * pixelsPerMeter / windowWidth);
        mesh.push_back(2.0f * point.y * pixelsPerMeter / windowHeight);
        mesh.push_back(0.0f);
    }

    const std::vector<const Circle*>& charges;
    int windowWidth;
    int windowHeight;
    float pixelsPerMeter;
    float flowSpeed;
    float sampleSpacing;
    float halfWidth;
    static constexpr float emissionInterval = 0.5f;
    static constexpr float particleLifetime = 5.0f;
    std::vector<Emitter> emitters;
    std::vector<Particle> particles;
    GLuint vao = 0;
    GLuint vbo = 0;
};

bool animationPaused = false;

void keyCallback(GLFWwindow*, int key, int, int action, int) {
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        animationPaused = !animationPaused;
    }
}

int main() {
    
    constexpr float pixelsPerMeter = 100.0f; // 100 pixels represent 1 meter.
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


    GLFWwindow* window = glfwCreateWindow(width, height, "EMF2D", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, keyCallback);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glViewport(0, 0, width, height);

    // Compile and link the shaders.
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    GLint colorLocation = glGetUniformLocation(shaderProgram, "objectColor");
    GLint positionLocation = glGetUniformLocation(shaderProgram, "positionOffset");

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glClearColor(0.0f, 0.1f, 0.15f, 1.0f);

    // One reusable dynamic buffer holds the six vertices for each arrow.
    GLuint arrowVao = 0;
    GLuint arrowVbo = 0;
    glGenVertexArrays(1, &arrowVao);
    glGenBuffers(1, &arrowVbo);
    glBindVertexArray(arrowVao);
    glBindBuffer(GL_ARRAY_BUFFER, arrowVbo);
    glBufferData(GL_ARRAY_BUFFER, 18 * sizeof(float), nullptr, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    {
    Circle circle(-2.0f, 0.0f, 1.0f, -1.0f, 0.1f, 64, width, height, pixelsPerMeter,100);
    Circle circle2(2.0f, 0.0f, 0.0f, 0.0f,0.1f, 64, width, height, pixelsPerMeter,-100);
    Circle circle3(0.0f, 1.0f, 0.0f, 0.0f,0.1f, 64, width, height, pixelsPerMeter,-100);
    Circle circle4(0.0f, -1.0f, 0.0f, 0.0f,0.1f, 64, width, height, pixelsPerMeter,100);
    Grid grid(0.15f, pwidth, pheight, pixelsPerMeter);
    const std::vector<const Circle*> charges{&circle, &circle2, &circle3, &circle4};
    Grid streakSeedGrid(0.375f, pwidth, pheight, pixelsPerMeter);
    AnimatedStreaks streaks(streakSeedGrid.getPoints(), charges,
                            width, height, pixelsPerMeter,
                            0.8f, 0.002f, 0.005f);
    const Acceleration acceleration{0.0f, 0.0f};
    double previousTime = glfwGetTime();

    // Keep the window open and process input until it is closed.
    while (!glfwWindowShouldClose(window)) {
        const double currentTime = glfwGetTime();
        const float dt = static_cast<float>(currentTime - previousTime);
        previousTime = currentTime;

        if (!animationPaused) {
            circle.update(acceleration, dt);
            circle2.update(acceleration, dt);
            streaks.update(dt);
        }
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glUniform3f(colorLocation, 1.0f, 0.0f, 0.0f);
        circle.draw(positionLocation);
        circle2.draw(positionLocation);
        circle3.draw(positionLocation);
        circle4.draw(positionLocation);
        streaks.draw(positionLocation, colorLocation);
        for (const GridPoint& point : grid.getPoints()) {
            const FieldVector field(point, charges);
            field.draw(arrowVao, arrowVbo, positionLocation, colorLocation, width, height, pixelsPerMeter, 400.0f, 0.15f);
        }
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    } // Destroy the circle's OpenGL buffers while the context is still active.

    glDeleteBuffers(1, &arrowVbo);
    glDeleteVertexArrays(1, &arrowVao);
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);

    glfwTerminate();
    return 0;
}
