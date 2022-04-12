//
// Created by wyz on 2021/4/7.
//
#include "NeuronVisGLApplication.hpp"
#include <cmdline.hpp>

#include <Common/renderer_config.hpp>
#include <Render/large_volume_renderer.hpp>
#include <Window/WindowManager.hpp>

int NeuronVisGLApplication::run(int argc, char **argv)
{
    cmdline::parser cmd;

    cmd.add<std::string>("config_file", 'c', "config file path", false, "renderer_config.json");
    cmd.parse_check(argc, argv);
    auto config_file = cmd.get<std::string>("config_file");
    try
    {
        RendererConfig renderer_config(config_file.c_str());
        WindowManager &windowManager = WindowManager::Instance();
        windowManager.Init(config_file);

        LargeVolumeRenderer renderer(windowManager.GetWholeWindowWidth(), windowManager.GetWholeWindowHeight());

        renderer.set_volume(renderer_config.getLodFile().c_str());

        renderer.set_transferfunc(renderer_config.getTransferFuncSetting());

        renderer.set_camera(renderer_config.getCameraSetting());

        renderer.render();
    }
    catch (const std::exception &err)
    {
        std::cout << err.what() << std::endl;
        return 1;
    }
    return 0;
}
