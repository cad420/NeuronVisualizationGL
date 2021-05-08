//
// Created by wyz on 2021/5/3.
//

#ifndef NEURONVISUALIZATIONGL_RENDERER_CONFIG_HPP
#define NEURONVISUALIZATIONGL_RENDERER_CONFIG_HPP
#include <json.hpp>
#include <map>
#include <array>
using json = nlohmann::json;
class RendererConfig{
public:
    explicit RendererConfig(const char* path){
        std::ifstream in(path);
        if(!in.is_open()){
            d=true;
            std::cout<<"Open renderer config file failed! Using default config."<<std::endl;
        }
        else{
            in>>j;
            d=false;
        }
    }
    uint32_t getWidth() const{
        if(d || j.find("width")==j.end()){
            std::cout<<"Not provide width in config file, use default width(1200)."<<std::endl;
            return 1200;
        }
        else{
            return j["width"];
        }
    }
    uint32_t getHeight() const{
        if(d || j.find("height")==j.end()){
            std::cout<<"Not provide height in config file, use default height(900)."<<std::endl;
            return 900;
        }
        else{
            return j["height"];
        }
    }
    std::string getLodFile() const{
        if(d || j.find("lod_file")==j.end()){
            std::cout<<"Not provide lod_file in config file, use default lod_file(mouse_file_config.json)."<<std::endl;
            return "mouse_file_config";
        }
        else{
            return j["lod_file"];
        }
    }
    std::map<uint8_t,std::array<double,4>> getTransferFuncSetting() const{
        std::map<uint8_t ,std::array<double,4>> color_map;
        if(!d && j.find("tf")!=j.end()){
            auto points=j["tf"];
            for(auto it=points.begin();it!=points.end();it++){
                int key=std::stoi(it.key());
                auto values=it.value();
                color_map[key]={values[0],values[1],values[2],values[3]};
            }
            return color_map;
        }
        else{
            std::cout<<"Not provide tf in config file, use default tf(linear gray)."<<std::endl;
            color_map[0]={0.0,0.0,0.0,0.0};
            color_map[255]={1.0,1.0,1.0,1.0};
            return color_map;
        }
    }
    std::tuple<std::array<float,3>,std::array<float,3>,std::array<float,3>,float,float,float> getCameraSetting() const{

        if(d || j.find("camera")==j.end()){
            std::cout<<"Not provide camera in config file, use default camera."<<std::endl;
            return std::make_tuple(std::array<float,3>{0.f,0.f,0.f},
                                   std::array<float,3>{0.f,0.f,-1.f},
                                   std::array<float,3>{0.f,1.f,0.f},
                                   20.f,1.f,512.f);
        }
        else{
            auto camera_config=j["camera"];
            auto pos=camera_config["pos"];
            std::array<float,3> camera_pos={pos[0],pos[1],pos[2]};
            auto front=camera_config["front"];
            std::array<float ,3> camera_front={front[0],front[1],front[2]};
            auto up=camera_config["up"];
            std::array<float ,3> camera_up={up[0],up[1],up[1]};
            float zoom=camera_config["zoom"];
            float n=camera_config["n"];
            float f=camera_config["f"];
            return std::make_tuple(camera_pos,camera_front,camera_up,zoom,n,f);
        }
    }

private:
    json j;
    bool d;
};




#endif //NEURONVISUALIZATIONGL_RENDERER_CONFIG_HPP
