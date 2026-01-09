#include <vector>
#include <thread>
#include <cmath>
#include <algorithm>
#include "nbody.h"

void simulateThreaded(
    int n, int steps,
    const std::vector<double>& mass,
    std::vector<Vec3>& pos,
    std::vector<Vec3>& vel,
    double G, double dt, double epsilon,
    int numThreads
) {
    std::vector<Vec3> newPos(n), newVel(n);

    // Simulation loop for time steps
    for (int step = 0; step < steps; step++) {
        std::vector<std::thread> threads;

        for (int t = 0; t < numThreads; t++) {
            threads.emplace_back([&, t]() {
                int chunk = (n + numThreads - 1) / numThreads;
                int start = chunk * t;
                int end = std::min(start + chunk, n);

                // Compute forces and update each body independently
                for (int i = start; i < end; i++) {
                    Vec3 force{0, 0, 0};

                    // Accumulate force contributions from all other bodies
                    for (int j = 0; j < n; j++) {
                        if (i != j) {
                            Vec3 r = pos[j] - pos[i];

                            // Squared distance with softening to avoid singularities
                            double dist2 = ddot(r, r) + epsilon * epsilon;

                            // Inverse distance cubed
                            double invDist = 1.0 / sqrt(dist2);
                            double invDist3 = invDist * invDist * invDist;

                            // Gravitational force contribution
                            double coef = G * mass[i] * mass[j] * invDist3;
                            force += r * coef;
                        }
                    }

                    // Convert forces to acceleration
                    Vec3 acc = force * (1.0 / mass[i]);

                    // Update velocity and position (Euler integration)
                    newVel[i] = vel[i] + acc * dt;
                    newPos[i] = pos[i] + newVel[i] * dt;
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        // Apply updates for the next step
        pos.swap(newPos);
        vel.swap(newVel);
    }
}