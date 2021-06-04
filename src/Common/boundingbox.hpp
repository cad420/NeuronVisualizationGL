//
// Created by wyz on 20-12-4.
//

#ifndef VOLUMERENDER_BOUNDINGBOX_H
#define VOLUMERENDER_BOUNDINGBOX_H
#include <limits>
#include <vector>
#include <array>

#include <glm/glm.hpp>

#include "utils.hpp"

#define INVALID 0b1111111111111111
namespace sv {
    class OBB;
    class AABB{
    public:
        AABB(){
            float min=std::numeric_limits<float>::lowest();
            float max=std::numeric_limits<float>::max();
            min_p={max,max,max};
            max_p={min,min,min};
            index={INVALID,INVALID,INVALID};
        };
        AABB(const glm::vec3& min_p,const glm::vec3& max_p,const std::array<uint32_t,4>& index)
        :min_p(min_p),max_p(max_p),index(index){}
        AABB(const glm::vec3& min_p,const glm::vec3& max_p):min_p(min_p),max_p(max_p),index({INVALID,INVALID,INVALID}){}

        AABB(const AABB& aabb){
            min_p=aabb.min_p;
            max_p=aabb.max_p;
            index=aabb.index;
        }
        AABB(const std::array<uint32_t,4>& index){
            float min=std::numeric_limits<float>::lowest();
            float max=std::numeric_limits<float>::max();
            min_p={max,max,max};
            max_p={min,min,min};
            this->index=index;
        }
        OBB convertToOBB() const;
        void Union(const glm::vec3& p){
            min_p={
                    std::fmin(min_p.x,p.x),
                    std::fmin(min_p.y,p.y),
                    std::fmin(min_p.z,p.z)
            };
            max_p={
                std::fmax(max_p.x,p.x),
                std::fmax(max_p.y,p.y),
                std::fmax(max_p.z,p.z)
            };
        }

        bool intersect(const AABB& aabb) const{
            if(fmax(min_p.x,aabb.min_p.x)>fmin(max_p.x,aabb.max_p.x)||
                    fmax(min_p.y,aabb.min_p.y)>fmin(max_p.y,aabb.max_p.y)||
                        fmax(min_p.z,aabb.min_p.z)>fmin(max_p.z,aabb.max_p.z))
                return false;
            return true;
        }
        friend std::ostream& operator<<(std::ostream& os,const AABB& aabb){
            os<<"("<<aabb.min_p.x<<" "<<aabb.min_p.y<<" "<<aabb.min_p.z<<")\t"
            <<"("<<aabb.max_p.x<<" "<<aabb.max_p.y<<" "<<aabb.max_p.z<<")"<<std::endl;
            return os;
        }
        bool operator==(const AABB& aabb) const {
//            std::cout<<"AABB =="<<std::endl;
            return (index[0]==aabb.index[0]
            && index[1]==aabb.index[1]
            && index[2]==aabb.index[2] && index[3]==aabb.index[3]);
        }
        bool operator=(const AABB& aabb){
            min_p=aabb.min_p;
            max_p=aabb.max_p;
            index=aabb.index;
        }
//        bool operator==(const std::array<uint32_t,3> index){
//            return (this->index[0]==index[0]
//            && this->index[1]==index[1]
//            && this->index[2]==index[2]);
//        }
        bool isCubic() const{
            auto x_len=max_p.x-min_p.x;
            auto y_len=max_p.y-min_p.y;
            auto z_len=max_p.z-min_p.z;
            if(std::fabs(x_len-y_len)>FLOAT_ZERO || std::fabs(y_len-z_len)>FLOAT_ZERO){
                return false;
            }
            else{
                return true;
            }
        }
        float get_half_length() const{
            if(isCubic()){
                return (max_p.x-min_p.x)*0.86603f;
            }
            else{
                print("error! not cubic aabb!");
                return 0.f;
            }
        }
    public:
        glm::vec3 min_p,max_p;
        std::array<uint32_t ,4> index;//x,y,z and lod
    };

/**
 * obb intersect algorithm:
 * https://blog.csdn.net/silangquan/article/details/50812425?utm_medium=distribute.pc_relevant.none-task-blog-2%7Edefault%7EBlogCommendFromBaidu%7Edefault-5.control&dist_request_id=&depth_1-utm_source=distribute.pc_relevant.none-task-blog-2%7Edefault%7EBlogCommendFromBaidu%7Edefault-5.control
 */

    class OBB{
    public:
        OBB(glm::vec3 center_pos,glm::vec3 x,glm::vec3 y,glm::vec3 z,float half_x,float half_y,float half_z)
        :center_pos(center_pos),unit_x(glm::normalize(x)),unit_y(glm::normalize(y)),unit_z(glm::normalize(z)),half_wx(half_x),half_wy(half_y),half_wz(half_z)
        {
            updateHalfLenVec();
        }

        void updateHalfLenVec(){
            hx=half_wx*unit_x;
            hy=half_wy*unit_y;
            hz=half_wz*unit_z;
        }
        OBB(const OBB& obb){
            this->center_pos=obb.center_pos;
            this->unit_x=obb.unit_x;
            this->unit_y=obb.unit_y;
            this->unit_z=obb.unit_z;
            this->half_wx=obb.half_wx;
            this->half_wy=obb.half_wy;
            this->half_wz=obb.half_wz;
        }
        OBB& operator=(const OBB& obb){
            this->center_pos=obb.center_pos;
            this->unit_x=obb.unit_x;
            this->unit_y=obb.unit_y;
            this->unit_z=obb.unit_z;
            this->half_wx=obb.half_wx;
            this->half_wy=obb.half_wy;
            this->half_wz=obb.half_wz;
            return *this;
        }

        AABB getAABB() const{
            std::vector<glm::vec3> pts;
            for(int i=-1;i<=1;i+=2){
                for(int j=-1;j<=1;j+=2){
                    for(int k=-1;k<=1;k+=2){
                        pts.push_back(center_pos+i*half_wx*unit_x+j*half_wy*unit_y+k*half_wz*unit_z);
                    }
                }
            }
            AABB aabb;
            for(auto& it:pts){
                aabb.Union(it);
            }
            return aabb;
        }

        friend class RayCastCamera;

        bool intersect_aabb(const AABB& aabb)=delete;
        bool intersect_obb(const OBB& obb);

    private:
        glm::vec3 center_pos;
        glm::vec3 unit_x;
        glm::vec3 unit_y;
        glm::vec3 unit_z;
        float half_wx;
        float half_wy;
        float half_wz;
        glm::vec3 hx,hy,hz;//normalized direction
    };

    inline bool OBB::intersect_obb(const OBB &obb)
    {
        auto T=obb.center_pos-this->center_pos;

        float t,ta,tb;

        std::vector<glm::vec3> Ls;
        Ls.push_back(this->unit_x);
        Ls.push_back(this->unit_y);
        Ls.push_back(this->unit_z);
        Ls.push_back(obb.unit_x);
        Ls.push_back(obb.unit_y);
        Ls.push_back(obb.unit_z);
        Ls.push_back(glm::cross(this->unit_x,obb.unit_x));
        Ls.push_back(glm::cross(this->unit_x,obb.unit_y));
        Ls.push_back(glm::cross(this->unit_x,obb.unit_z));
        Ls.push_back(glm::cross(this->unit_y,obb.unit_x));
        Ls.push_back(glm::cross(this->unit_y,obb.unit_y));
        Ls.push_back(glm::cross(this->unit_y,obb.unit_z));
        Ls.push_back(glm::cross(this->unit_z,obb.unit_x));
        Ls.push_back(glm::cross(this->unit_z,obb.unit_y));
        Ls.push_back(glm::cross(this->unit_z,obb.unit_z));

        for(auto& L:Ls){
            t=fabs(glm::dot(T,L));
            ta=fabs(glm::dot(this->hx,L))+fabs(glm::dot(this->hy,L))+fabs(glm::dot(this->hz,L));
            tb=fabs(glm::dot(obb.hx,L))+fabs(glm::dot(obb.hy,L))+fabs(glm::dot(obb.hz,L));
//            print("t: ",t,"ta: ",ta,"tb: ",tb);
            if(t>ta+tb){
//                std::cout<<"not intersect with obb"<<std::endl;
                return false;
            }
        }
        return true;
    }


    inline OBB AABB::convertToOBB() const{
        return OBB((min_p+max_p)/2.f,{1.f,0.f,0.f},{0.f,1.f,0.f},{0.f,0.f,1.f},
                   (max_p.x-min_p.x)/2.f,(max_p.y-min_p.y)/2.f,(max_p.z-min_p.z)/2.f);
    }
    inline OBB AABBToOBB(const AABB& aabb){
        return aabb.convertToOBB();
    }

    class Pyramid{
    public:
        Pyramid(glm::vec3 start_pos,glm::vec3 lu_pos,glm::vec3 ru_pos,glm::vec3 ld_pos,glm::vec3 rd_pos)
        :start_pos(start_pos),left_up_pos(lu_pos),right_up_pos(ru_pos),left_down_pos(ld_pos),right_down_pos(rd_pos),is_valid_pyramid(can_get_valid_obb())
        {
            calc_coefficient();
        }

        void reshape(float len){
            right_up_pos=glm::normalize(right_up_pos-start_pos)*len+start_pos;
            left_down_pos=glm::normalize(left_down_pos-start_pos)*len+start_pos;
            right_down_pos=glm::normalize(right_down_pos-start_pos)*len+start_pos;
            is_valid_pyramid=can_get_valid_obb();
            calc_coefficient();
        }

        Pyramid(const Pyramid& pyramid){
            *this=pyramid;
        }

        Pyramid& operator=(const Pyramid& pyramid){
            this->start_pos=pyramid.start_pos;
            this->left_up_pos=pyramid.left_up_pos;
            this->right_up_pos=pyramid.right_up_pos;
            this->left_down_pos=pyramid.left_down_pos;
            this->right_down_pos=pyramid.right_down_pos;
            this->coefficient=pyramid.coefficient;
            return *this;
        }

        OBB getOBB() const{
            if(!is_valid_pyramid){
                throw std::runtime_error("get OBB with invalid pyramid");
            }
            glm::vec3 far_center_pos=(left_up_pos+right_up_pos+left_down_pos+right_down_pos)/4.f;
            glm::vec3 obb_center_pos=(far_center_pos+start_pos)/2.f;
            glm::vec3 obb_x=right_up_pos-left_up_pos;
            glm::vec3 obb_z=start_pos-far_center_pos;
            glm::vec3 obb_y=glm::cross(obb_x, obb_z);
//            glm::vec3 obb_y=left_up_pos-left_down_pos;
            return OBB(obb_center_pos,obb_x,obb_y,obb_z,glm::length(obb_x)/2,glm::length(obb_y)/2,glm::length(obb_z)/2);
        }

        friend class RayCastCamera;

        bool intersect_aabb(const AABB& aabb) const;

        bool intersect_obb(const OBB& obb)=delete;

        bool can_get_valid_obb() const;

        bool point_inside_pyramid(const glm::vec3& point) const;
    private:
        void calc_coefficient();
    private:
        glm::vec3 start_pos;
        glm::vec3 left_up_pos;
        glm::vec3 right_up_pos;
        glm::vec3 left_down_pos;
        glm::vec3 right_down_pos;
        bool is_valid_pyramid;
        //five plane inequality form a pyramid volume
        //Ax+By+Cz+D>0
        //D=-(A*x0+B*y0+C*z0)
        std::vector<std::array<float,4>> coefficient;

    };

    inline bool Pyramid::can_get_valid_obb() const {
        auto v0=glm::normalize(right_up_pos-left_up_pos);
        auto v1=glm::normalize(left_down_pos-left_up_pos);
        auto v2=glm::normalize(right_down_pos-left_up_pos);
//        auto normal=glm::normalize(glm::cross(v0,v1));
//        std::cout<<__FUNCTION__ <<std::endl;
//        std::cout << glm::dot(glm::cross(v0,v1),v2) << std::endl;
        if(std::fabs(glm::dot(glm::cross(v0,v1),v2))>FLOAT_ZERO){
//            print_vec(normal);
//            print_vec(v2);
//            std::cout<<glm::dot(normal,v2)<<std::endl;
//            std::cout<<v0.x<<" "<<v0.y<<" "<<v0.z<<std::endl;
//            std::cout<<v1.x<<" "<<v1.y<<" "<<v1.z<<std::endl;
            return false;
        }

//        auto center_pos=((left_up_pos+right_up_pos)/2.f+(left_down_pos+right_down_pos)/2.f)/2.f;
//        auto s2e=glm::normalize(center_pos-start_pos);
//        if(std::fabs(glm::dot(s2e,v0))>FLOAT_ZERO){
//            std::cout<<v0.x<<" "<<v0.y<<" "<<v0.z<<std::endl;
//            std::cout<<s2e.x<<" "<<s2e.y<<" "<<s2e.z<<std::endl;
//            std::cout<<"std::fabs(glm::dot(s2e,v0)): "<<std::fabs(glm::dot(s2e,v0))<<std::endl;
//            return false;
//        }
        return true;
    }
    inline void Pyramid::calc_coefficient() {
        if(!is_valid_pyramid){
            print("calc invalid pyramid coefficient");
            return;
        }
        coefficient.resize(5);
        auto normal=glm::cross(right_up_pos-left_up_pos,left_down_pos-left_up_pos);
        coefficient[0][0]=normal.x;
        coefficient[0][1]=normal.y;
        coefficient[0][2]=normal.z;
        coefficient[0][3]=-(normal.x*left_up_pos.x+normal.y*left_up_pos.y+normal.z*left_up_pos.z);

        normal=glm::cross(left_up_pos-start_pos,left_down_pos-start_pos);
        coefficient[1][0]=normal.x;
        coefficient[1][1]=normal.y;
        coefficient[1][2]=normal.z;
        coefficient[1][3]=-(normal.x*start_pos.x+normal.y*start_pos.y+normal.z*start_pos.z);

        normal=glm::cross(right_up_pos-start_pos,left_up_pos-start_pos);
        coefficient[2][0]=normal.x;
        coefficient[2][1]=normal.y;
        coefficient[2][2]=normal.z;
        coefficient[2][3]=-(normal.x*start_pos.x+normal.y*start_pos.y+normal.z*start_pos.z);

        normal=glm::cross(right_down_pos-start_pos,right_up_pos-start_pos);
        coefficient[3][0]=normal.x;
        coefficient[3][1]=normal.y;
        coefficient[3][2]=normal.z;
        coefficient[3][3]=-(normal.x*start_pos.x+normal.y*start_pos.y+normal.z*start_pos.z);

        normal=glm::cross(left_down_pos-start_pos,right_down_pos-start_pos);
        coefficient[4][0]=normal.x;
        coefficient[4][1]=normal.y;
        coefficient[4][2]=normal.z;
        coefficient[4][3]=-(normal.x*start_pos.x+normal.y*start_pos.y+normal.z*start_pos.z);

    }
    inline bool Pyramid::point_inside_pyramid(const glm::vec3 &point) const {
        bool inside=true;
        for(int i=0;i<5;i++){
            inside= inside & (coefficient[i][0] * point.x + coefficient[i][1] * point.y + coefficient[i][2] * point.z +
                              coefficient[i][3] < 0.f);
        }
        return inside;
    }
    /**
     * assert aabb is cubic
     * use sphere approximately test intersection
     * distance calc for point to plane:
     *  (Ax+By+Cz+D)/sqrt(A*A+B*B+C*C)
     */
    inline bool Pyramid::intersect_aabb(const AABB &aabb) const{
        static auto point_to_plane_dist=[](const glm::vec3& point,const std::array<float,4>& plane)->float{
            return (plane[0]*point.x+plane[1]*point.y+plane[2]*point.z+plane[3])/sqrt(plane[0]*plane[0]+plane[1]*plane[1]+plane[2]*plane[2]);
        };
//        if(!aabb.isCubic()){
//            throw std::runtime_error("test in-cubic aabb with pyramid");
//        }
        auto center=(aabb.min_p+aabb.max_p)/2.f;

//        if(point_inside_pyramid(center)){
//            return true;
//        }

        //test five plane for pyramid
        //assert really intersect return true but true may not really intersect
        auto half_length=aabb.get_half_length();
        for(int i=0;i<5;i++){
            float dist=point_to_plane_dist(center,coefficient[i]);
            if(dist>0.f && dist>half_length){
//                std::cout<<"not intersect with pyramid: ";
//                print_array(aabb.index);
                return false;
            }
        }
        return true;
    }
}





#endif //VOLUMERENDER_BOUNDINGBOX_H
