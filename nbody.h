#ifndef PROJECT_NBODY_H
#define PROJECT_NBODY_H

#include <vector>
#include <mpi.h>

struct Vec3 {
    double x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& other) const { return {x + other.x, y + other.y, z + other.z}; }
    Vec3 operator-(const Vec3& other) const { return {x - other.x, y - other.y, z - other.z}; }
    Vec3 operator*(double scalar) const { return {x * scalar, y * scalar, z * scalar}; }
    Vec3& operator+=(const Vec3& other) { x += other.x; y += other.y; z += other.z; return *this; }
};

inline double ddot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

void simulateSequential(
    int n, int steps,
    const std::vector<double>& mass,
    std::vector<Vec3>& pos,
    std::vector<Vec3>& vel,
    double G, double dt, double epsilon
);

void simulateThreaded(
    int n, int steps,
    const std::vector<double>& mass,
    std::vector<Vec3>& pos,
    std::vector<Vec3>& vel,
    double G, double dt, double epsilon,
    int numThreads
);

void simulateMPI(
    int n, int steps,
    const std::vector<double>& mass,
    std::vector<Vec3>& pos,
    std::vector<Vec3>& vel,
    double G, double dt, double epsilon,
    MPI_Comm comm
);

#endif //PROJECT_NBODY_H