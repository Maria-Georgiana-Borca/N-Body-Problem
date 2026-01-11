#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <functional>
#include "nbody.h"

#ifdef USE_MPI
  #include <mpi.h>
#endif

// -------------------- deterministic initialization --------------------
static void initBodies(int n,
                       std::vector<double>& mass,
                       std::vector<Vec3>& pos,
                       std::vector<Vec3>& vel,
                       unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> distPos(-1.0, 1.0);
    std::uniform_real_distribution<double> distVel(-0.01, 0.01);
    std::uniform_real_distribution<double> distMass(0.5, 2.0);

    mass.resize(n);
    pos.resize(n);
    vel.resize(n);

    for (int i = 0; i < n; i++) {
        mass[i] = distMass(rng);
        pos[i]  = Vec3{distPos(rng), distPos(rng), distPos(rng)};
        vel[i]  = Vec3{distVel(rng), distVel(rng), distVel(rng)};
    }
}

// -------------------- checksum (sanity check) --------------------
static double checksumState(const std::vector<double>& mass,
                            const std::vector<Vec3>& pos,
                            const std::vector<Vec3>& vel) {
    double s = 0.0;
    int n = (int)mass.size();
    for (int i = 0; i < n; i++) {
        s += mass[i] * (pos[i].x * 0.11 + pos[i].y * 0.13 + pos[i].z * 0.17);
        s += (vel[i].x * 0.19 + vel[i].y * 0.23 + vel[i].z * 0.29);
    }
    return s;
}

// -------------------- timing helper --------------------
template <class F>
static double timeSeconds(F&& fn) {
    auto t0 = std::chrono::steady_clock::now();
    fn();
    auto t1 = std::chrono::steady_clock::now();
    std::chrono::duration<double> dt = t1 - t0;
    return dt.count();
}

// -------------------- CSV helper --------------------
static void appendCsvRow(const std::string& path,
                         const std::string& impl,
                         int workers,
                         int n, int steps,
                         double dt, double eps, double G,
                         unsigned seed,
                         int repeats,
                         double bestTime,
                         double checksum) {
    // Create file with header if it doesn't exist yet
    std::ifstream in(path);
    bool exists = in.good();
    in.close();

    std::ofstream out(path, std::ios::app);
    if (!exists) {
        out << "impl,workers,n,steps,dt,eps,G,seed,repeats,best_time_s,checksum\n";
    }

    out << impl << "," << workers << ","
        << n << "," << steps << ","
        << dt << "," << eps << "," << G << ","
        << seed << "," << repeats << ","
        << std::setprecision(10) << bestTime << ","
        << std::setprecision(10) << checksum
        << "\n";
}

// -------------------- benchmark config --------------------
struct BenchConfig {
    int n;
    int steps;
};

static double runBestOfRepeats(int repeats, const std::function<void()>& runOnce) {
    double best = 1e100;
    for (int r = 0; r < repeats; r++) {
        best = std::min(best, timeSeconds(runOnce));
    }
    return best;
}

// ----------------------------------------------------------------------------
int main(int argc, char** argv) {
#ifdef USE_MPI
    MPI_Init(&argc, &argv);
    int rank = 0, size = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
#else
    int rank = 0, size = 1;
#endif

    if (argc < 2) {
        if (rank == 0) {
            std::cerr
                << "Usage:\n"
                << "  " << argv[0] << " bench         # runs seq + threads benchmark suite\n"
                << "  " << argv[0] << " bench_mpi     # runs MPI benchmark suite (launch with mpirun)\n"
                << "  " << argv[0] << " bench_opencl  # runs OpenCL benchmark suite\n"
                << "  " << argv[0] << " seq <n> <steps> [dt eps G seed]\n"
                << "  " << argv[0] << " threads <n> <steps> <numThreads> [dt eps G seed]\n"
#ifdef USE_MPI
                << "  " << argv[0] << " mpi <n> <steps> [dt eps G seed]   (launch with mpirun)\n";
#else
                << "  (MPI disabled in this build)\n";
#endif
        }
#ifdef USE_MPI
        MPI_Finalize();
#endif
        return 1;
    }

    // Shared params (keep them identical across experiments)
    double dt = 0.01;
    double eps = 1e-3;
    double G = 1.0;
    unsigned seed = 42;

    // Benchmark suite (edit if you want)
    std::vector<BenchConfig> tests = {
        {1000, 50},
        {2000, 50},
        {4000, 20}
    };

    // Thread/MPI worker counts (edit if you want)
    std::vector<int> threadCounts = {1, 2, 4, 8};
    int repeats = 3; // best-of-3 (simple + robust)

    std::string cmd = argv[1];
    const std::string csvPath = "benchmark_results.csv";

    // -------------------- BENCH: seq + threads --------------------
    if (cmd == "bench") {
        if (rank != 0) {
            // Non-MPI run: rank is always 0, but keep it safe.
#ifdef USE_MPI
            MPI_Finalize();
#endif
            return 0;
        }

        std::cout << "Running benchmark suite (seq + threads)...\n";
        for (const auto& tc : tests) {
            int n = tc.n, steps = tc.steps;

            // Init once per test, then re-init before each run to keep runs identical
            std::vector<double> mass0;
            std::vector<Vec3> pos0, vel0;
            initBodies(n, mass0, pos0, vel0, seed);

            // --- sequential ---
            {
                auto mass = mass0;
                auto pos = pos0;
                auto vel = vel0;

                double best = runBestOfRepeats(repeats, [&]() {
                    // reset state before each repeat
                    pos = pos0; vel = vel0;
                    simulateSequential(n, steps, mass, pos, vel, G, dt, eps);
                });

                double ck = checksumState(mass, pos, vel);
                std::cout << "seq     n=" << n << " steps=" << steps
                          << " best=" << best << "s"
                          << " checksum=" << ck << "\n";
                appendCsvRow(csvPath, "seq", 1, n, steps, dt, eps, G, seed, repeats, best, ck);
            }

            // --- threads ---
            for (int th : threadCounts) {
                auto mass = mass0;
                auto pos = pos0;
                auto vel = vel0;

                double best = runBestOfRepeats(repeats, [&]() {
                    pos = pos0; vel = vel0;
                    simulateThreaded(n, steps, mass, pos, vel, G, dt, eps, th);
                });

                double ck = checksumState(mass, pos, vel);
                std::cout << "threads n=" << n << " steps=" << steps
                          << " th=" << th
                          << " best=" << best << "s"
                          << " checksum=" << ck << "\n";
                appendCsvRow(csvPath, "threads", th, n, steps, dt, eps, G, seed, repeats, best, ck);
            }
        }

        std::cout << "Saved results to " << csvPath << "\n";
#ifdef USE_MPI
        MPI_Finalize();
#endif
        return 0;
    }

    // -------------------- BENCH: OpenCL --------------------
    if (cmd == "bench_opencl") {
        if (rank != 0) {
#ifdef USE_MPI
            MPI_Finalize();
#endif
            return 0;
        }

        std::cout << "Running OpenCL benchmark suite...\n";
        for (const auto& tc : tests) {
            int n = tc.n, steps = tc.steps;

            std::vector<double> mass0;
            std::vector<Vec3> pos0, vel0;
            initBodies(n, mass0, pos0, vel0, seed);

            auto mass = mass0;
            auto pos = pos0;
            auto vel = vel0;

            double best = runBestOfRepeats(repeats, [&]() {
                pos = pos0; vel = vel0;
                simulateOpenCL(n, steps, mass, pos, vel, G, dt, eps);
            });

            double ck = checksumState(mass, pos, vel);
            std::cout << "opencl  n=" << n << " steps=" << steps
                      << " best=" << best << "s"
                      << " checksum=" << ck << "\n";
            appendCsvRow(csvPath, "opencl", 1, n, steps, dt, eps, G, seed, repeats, best, ck);
        }

        std::cout << "Saved results to " << csvPath << "\n";
#ifdef USE_MPI
        MPI_Finalize();
#endif
        return 0;
    }

    // -------------------- BENCH: MPI --------------------
    // IMPORTANT: this must be launched with mpirun -np <P> ./Project bench_mpi
    if (cmd == "bench_mpi") {
#ifndef USE_MPI
        if (rank == 0) std::cerr << "Error: bench_mpi requires MPI build (-DUSE_MPI).\n";
        return 1;
#else
        if (rank == 0) {
            std::cout << "Running MPI benchmark suite with " << size << " processes...\n";
        }

        for (const auto& tc : tests) {
            int n = tc.n, steps = tc.steps;

            std::vector<double> mass;
            std::vector<Vec3> pos, vel;
            initBodies(n, mass, pos, vel, seed);

            MPI_Barrier(MPI_COMM_WORLD);

            double best = 1e100;
            double lastChecksum = 0.0;

            for (int r = 0; r < repeats; r++) {
                initBodies(n, mass, pos, vel, seed); // reset for identical repeats
                MPI_Barrier(MPI_COMM_WORLD);

                double t = timeSeconds([&]() {
                    simulateMPI(n, steps, mass, pos, vel, G, dt, eps, MPI_COMM_WORLD);
                });

                best = std::min(best, t);

                double localCk = checksumState(mass, pos, vel);
                double globalCk = 0.0;
                MPI_Reduce(&localCk, &globalCk, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
                if (rank == 0) lastChecksum = globalCk;
            }

            if (rank == 0) {
                std::cout << "mpi     n=" << n << " steps=" << steps
                          << " np=" << size
                          << " best=" << best << "s"
                          << " checksum=" << lastChecksum << "\n";
                appendCsvRow(csvPath, "mpi", size, n, steps, dt, eps, G, seed, repeats, best, lastChecksum);
            }
        }

        if (rank == 0) {
            std::cout << "Saved results to " << csvPath << "\n";
        }

        MPI_Finalize();
        return 0;
#endif
    }

    // -------------------- Single-run modes (optional) --------------------
    // Keep your old CLI if you still want it. (Not required for benchmarking.)
    if (rank == 0) {
        std::cerr << "Unknown command: " << cmd << "\n";
        std::cerr << "Use: bench | bench_mpi | bench_opencl\n";
    }

#ifdef USE_MPI
    MPI_Finalize();
#endif
    return 1;
}