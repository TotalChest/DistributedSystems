# DistributedSystems
Practice for the distributed systems course

### CircleAlgorithm
```bash
$ mpicc circle_algorithm.c -o circle_algorithm
$ mpirun -oversubscribe -n 36 circle_algorithm <INITIATOR NUMBER>
```

### Seidel-2d (ULFM)
```bash
$ mkdir CP
$ mpicc seidel-2d.c -o seidel-2d # MPI with ULFM
$ mpirun -oversubscribe -n 11 seidel-2d # MPI with ULFM
```