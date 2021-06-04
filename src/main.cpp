//
// Created by wyz on 2021/4/7.
//
#include<spdlog/spdlog.h>
#include<glm/glm.hpp>
#include "NeuronVisGLApplication.hpp"

int main(int argc,char** argv)
{
    spdlog::info("Welcome to NeuronVisualizationGL!");

    NeuronVisGLApplication app;

    return app.run(argc,argv);
}
