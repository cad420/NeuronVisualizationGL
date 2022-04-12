//
// Created by wyz on 2021/4/7.
//
#include "NeuronVisGLApplication.hpp"
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

int main(int argc, char **argv)
{
    spdlog::info("Welcome to NeuronVisualizationGL!");

    NeuronVisGLApplication app;

    return app.run(argc, argv);
}
