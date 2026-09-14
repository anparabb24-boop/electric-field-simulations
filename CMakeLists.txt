cmake_minimum_required(VERSION 3.10)
project(PhysicsSim)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

add_executable(PhysicsSim
    src/main.cpp
    src/glad/src/glad.c
)


target_include_directories(PhysicsSim PRIVATE src/glad/include)
target_link_libraries(PhysicsSim glfw glm::glm "-framework OpenGL")

add_executable(Demo
    openwindow.cpp
    src/glad/src/glad.c
)

target_include_directories(Demo PRIVATE src/glad/include)
target_link_libraries(Demo glfw glm::glm "-framework OpenGL")

add_executable(Try
    gravity.cpp
    src/glad/src/glad.c
)

add_executable(Try
    planet.cpp
    src/glad/src/glad.c
)

target_include_directories(Try PRIVATE src/glad/include)
target_link_libraries(Try glfw glm::glm "-framework OpenGL")