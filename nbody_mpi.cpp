#include <vector>
#include <mpi.h>
#include <cmath>
#include <algorithm>
#include "nbody.h"

void simulateMPI(
    int n, int steps,
    const std::vector<double>& mass,
    std::vector<Vec3>& pos,
    std::vector<Vec3>& vel,
    double G, double dt, double epsilon,
    MPI_Comm comm
) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    // Determine local range of bodies for this process
    int chunk = (n + size - 1) / size;
    int start = chunk * rank;
    int end = std::min(start + chunk, n);
    int localN = std::max(end - start, 0);

    std::vector<Vec3> newPos(localN), newVel(localN);
    std::vector<Vec3> allPos(n), allVel(n);

    // Precompute counts and displacements for MPI_Allgatherv
    std::vector<int> counts(size), displs(size);
    for (int r = 0; r < size; r++) {
        int s = r * chunk;
        int e = std::min(s + chunk, n);
        counts[r] = std::max(e - s, 0) * 3; // 3 doubles per Vec3
        displs[r] = s * 3;
    }

    for (int step = 0; step < steps; step++) {
        // Gather all possitions from all processes
        MPI_Allgatherv(
            pos.data() + start,
            localN * 3,
            MPI_DOUBLE,
            allPos.data(),
            counts.data(),
            displs.data(),
            MPI_DOUBLE,
            comm
            );

        // Compute forces and update only local bodies
        for (int idx = 0; idx < localN; idx++) {
            int i = start + idx;
            Vec3 force{0, 0, 0};

            for (int j = 0; j < n; j++) {
                if (i != j) {
                    Vec3 r = allPos[j] - allPos[i];
                    double dist2 = ddot(r, r) + epsilon * epsilon;
                    double invDist = 1.0 / sqrt(dist2);
                    double invDist3 = invDist * invDist * invDist;

                    double coef = G * mass[i] * mass[j] * invDist3;
                    force += r * coef;
                }
            }

            Vec3 acc = force * (1.0 / mass[i]);
            newVel[idx] = vel[i] + acc * dt;
            newPos[idx] = pos[i] + newVel[idx] * dt;
        }

        // Write local updates back to global arrays
        for (int idx = 0; idx < localN; idx++) {
            pos[start + idx] = newPos[idx];
            vel[start + idx] = newVel[idx];
        }
    }
}