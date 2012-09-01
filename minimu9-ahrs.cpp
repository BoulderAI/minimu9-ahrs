#include "vector.h"
#include "MinIMU9.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <system_error>

// TODO: print warning if accelerometer magnitude is not close to 1 when starting up

int millis()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec) * 1000 + (tv.tv_usec)/1000;
}

void print(matrix m)
{
    printf("%7.4f %7.4f %7.4f %7.4f %7.4f %7.4f %7.4f %7.4f %7.4f",
           m(0,0), m(0,1), m(0,2),
           m(1,0), m(1,1), m(1,2),
           m(2,0), m(2,1), m(2,2));
}

void streamRawValues(IMU& imu)
{
    imu.enable();
    while(1)
    {
        imu.read();
        printf("%7d %7d %7d  %7d %7d %7d  %7d %7d %7d\n",
               imu.raw_m[0], imu.raw_m[1], imu.raw_m[2],
               imu.raw_a[0], imu.raw_a[1], imu.raw_a[2],
               imu.raw_g[0], imu.raw_g[1], imu.raw_g[2]
        );
        usleep(20*1000);
    }
}

matrix rotationFromCompass(const vector& acceleration, const vector& magnetic_field)
{
    vector up = acceleration;     // usually true
    vector magnetic_east = magnetic_field.cross(up);
    vector east = magnetic_east;  // a rough approximation
    vector north = up.cross(east);

    matrix rotationFromCompass;
    rotationFromCompass.row(0) = east;
    rotationFromCompass.row(1) = north;
    rotationFromCompass.row(2) = up;
    rotationFromCompass.row(0).normalize();
    rotationFromCompass.row(1).normalize();
    rotationFromCompass.row(2).normalize();

    return rotationFromCompass;
}

float heading(matrix rotation)
{
    // The board's x axis in earth coordinates.
    vector x = rotation.col(0);
    x.normalize();
    x(2) = 0;
    //float heading_weight = x.norm();

    // 0 = east, pi/2 = north
    return atan2(x(1), x(0));
}

typedef void fuse_function(quaternion& rotation, float dt, const vector& angular_velocity,
                  const vector& acceleration, const vector& magnetic_field);


void fuse_compass_only(quaternion& rotation, float dt, const vector& angular_velocity,
  const vector& acceleration, const vector& magnetic_field)
{
    rotation = rotationFromCompass(acceleration, magnetic_field);
}


void fuse_gyro_only(quaternion& rotation, float dt, const vector& angular_velocity,
  const vector& acceleration, const vector& magnetic_field)
{
    rotation = quaternion::Identity();

    quaternion w = quaternion(1, angular_velocity(0)*dt, angular_velocity(1)*dt, angular_velocity(2)*dt);
    rotation = w;
    rotation.normalize();

    // TODO: finish this algorithm

    //rotation *= updateMatrix(angular_velocity, dt);
    //rotation = normalize(rotation);
}

#define Kp_ROLLPITCH 1
#define Ki_ROLLPITCH 0.00002
#define Kp_YAW 1
#define Ki_YAW 0.00002

#define PI 3.14159265

void fuse_default(quaternion& rotation, float dt, const vector& angular_velocity,
  const vector& acceleration, const vector& magnetic_field)
{
    rotation = quaternion::Identity();
    // TODO: finish this algorithm

    //matrix rotationCompass = rotationFromCompass(acceleration, magnetic_field);

    // We trust the accelerometer more if it is telling us 1G.
    //float accel_weight = 1 - 2*abs(1 - acceleration.norm());
    //if (accel_weight < 0){ accel_weight = 0; }

    // Add a "torque" that makes our up vector (rotation.row(2))
    // get closer to the acceleration vector.
    //vector errorRollPitch = acceleration.cross(rotation.row(2)) * accel_weight;
 
    // Add a "torque" that makes our east vector (rotation.row(0))
    // get closer to east vector calculated from the compass.
    //vector errorYaw = rotationCompass.row(0).cross(rotation.row(0));

    //vector drift_correction = errorYaw * Kp_YAW + errorRollPitch * Kp_ROLLPITCH;

    //rotation *= updateMatrix(angular_velocity + drift_correction, dt);
    //rotation = normalize(rotation);
}

void ahrs(IMU& imu, fuse_function * fuse_func)
{
    imu.loadCalibration();
    imu.enable();
    imu.measureOffsets();
    
    // The quaternion that can convert a vector in body coordinates
    // to ground coordinates when it its changed to a matrix.
    quaternion rotation = quaternion::Identity();

    int start = millis(); // truncate 64-bit return value
    while(1)
    {
        int last_start = start;
        start = millis();
        float dt = (start-last_start)/1000.0;
        if (dt < 0){ throw std::runtime_error("time went backwards"); }

        vector angular_velocity = imu.readGyro();
        vector acceleration = imu.readAcc();
        vector magnetic_field = imu.readMag();

        fuse_func(rotation, dt, angular_velocity, acceleration, magnetic_field);

        matrix r = rotation.matrix();
        printf("%7.4f %7.4f %7.4f %7.4f %7.4f %7.4f %7.4f %7.4f %7.4f  %7.4f %7.4f %7.4f  %7.4f %7.4f %7.4f\n",
               r(0,0), r(0,1), r(0,2),
               r(1,0), r(1,1), r(1,2),
               r(2,0), r(2,1), r(2,2),
               acceleration(0), acceleration(1), acceleration(2),
               magnetic_field(0), magnetic_field(1), magnetic_field(2));
        fflush(stdout);

        // Ensure that each iteration of the loop takes at least 20 ms.
        while(millis() - start < 20)
        {
            usleep(1000);
        }
    }
}

int main(int argc, char *argv[])
{
    try
    {
        MinIMU9 imu("/dev/i2c-0");
        
        imu.checkConnection();

        if (argc > 1)
        {
            const char * action = argv[1];
            if (0 == strcmp("raw", action))
            {
                streamRawValues(imu);
            }
            else if (0 == strcmp("gyro-only", action))
            {
                ahrs(imu, &fuse_gyro_only);
            }
            else if (0 == strcmp("compass-only", action))
            {
                ahrs(imu, &fuse_compass_only);
            }
            else
            {
                fprintf(stderr, "Unknown action '%s'.\n", action);
                return 3;
            }
        }
        else
        {
            ahrs(imu, &fuse_default);
        }        
        return 0;
    }
    catch(const std::system_error & error)
    {
        std::string what = error.what();
        const std::error_code & code = error.code();
        std::cerr << "Error: " << what << "  " << code.message() << " (" << code << ")\n";
    }
    catch(const std::exception & error)    
    {
        std::cerr << "Error: " << error.what() << '\n';
    }
    return 1;
}
