# N-Body Simulation Problem

The **n-body simulation problem** models the evolution over time of a system of *n* interacting bodies. Each body is characterized by a mass, a position, and a velocity. During the simulation, bodies influence each other through pairwise interactions, most commonly modeled using gravitational forces.

At each simulation step, the force acting on a body is computed as the sum of the forces exerted by all other bodies in the system. Based on these forces, the body’s acceleration, velocity, and position are updated. This process is repeated for a fixed number of time steps in order to observe the dynamic behavior of the system.

The computational cost of the n-body problem grows quadratically with the number of bodies, since each body interacts with every other body. Because the force computation for different bodies can be performed independently, the problem is well suited for parallel and distributed implementations, making it a common benchmark in parallel and distributed programming.


## Algorithms Description

### Sequential N-Body Simulation Algorithm

The sequential implementation of the n-body simulation serves as a baseline for correctness and performance comparison. The algorithm simulates the evolution of a system of bodies over a fixed number of discrete time steps.

At each time step, the net force acting on each body is computed as the sum of the pairwise forces exerted by all other bodies. Based on this force, the body’s acceleration, velocity, and position are updated using a simple numerical integration method.

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

