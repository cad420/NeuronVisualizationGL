//
// Created by wyz on 2021/4/7.
//
#include "NeuronVisGLApplication.hpp"
#include <cmdline.hpp>

#include <Render/large_volume_renderer.hpp>
#include <Common/renderer_config.hpp>
int NeuronVisGLApplication::run(int argc, char **argv) {
    cmdline::parser cmd;

    cmd.add<std::string>("config_file",'c',"config file path",false,"renderer_config.json");
    cmd.parse_check(argc,argv);
    auto config_file=cmd.get<std::string>("config_file");
    try{
        RendererConfig renderer_config(config_file.c_str());

        LargeVolumeRenderer renderer(renderer_config.getWidth(),renderer_config.getHeight());

        renderer.set_volume(renderer_config.getLodFile().c_str());

        renderer.set_transferfunc(renderer_config.getTransferFuncSetting());

        renderer.set_camera(renderer_config.getCameraSetting());

        renderer.render();
    }
    catch (const std::exception& err) {
        std::cout<<err.what()<<std::endl;
        return 1;
    }
    return 0;
}
