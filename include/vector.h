#include <cmath>

class vector3D
{
private:
public:

    double x,y,z;
    vector3D(double x_, double y_, double z_){
        x = x_;
        y = y_;
        z = z_;
    }

    vector3D(){
        x = 0;
        y = 0;
        z = 0;
    }

    void load(double x_, double y_, double z_){
        x = x_;
        y = y_;
        z = z_;
    }


    double norm(){
        return sqrt(x*x+y*y+z*z);
    }

    vector3D scalar(double a){
        return vector3D(x*a,y*a,z*a);
    }
    
    vector3D operator+ (vector3D &obj){
        return vector3D(x+obj.x,y+obj.y,z+obj.z);
    }

    vector3D operator- (vector3D &obj){
        return vector3D(x-obj.x,y-obj.y,z-obj.z);
    }

    vector3D operator* (vector3D &obj){
        return vector3D(x*obj.x,y*obj.y,z*obj.z);
    }

    vector3D operator^ (vector3D &obj){
        return vector3D(y*obj.z-obj.y*z,z*obj.x-obj.z*x,x*obj.y-obj.x*y);
    }

};