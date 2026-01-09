#include <iostream>
#include "nbody.h"

#include <mpi.h>
#include "nbody.h"

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int n = 2;
    int steps = 1;

    double G = 1.0;
    double dt = 0.01;
    double eps = 1e-3;

    std::vector<double> mass = {1.0, 1.0};
    std::vector<Vec3> pos = {
        {-1.0, 0.0, 0.0},
        { 1.0, 0.0, 0.0}
    };
    std::vector<Vec3> vel = {
        {0.0, 0.0, 0.0},
        {0.0, 0.0, 0.0}
    };

    // initialize n, steps, mass, pos, vel, etc.
    simulateMPI(n, steps, mass, pos, vel, G, dt, eps, MPI_COMM_WORLD);
    for (int i = 0; i < n; i++) {
        std::cout << "Body " << i
                  << " pos=(" << pos[i].x << ", " << pos[i].y << ", " << pos[i].z << ") "
                  << " vel=(" << vel[i].x << ", " << vel[i].y << ", " << vel[i].z << ")\n";
    }

    MPI_Finalize();
    return 0;
}



// int main() {
//     int n = 2;
//     int steps = 1;
//
//     double G = 1.0;
//     double dt = 0.01;
//     double eps = 1e-3;
//
//     std::vector<double> mass = {1.0, 1.0};
//     std::vector<Vec3> pos = {
//         {-1.0, 0.0, 0.0},
//         { 1.0, 0.0, 0.0}
//     };
//     std::vector<Vec3> vel = {
//         {0.0, 0.0, 0.0},
//         {0.0, 0.0, 0.0}
//     };
//
//     // simulateSequential(n, steps, mass, pos, vel, G, dt, eps);
//     simulateThreaded(n, steps, mass, pos, vel, G, dt, eps, 4);
//
//     for (int i = 0; i < n; i++) {
//         std::cout << "Body " << i
//                   << " pos=(" << pos[i].x << ", " << pos[i].y << ", " << pos[i].z << ") "
//                   << " vel=(" << vel[i].x << ", " << vel[i].y << ", " << vel[i].z << ")\n";
//     }
//
//     return 0;
// }