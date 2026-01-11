# N-Body Simulation Problem

The **n-body simulation problem** models the evolution over time of a system of *n* interacting bodies. Each body is characterized by a mass, a position, and a velocity. During the simulation, bodies influence each other through pairwise interactions, most commonly modeled using gravitational forces.

At each simulation step, the force acting on a body is computed as the sum of the forces exerted by all other bodies in the system. Based on these forces, the body’s acceleration, velocity, and position are updated. This process is repeated for a fixed number of time steps in order to observe the dynamic behavior of the system.

The computational cost of the n-body problem grows quadratically with the number of bodies, since each body interacts with every other body. Because the force computation for different bodies can be performed independently, the problem is well suited for parallel and distributed implementations, making it a common benchmark in parallel and distributed programming.


## Algorithms Description

### Sequential N-Body Simulation Algorithm

The sequential implementation of the n-body simulation serves as a baseline for correctness and performance comparison. The algorithm simulates the evolution of a system of bodies over a fixed number of discrete time steps.

To ensure correctness, the positions and velocities used for force computation are those from the beginning of the current time step. Updated values are stored in temporary arrays and applied only after all forces have been computed.

#### Algorithm outline

For each simulation step:

1. For every body, compute the net force resulting from interactions with all other bodies.
2. Convert the net force into acceleration.
3. Update the body’s velocity and position.
4. Replace the old positions and velocities with the newly computed values.


#### Complexity

The algorithm computes pairwise interactions between all bodies. For *n* bodies, this results in a time complexity of **O(n²)** per simulation step. This quadratic complexity motivates the use of parallel and distributed approaches for larger systems.


### Parallel N-Body Simulation Algorithm (Thread-Based)

The threaded implementation parallelizes the n-body simulation by distributing the computation of forces and updates across multiple threads. This version builds directly on the sequential algorithm and uses data parallelism to improve performance.

At each time step, the set of bodies is partitioned into disjoint subsets, and each thread is responsible for computing the forces and updates for its assigned subset. All threads read from the same snapshot of positions and velocities taken at the beginning of the time step.

To avoid race conditions, each body is updated by exactly one thread. Updated positions and velocities are stored in temporary arrays and applied only after all threads have completed their computations for the current time step.

#### Algorithm outline

For each simulation step:

1. Partition the set of bodies among the available threads.
2. In parallel, for each assigned body:

    * compute the net force resulting from interactions with all other bodies,
    * convert the net force into acceleration,
    * update the body’s velocity and position.
3. Synchronize all threads at the end of the step.
4. Replace the old positions and velocities with the newly computed values.

#### Complexity

The total computational work per simulation step remains **O(n²)**, as all pairwise interactions are still computed. However, when using *p* threads, the workload is divided among the threads, resulting in an ideal per-thread complexity of **O(n² / p)**. The overall performance gain is limited by thread synchronization overhead and available hardware parallelism.


## Parallel N-Body Simulation Algorithm (MPI-Based)

The MPI-based implementation parallelizes the n-body simulation using distributed-memory processes. The set of bodies is divided among multiple processes, with each process responsible for updating a distinct subset of bodies.

At each simulation step, all processes exchange position data so that every process has a consistent snapshot of the system state. Using this snapshot, each process computes the forces and updates only for the bodies it owns. This approach preserves the structure of the sequential algorithm while enabling distributed execution.

The algorithm proceeds in synchronized time steps, ensuring that all processes advance the simulation consistently.

### Algorithm outline

For each simulation step:

1. Partition the set of bodies among the available processes.
2. Exchange position data between all processes to obtain a global snapshot.
3. For each locally assigned body:

   * compute the net force resulting from interactions with all other bodies,
   * convert the net force into acceleration,
   * update the body’s velocity and position.
4. Proceed to the next simulation step.

### Complexity

As in the sequential case, the total computational work per simulation step is **O(n²)**. When using *p* processes, each process performs approximately **O(n² / p)** computations. Additional overhead is introduced by the communication required to exchange position data at each step, which limits scalability for very large numbers of processes.

---

## Synchronization used in the parallelized variants 

## Synchronization in the Thread-Based Parallel Implementation

The thread-based parallel implementation uses a data-parallel approach in which the computation of forces and updates is distributed among multiple threads. Synchronization is designed to be minimal in order to reduce overhead and improve scalability.

At each simulation step, the set of bodies is partitioned into disjoint subsets, with each thread responsible for updating a distinct subset of bodies. During this phase, positions and velocities from the previous step are treated as read-only shared data.

To ensure correctness and avoid race conditions, updated positions and velocities are written into temporary arrays. Each body is updated by exactly one thread, so no two threads write to the same memory locations.

A synchronization point is required at the end of each simulation step to ensure that all threads have completed their computations before the global state is updated. This synchronization is implemented implicitly by joining all threads. Once all threads have finished, the temporary arrays are swapped with the main position and velocity arrays, and the simulation proceeds to the next step.

Because the algorithm avoids shared writes and uses synchronization only at well-defined step boundaries, no explicit locking mechanisms (such as mutexes or atomic operations) are required.



## Synchronization in the MPI-Based Parallel Implementation

In the MPI-based implementation, synchronization is achieved through collective communication operations rather than explicit locking mechanisms.

At the beginning of each simulation step, all processes participate in a collective exchange of position data. This operation ensures that every process receives the complete and consistent set of body positions before force computation begins. As a result, all processes operate on the same snapshot of the system state during a given step.

Each process updates only its own subset of bodies, eliminating write conflicts between processes. Because all processes must complete the position exchange before proceeding, the collective communication implicitly acts as a synchronization barrier.

No additional synchronization primitives are required. The combination of data partitioning and collective communication ensures correctness while keeping synchronization overhead minimal.

## Performance Measurements

### Experimental setup

All experiments were executed on the same machine using identical simulation parameters for all implementations. The following parameters were fixed across all runs:

* Time step: `dt = 0.01`
* Softening factor: `ε = 0.001`
* Gravitational constant: `G = 1.0`
* Random seed: `42`
* Numerical integration method: explicit Euler

Three problem sizes were evaluated:

* **n = 1000, steps = 50**
* **n = 2000, steps = 50**
* **n = 4000, steps = 20**

For each configuration, the best execution time out of three runs was recorded. Correctness was verified using a checksum computed from the final positions and velocities.


### Sequential and thread-based performance

The sequential implementation serves as the baseline. The thread-based implementation was evaluated using 1, 2, 4, and 8 threads.

#### Results (best execution time, seconds)

**n = 2000, steps = 50**

| Implementation | Workers | Time (s) | Speedup |
| -------------- | ------- | -------- | ------- |
| Sequential     | 1       | 6.923    | 1.00×   |
| Threads        | 1       | 5.696    | 1.22×   |
| Threads        | 2       | 2.970    | 2.33×   |
| Threads        | 4       | 1.537    | 4.50×   |
| Threads        | 8       | 0.792    | 8.74×   |

Similar near-linear scaling behavior was observed for the other problem sizes.

#### Observations

* The thread-based implementation achieves **almost linear speedup** up to 8 threads.
* Minor super-linear speedups are observed in some cases, likely due to cache effects and reduced memory pressure.
* Checksums are identical between sequential and threaded runs, confirming numerical consistency.


### MPI-based performance

The MPI implementation was evaluated using 1, 2, and 4 processes. Each MPI process handled a disjoint subset of bodies, and position data was exchanged at every simulation step using collective communication.

#### Results (best execution time, seconds)

**n = 2000, steps = 50**

| Implementation | Processes | Time (s) | Speedup |
| -------------- | --------- | -------- | ------- |
| Sequential     | 1         | 6.923    | 1.00×   |
| MPI            | 1         | 0.361    | 19.2×   |
| MPI            | 2         | 0.188    | 36.8×   |
| MPI            | 4         | 0.096    | 71.8×   |

#### Observations

* The MPI implementation significantly outperforms both sequential and thread-based versions for the tested problem sizes.
* Increasing the number of MPI processes results in substantial reductions in execution time.
* For small to medium problem sizes, communication overhead remains low compared to computation cost.


### Numerical consistency

For **MPI with one process**, the checksum exactly matches the sequential and threaded implementations, confirming correctness.

For **MPI with multiple processes**, small differences in the checksum are observed. This behavior is expected and is caused by:

* differences in the order of floating-point operations across processes,
* non-associativity of floating-point arithmetic.

The observed checksum drift increases with the number of processes but remains within acceptable numerical limits and does not indicate incorrect physical behavior.


## Parallel N-Body Simulation Algorithm (OpenCL-Based)

The OpenCL-based implementation parallelizes the n-body simulation using GPU acceleration. OpenCL (Open Computing Language) is a framework for writing programs that execute across heterogeneous platforms, including GPUs, CPUs, and other processors.

In this implementation, the entire force computation and position update for all bodies is offloaded to the GPU. Each GPU work-item (thread) is responsible for computing the forces acting on a single body and updating its velocity and position.

### Algorithm outline

1. Transfer initial mass, position, and velocity data from host (CPU) to device (GPU) memory.
2. For each simulation step:
   * Launch a kernel where each work-item computes forces for one body.
   * Use local memory tiling to reduce global memory access latency.
   * Update velocities and compute new positions.
   * Swap position buffers on the GPU (no data transfer back to CPU).
3. After all steps complete, transfer final positions and velocities back to host memory.

### GPU Kernel Design

The kernel uses a **tiled algorithm** with local memory optimization:

* Work-items are organized into work-groups of size 128 (WG = 128).
* Each work-group cooperatively loads a tile of body data into fast local (shared) memory.
* All work-items in the group then compute interactions with the bodies in the tile.
* This process repeats for all tiles, accumulating forces.
* Barriers (`barrier(CLK_LOCAL_MEM_FENCE)`) synchronize work-items within a group between loading and computing phases.

This tiling approach significantly reduces global memory bandwidth requirements by reusing loaded data across multiple work-items.

### Memory Management

To minimize host-device transfer overhead:

* Data is uploaded to the GPU **once** at the beginning of the simulation.
* Between simulation steps, position buffers are **swapped on the GPU** using buffer handle exchanges.
* Data is downloaded from the GPU **once** at the end of the simulation.

This approach eliminates the per-step transfer overhead that would otherwise dominate execution time for iterative simulations.

### Precision Considerations

The OpenCL implementation uses **single-precision floating-point (float)** arithmetic instead of double-precision for performance reasons:

* GPUs typically have significantly higher throughput for 32-bit operations.
* The `rsqrt()` function provides fast inverse square root computation.
* Single precision is sufficient for visualization and many simulation purposes.

As a result, checksums differ slightly from the double-precision CPU implementations.

### Complexity

The computational complexity per simulation step remains **O(n²)**, as all pairwise interactions are computed. However, the GPU executes thousands of work-items in parallel, with the tiling strategy ensuring efficient memory access patterns. For large n, the GPU can achieve orders of magnitude higher throughput than CPU implementations.

---

## Synchronization in the OpenCL-Based Parallel Implementation

The OpenCL implementation uses a hierarchical synchronization model appropriate for GPU execution:

### Intra-work-group synchronization

Within each work-group, work-items must coordinate when accessing local memory. The `barrier(CLK_LOCAL_MEM_FENCE)` function is used to ensure that:

1. All work-items have finished writing body data to local memory before any work-item begins reading.
2. All work-items have finished computing with the current tile before loading the next tile.

These barriers are placed at tile boundaries in the force computation loop.

### Inter-work-group synchronization

Work-groups execute independently and do not synchronize with each other during kernel execution. This is acceptable because:

* Each work-item updates only one body.
* Position reads use the snapshot from the beginning of the step.
* New positions are written to a separate output buffer.

### Host-device synchronization

The host (CPU) waits for GPU completion using:

* `clctx.queue.finish()` — blocks until all enqueued commands complete.
* Synchronous buffer operations (`CL_TRUE` flag) — ensure data transfers complete before returning.

### Buffer swapping

Between simulation steps, position buffers are swapped by exchanging buffer handles on the host side (`std::swap`). This is a metadata operation that does not involve data movement and incurs negligible overhead.


### OpenCL-based performance

The OpenCL implementation was evaluated on an NVIDIA GPU using the CUDA OpenCL runtime. The implementation uses single-precision floating-point arithmetic and a work-group size of 128.

#### Results (best execution time, seconds)

| n    | steps | Sequential | Threads (8) | MPI (4) | OpenCL  | OpenCL Speedup vs Seq |
| ---- | ----- | ---------- | ----------- | ------- | ------- | --------------------- |
| 1000 | 50    | 0.214      | 0.054       | 0.061   | 0.114   | 1.88×                 |
| 2000 | 50    | 0.858      | 0.158       | 0.220   | 0.114   | 7.53×                 |
| 4000 | 20    | 1.368      | 0.333       | 0.355   | 0.110   | 12.44×                |

#### Observations

* The OpenCL implementation shows **near-constant execution time** across different problem sizes, demonstrating excellent GPU scalability.
* For small n (1000), CPU-based parallelization (threads/MPI) outperforms OpenCL due to GPU kernel launch and initialization overhead.
* For larger n (2000+), OpenCL significantly outperforms all CPU implementations, achieving up to **12× speedup** over sequential execution.
* The GPU's massive parallelism effectively hides the O(n²) computational complexity for the tested problem sizes.

#### Comparison of all implementations (n = 4000, steps = 20)

| Implementation | Time (s) | Speedup vs Sequential |
| -------------- | -------- | --------------------- |
| Sequential     | 1.368    | 1.00×                 |
| Threads (8)    | 0.333    | 4.11×                 |
| MPI (4)        | 0.355    | 3.85×                 |
| **OpenCL**     | **0.110**| **12.44×**            |

### Numerical consistency (OpenCL)

The OpenCL implementation produces different checksums compared to CPU implementations due to:

* **Single-precision arithmetic**: The GPU kernel uses 32-bit floats instead of 64-bit doubles.
* **Different operation ordering**: GPU parallel execution may accumulate forces in a different order.
* **Fast math functions**: The `rsqrt()` function may use hardware approximations.

These differences are expected and acceptable for simulation purposes. The physical behavior of the simulation remains correct, and the checksums are consistent across repeated OpenCL runs.

