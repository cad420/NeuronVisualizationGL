
include(${PROJECT_SOURCE_DIR}/cmake/utils.cmake)
#embed_shaders(
#        CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/Render/shaders.hpp.in
#        TARGET ${CMAKE_CURRENT_SOURCE_DIR}/Render/shaders.hpp
#        DIR ${CMAKE_CURRENT_SOURCE_DIR}/Render/shaders
#)
include(${PROJECT_SOURCE_DIR}/third_party/spdlog.cmake)
include(${PROJECT_SOURCE_DIR}/third_party/glfw.cmake)
include(${PROJECT_SOURCE_DIR}/third_party/glm.cmake)
include(${PROJECT_SOURCE_DIR}/third_party/msmpi.cmake)
include(${PROJECT_SOURCE_DIR}/third_party/VolumeCompression.cmake)
include(${PROJECT_SOURCE_DIR}/third_party/imgui.cmake)
find_package(CUDA REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)
find_package(OpenMP REQUIRED)
list(
        APPEND
        NeuronVisualizationGL_SRCS
        ${PROJECT_SOURCE_DIR}/third_party/glad/src/glad.c
        main.cpp
        NeuronVisGLApplication.cpp
        Data/large_volume_manager.cpp
        IO/large_volume_reader.cpp
        Render/large_volume_renderer.cpp
)

add_executable(NeuronVisualizationGL ${NeuronVisualizationGL_SRCS})
target_include_directories(
        NeuronVisualizationGL
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CUDA_INCLUDE_DIRS}
        ${MPI_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/third_party/glad/include
        ${PROJECT_SOURCE_DIR}/third_party
)
target_link_libraries(
        NeuronVisualizationGL
        PRIVATE
        Threads::Threads
        OpenMP::OpenMP_CXX
        spdlog::spdlog
        glm::glm
        glfw
        imgui
        voxel_uncompress
        ${OPENGL_LIBRARIES}
        ${CUDA_CUDA_LIBRARY}
        ${CMAKE_DL_LIBS}
        ${MPI_LIBRARIES}
)
target_compile_features(NeuronVisualizationGL PRIVATE cxx_std_17)
