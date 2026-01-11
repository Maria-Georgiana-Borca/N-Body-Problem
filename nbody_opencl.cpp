#define CL_HPP_TARGET_OPENCL_VERSION 200
#define CL_HPP_MINIMUM_OPENCL_VERSION 200
#include <CL/opencl.hpp>

#include <vector>
#include <iostream>
#include "nbody.h"

#define WG 128

struct OpenCLContext {
    cl::Context context;
    cl::Device device;
    cl::CommandQueue queue;
    cl::Program program;
    cl::Kernel kernel;

    cl::Buffer bufMass;
    cl::Buffer bufPosX, bufPosY, bufPosZ;
    cl::Buffer bufVelX, bufVelY, bufVelZ;
    cl::Buffer bufNewPosX, bufNewPosY, bufNewPosZ;

    int n;

    OpenCLContext(int n) : n(n) {
        std::vector<cl::Platform> platforms;
        cl::Platform::get(&platforms);
        auto platform = platforms[0];

        std::vector<cl::Device> devices;
        platform.getDevices(CL_DEVICE_TYPE_GPU, &devices);
        device = devices[0];

        context = cl::Context(device);
        queue = cl::CommandQueue(context, device);

        const std::string src = R"(
#define WG 128

__kernel void nbody_step(
    const int n,
    const float G,
    const float dt,
    const float epsilon,
    __global const float* mass,
    __global const float* posX,
    __global const float* posY,
    __global const float* posZ,
    __global float* velX,
    __global float* velY,
    __global float* velZ,
    __global float* newPosX,
    __global float* newPosY,
    __global float* newPosZ
) {
    int i = get_global_id(0);
    int lid = get_local_id(0);
    if (i >= n) return;

    __local float lposX[WG];
    __local float lposY[WG];
    __local float lposZ[WG];
    __local float lmass[WG];

    float xi = posX[i];
    float yi = posY[i];
    float zi = posZ[i];

    float fx = 0.0f, fy = 0.0f, fz = 0.0f;

    for (int tile = 0; tile < n; tile += WG) {
        int j = tile + lid;
        if (j < n) {
            lposX[lid] = posX[j];
            lposY[lid] = posY[j];
            lposZ[lid] = posZ[j];
            lmass[lid] = mass[j];
        }
        barrier(CLK_LOCAL_MEM_FENCE);

        int limit = min(WG, n - tile);
        for (int k = 0; k < limit; k++) {
            float rx = lposX[k] - xi;
            float ry = lposY[k] - yi;
            float rz = lposZ[k] - zi;

            float dist2 = rx*rx + ry*ry + rz*rz + epsilon*epsilon;
            float invDist = rsqrt(dist2);
            float invDist3 = invDist * invDist * invDist;

            float coeff = G * lmass[k] * invDist3;

            fx += rx * coeff;
            fy += ry * coeff;
            fz += rz * coeff;
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    velX[i] += fx * dt;
    velY[i] += fy * dt;
    velZ[i] += fz * dt;

    newPosX[i] = xi + velX[i] * dt;
    newPosY[i] = yi + velY[i] * dt;
    newPosZ[i] = zi + velZ[i] * dt;
}
)";

        program = cl::Program(context, src);
        program.build({device});
        kernel = cl::Kernel(program, "nbody_step");

        bufMass    = cl::Buffer(context, CL_MEM_READ_ONLY,  n * sizeof(float));
        bufPosX    = cl::Buffer(context, CL_MEM_READ_WRITE, n * sizeof(float));
        bufPosY    = cl::Buffer(context, CL_MEM_READ_WRITE, n * sizeof(float));
        bufPosZ    = cl::Buffer(context, CL_MEM_READ_WRITE, n * sizeof(float));
        bufVelX    = cl::Buffer(context, CL_MEM_READ_WRITE, n * sizeof(float));
        bufVelY    = cl::Buffer(context, CL_MEM_READ_WRITE, n * sizeof(float));
        bufVelZ    = cl::Buffer(context, CL_MEM_READ_WRITE, n * sizeof(float));
        bufNewPosX = cl::Buffer(context, CL_MEM_READ_WRITE, n * sizeof(float));
        bufNewPosY = cl::Buffer(context, CL_MEM_READ_WRITE, n * sizeof(float));
        bufNewPosZ = cl::Buffer(context, CL_MEM_READ_WRITE, n * sizeof(float));
    }
};

void simulateOpenCL(
    int n, int steps,
    const std::vector<double>& mass,
    std::vector<Vec3>& pos,
    std::vector<Vec3>& vel,
    double G, double dt, double epsilon
) {
    OpenCLContext clctx(n);

    std::vector<float> massF(n);
    std::vector<float> posX(n), posY(n), posZ(n);
    std::vector<float> velX(n), velY(n), velZ(n);

    for (int i = 0; i < n; i++) {
        massF[i] = (float)mass[i];
        posX[i] = (float)pos[i].x;
        posY[i] = (float)pos[i].y;
        posZ[i] = (float)pos[i].z;
        velX[i] = (float)vel[i].x;
        velY[i] = (float)vel[i].y;
        velZ[i] = (float)vel[i].z;
    }

    clctx.queue.enqueueWriteBuffer(clctx.bufMass, CL_TRUE, 0, n*sizeof(float), massF.data());
    clctx.queue.enqueueWriteBuffer(clctx.bufPosX, CL_TRUE, 0, n*sizeof(float), posX.data());
    clctx.queue.enqueueWriteBuffer(clctx.bufPosY, CL_TRUE, 0, n*sizeof(float), posY.data());
    clctx.queue.enqueueWriteBuffer(clctx.bufPosZ, CL_TRUE, 0, n*sizeof(float), posZ.data());
    clctx.queue.enqueueWriteBuffer(clctx.bufVelX, CL_TRUE, 0, n*sizeof(float), velX.data());
    clctx.queue.enqueueWriteBuffer(clctx.bufVelY, CL_TRUE, 0, n*sizeof(float), velY.data());
    clctx.queue.enqueueWriteBuffer(clctx.bufVelZ, CL_TRUE, 0, n*sizeof(float), velZ.data());

    size_t global = ((n + WG - 1) / WG) * WG;

    for (int step = 0; step < steps; step++) {
        clctx.kernel.setArg(0, n);
        clctx.kernel.setArg(1, (float)G);
        clctx.kernel.setArg(2, (float)dt);
        clctx.kernel.setArg(3, (float)epsilon);
        clctx.kernel.setArg(4, clctx.bufMass);
        clctx.kernel.setArg(5, clctx.bufPosX);
        clctx.kernel.setArg(6, clctx.bufPosY);
        clctx.kernel.setArg(7, clctx.bufPosZ);
        clctx.kernel.setArg(8, clctx.bufVelX);
        clctx.kernel.setArg(9, clctx.bufVelY);
        clctx.kernel.setArg(10, clctx.bufVelZ);
        clctx.kernel.setArg(11, clctx.bufNewPosX);
        clctx.kernel.setArg(12, clctx.bufNewPosY);
        clctx.kernel.setArg(13, clctx.bufNewPosZ);

        clctx.queue.enqueueNDRangeKernel(
            clctx.kernel,
            cl::NullRange,
            cl::NDRange(global),
            cl::NDRange(WG)
        );

        std::swap(clctx.bufPosX, clctx.bufNewPosX);
        std::swap(clctx.bufPosY, clctx.bufNewPosY);
        std::swap(clctx.bufPosZ, clctx.bufNewPosZ);
    }

    clctx.queue.finish();

    clctx.queue.enqueueReadBuffer(clctx.bufPosX, CL_TRUE, 0, n*sizeof(float), posX.data());
    clctx.queue.enqueueReadBuffer(clctx.bufPosY, CL_TRUE, 0, n*sizeof(float), posY.data());
    clctx.queue.enqueueReadBuffer(clctx.bufPosZ, CL_TRUE, 0, n*sizeof(float), posZ.data());
    clctx.queue.enqueueReadBuffer(clctx.bufVelX, CL_TRUE, 0, n*sizeof(float), velX.data());
    clctx.queue.enqueueReadBuffer(clctx.bufVelY, CL_TRUE, 0, n*sizeof(float), velY.data());
    clctx.queue.enqueueReadBuffer(clctx.bufVelZ, CL_TRUE, 0, n*sizeof(float), velZ.data());

    for (int i = 0; i < n; i++) {
        pos[i] = {posX[i], posY[i], posZ[i]};
        vel[i] = {velX[i], velY[i], velZ[i]};
    }
}
