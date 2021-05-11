//
// Created by wyz on 20-12-8.
//

#ifndef VOLUMERENDERER_CAMERA_H
#define VOLUMERENDERER_CAMERA_H
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<math.h>
#include<iostream>
#include"boundingbox.hpp"
namespace sv {
    enum class CameraDefinedKey{
        MOVE_FASTER,MOVE_SLOWER,ROTATE_FASTER,ROTATE_SLOWER,VIEW_FAR,VIEW_NEAR};

    enum class CameraMoveDirection{
        FORWARD,BACKWARD,
        LEFT,RIGHT,
        UP,DOWN
    };

    class Camera{

    public:
        Camera(glm::vec3 camera_pos):
                pos(camera_pos),up(glm::vec3(0.0f,1.0f,0.0f)),
                front(glm::vec3(0.0f,0.0f,-1.0f)),
//        right(glm::vec3(1.0f,0.0f,0.0f)),
                world_up(glm::vec3(0.0f,1.0f,0.0f)),
                yaw(-90.0f),pitch(0.0f),
                move_speed(0.03f),
                mouse_sensitivity(0.1f),
                zoom(20.0f)
        {
            updateCameraVectors();
        }

        glm::mat4 getViewMatrix();
        void processMovementByKeyboard(CameraMoveDirection direction,float delta_t);
        void processMouseMovement(float xoffset,float yoffset);
        void processMouseScroll(float yoffset);
        void processKeyboardForArgs(CameraDefinedKey arg);
        void updateCameraVectors();

    private:
        glm::vec3 pos;//point
        glm::vec3 front;//vector
        glm::vec3 up;//vector
        glm::vec3 right;//vector

        glm::vec3 world_up;//vector

        float yaw;
        float pitch;

        float move_speed;
        float mouse_sensitivity;
        float zoom;
    public:
        float getZoom(){return zoom;}
    };
    inline glm::mat4 Camera::getViewMatrix()
    {
//        std::cout<<pos.x<<" "<<pos.y<<" "<<pos.z<<std::endl;
//        std::cout<<front.x<<" "<<front.y<<" "<<front.z<<std::endl;
//        std::cout<<up.x<<" "<<up.y<<" "<<up.z<<std::endl;
        return glm::lookAt(pos,pos+front,up);
    }
    inline void Camera::processMovementByKeyboard(CameraMoveDirection direction,float delta_t)
    {

        float ds=move_speed*delta_t;
        switch (direction) {
            case CameraMoveDirection::FORWARD: pos+=front*ds;break;
            case CameraMoveDirection::BACKWARD: pos-=front*ds;break;
            case CameraMoveDirection::LEFT: pos-=right*ds;break;
            case CameraMoveDirection::RIGHT: pos+=right*ds;break;
            case CameraMoveDirection::UP: pos+=up*ds;break;
            case CameraMoveDirection::DOWN: pos-=up*ds;break;
        }
    }
    inline void Camera::processMouseMovement(float xoffset,float yoffset)
    {
        yaw+=xoffset*mouse_sensitivity;
        pitch+=yoffset*mouse_sensitivity;
        if(pitch>60.0f)
            pitch=60.0f;
        if(pitch<-60.0f)
            pitch=-60.0f;
        updateCameraVectors();
    }
    inline void Camera::processMouseScroll(float yoffset)
    {
        zoom-=yoffset;
        if(zoom<0.1f)
            zoom=0.1f;
        if(zoom>45.0f)
            zoom=45.0f;
    }
    inline void Camera::processKeyboardForArgs(CameraDefinedKey arg)
    {
        if(arg==CameraDefinedKey::MOVE_FASTER)
            move_speed*=2;
        else if(arg==CameraDefinedKey::MOVE_SLOWER)
            move_speed/=2;
        else if(arg==CameraDefinedKey::ROTATE_FASTER)
            mouse_sensitivity*=2;
        else if(arg==CameraDefinedKey::ROTATE_SLOWER)
            mouse_sensitivity/=2;
    }
    inline void Camera::updateCameraVectors()
    {
        glm::vec3 f;
        f.x=std::cos(glm::radians(yaw))*std::cos(glm::radians(pitch));
        f.y=std::sin(glm::radians(pitch));
        f.z=std::sin(glm::radians(yaw))*std::cos(glm::radians(pitch));
        front=glm::normalize(f);
        right=glm::normalize(glm::cross(front,world_up));
        up=glm::normalize(glm::cross(right,front));
    }


    class RayCastCamera{
    public:
        RayCastCamera(glm::vec3 camera_pos,glm::vec3 front,glm::vec3 up,float zoom,float n,float f,uint32_t half_w,uint32_t half_f)
        :camera_pos(camera_pos),view_direction(front),up(up),fov(zoom),n(n),f(f),half_x_n(half_w),half_y_n(half_f)
        {
            this->world_up=glm::vec3(0.f,1.f,0.f);
            this->move_speed=1.f;
            this->yaw=-90.f;
            this->pitch=0.f;
            this->move_sense=0.05f;
            this->is_perspective=true;
            this->space_z=1.f;
            this->ratio=(float)half_x_n/half_y_n;
            updateSpaceXY();
            updateCameraVectors();
        }
        RayCastCamera(glm::vec3 view_pos, uint32_t half_w, uint32_t half_h, bool is_pers=false):
                half_x_n(half_w),half_y_n(half_h),is_perspective(is_pers)
        {

            this->camera_pos=view_pos;
            this->world_up=glm::vec3(0.f,1.f,0.f);
            this->yaw=-90.f;
            this->pitch=0.f;
            this->move_speed=1.f;
            this->move_sense=0.05f;
            this->fov=20.f;
            this->space_x=this->space_y=this->space_z=1.f;

            this->n=0.f;//assert!!!
            this->f=512.f;
            if(is_perspective){
                setCameraMode(true);
            }
            updateCameraVectors();
        }
        OBB getOBB(){
//            assert(this->n==0.f);
            auto center_pos=camera_pos+view_direction*(f+n)/2.f;
            if(!is_perspective){
                setCameraMode(false);
                return OBB(center_pos,right,up,view_direction,half_x_n*space_x,half_y_n*space_y,(f-n)/2.f);
            }
            else{
                setCameraMode(true);
                return OBB(center_pos,right,up,view_direction,f*tanf(glm::radians(fov/2))*half_x_n/half_y_n,f*tanf(glm::radians(fov/2)),(f-n)/2.f);
            }
        }
        Pyramid getPyramid() const{
            float far_plane_half_h=this->f*tanf(glm::radians(fov/2));
            float far_plane_half_w=far_plane_half_h*ratio;
            auto lu_pos=camera_pos+f*view_direction+up*far_plane_half_h-right*far_plane_half_w;
            auto ru_pos=camera_pos+f*view_direction+up*far_plane_half_h+right*far_plane_half_w;
            auto ld_pos=camera_pos+f*view_direction-up*far_plane_half_h-right*far_plane_half_w;
            auto rd_pos=camera_pos+f*view_direction-up*far_plane_half_h+right*far_plane_half_w;
//            std::cout<<__FUNCTION__ <<std::endl;
//            print_vec(view_direction);
            return Pyramid(camera_pos,lu_pos,ru_pos,ld_pos,rd_pos);
        }
        void setupOBB(OBB& obb){
            assert(this->n==0.f);
            obb.center_pos=camera_pos+view_direction*(f+3*n)/2.f;;
            obb.unit_x=right;
            obb.unit_y=up;
            obb.unit_z=view_direction;
            obb.half_wx=half_x_n*space_x;
            obb.half_wy=half_y_n*space_y;
            obb.half_wz=(n+f)/2.f;
            obb.updateHalfLenVec();
        }
        glm::mat4 RayCastCamera::getViewMatrix();
        void processMovementByKey(CameraMoveDirection direction,float delta_t);
        void processMouseMove(float xoffset,float yoffset);
        void processMouseScroll(float yoffset);
        void processKeyForArg(CameraDefinedKey arg);
        void updateCameraVectors() ;
        void updateSpaceXY();
        void setCameraMode(bool is_pers){
            this->is_perspective=is_pers;
            if(is_perspective){
                this->n=1.f;
                this->f=512.f;
                updateSpaceXY();
            }
            else{
                if(this->space_x<0.1f){
                    this->space_x=this->space_y=1.f;
                }
            }
        }

    public:
        glm::vec3 camera_pos;
        glm::vec3 view_direction;//keep unit
        glm::vec3 up,right;//keep unit
        glm::vec3 world_up;//keep unit
        float n,f;
        float ratio;

        float yaw,pitch;

        float move_speed;
        float move_sense;
    public:
        float space_x,space_y,space_z;//x-direction and y-direction gap distance between two rays
        uint32_t half_x_n, half_y_n;
        float fov;
        bool is_perspective=false;

    };


    inline glm::mat4 RayCastCamera::getViewMatrix()
    {

        return glm::lookAt(camera_pos,camera_pos+view_direction,up);
    }

    inline void RayCastCamera::updateSpaceXY() {
        this->space_y=this->f*tanf(glm::radians(fov/2))/this->half_y_n;
        this->space_x=this->space_y;

    }

    inline void RayCastCamera::processMovementByKey(CameraMoveDirection direction, float delta_t) {
//        std::cout<<__FUNCTION__ <<std::endl;
        float ds=move_speed*delta_t;
        switch (direction) {
            case CameraMoveDirection::FORWARD: camera_pos+=view_direction*ds;break;
            case CameraMoveDirection::BACKWARD: camera_pos-=view_direction*ds;break;
            case CameraMoveDirection::LEFT: camera_pos-=right*ds;break;
            case CameraMoveDirection::RIGHT: camera_pos+=right*ds;break;
            case CameraMoveDirection::UP: camera_pos+=up*ds;break;
            case CameraMoveDirection::DOWN: camera_pos-=up*ds;break;
        }

    }

    inline void RayCastCamera::processMouseMove(float xoffset, float yoffset) {
        yaw+=xoffset*move_sense;
        pitch+=yoffset*move_sense;
//        std::cout<<xoffset<<"**"<<yoffset<<std::endl;
//        std::cout<<yaw<<"**"<<pitch<<std::endl;
        if(pitch>60.0f)
            pitch=60.0f;
        if(pitch<-60.0f)
            pitch=-60.0f;
        updateCameraVectors();
    }

    inline void RayCastCamera::processMouseScroll(float yoffset) {\
        if(!is_perspective) {
            if (yoffset > 0) {
                space_x += 0.1f;
                space_y += 0.1f;
                if (space_x > 1.5f) {
                    space_x = 1.5f;
                    space_y = 1.5f;
                }
            } else {
                space_x -= 0.1f;
                space_y -= 0.1f;
                if (space_x < 0.2f) {
                    space_x = 0.2f;
                    space_y = 0.2f;
                }
            }
        }
        else{
            fov+=yoffset;
            if(fov<0.1f){
                fov=0.1f;
            }
            else if(fov>60.f){
                fov=60.f;
            }
            updateSpaceXY();
        }
//        std::cout<<"space: "<<space_x<<std::endl;
    }

    inline void RayCastCamera::processKeyForArg(CameraDefinedKey arg) {
        if(arg==CameraDefinedKey::MOVE_FASTER)
            move_speed*=2;
        else if(arg==CameraDefinedKey::MOVE_SLOWER)
            move_speed/=2;
        else if(arg==CameraDefinedKey::ROTATE_FASTER)
            move_sense*=2;
        else if(arg==CameraDefinedKey::ROTATE_SLOWER)
            move_sense/=2;
        else if(arg==CameraDefinedKey::VIEW_FAR)
            f*=1.1f;
        else if(arg==CameraDefinedKey::VIEW_NEAR)
            f*=0.9f;
    }

    inline void RayCastCamera::updateCameraVectors() {
        glm::vec3 f;
        f.x=std::cos(glm::radians(yaw))*std::cos(glm::radians(pitch));
        f.y=std::sin(glm::radians(pitch));
        f.z=std::sin(glm::radians(yaw))*std::cos(glm::radians(pitch));
        view_direction=glm::normalize(f);
        right=glm::normalize(glm::cross(view_direction,world_up));
        up=glm::normalize(glm::cross(right,view_direction));

    }

}

#endif //VOLUMERENDERER_CAMERA_H
