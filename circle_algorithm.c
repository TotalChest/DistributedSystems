#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include "mpi.h"
#include <string.h>

#define ELECTION 100
#define OK 200     
#define COORDINATOR 300

const int N = 36;

double get_uniform_rand(double a, double b) {
    return (double) rand() / RAND_MAX * (b - a) + a;
}

double work_init() {
    double r = get_uniform_rand(0, 1);
    if (r < 0.5) {
        return true;
    }
    return false;
}

int max_non_zero_index(int *array, int size) {
    for (int i = size - 1; i >= 0; i -= 1) {
        if (array[i] != 0) {
            return i;
        }
    }
}

int send_to_next(int rank, int size, int *array, int tag){
    MPI_Request request;
    MPI_Status status;
    int answer;
    int sended = 0;
    int next = 1;
    while (!sended){
        MPI_Isend(array, size, MPI_INT, (rank + next)%size, tag, MPI_COMM_WORLD, &request);
        MPI_Irecv(&answer, 1, MPI_INT, (rank + next)%size, OK, MPI_COMM_WORLD, &request);

        double start = MPI_Wtime();
        while (!sended) {
            MPI_Test(&request, &sended, &status);
            if (MPI_Wtime() - start >= 1) {
                printf("%d: Подтверждение от %d не пришло.\n", rank, (rank + next)%size);
                fflush(stdout);
                MPI_Cancel(&request);
                MPI_Request_free(&request);
                break;
            }
        }
        next += 1;
    }
    return status.MPI_SOURCE;
}

int main(int argc, char* argv[]) {

    setvbuf( stdout, NULL, _IOLBF, BUFSIZ );
    setvbuf( stderr, NULL, _IOLBF, BUFSIZ );

    int size;
    int rank;


    MPI_Init(&argc, &argv);               
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    srand(time(NULL) * rank);

    int circle_start = 0;
    if (argc == 2) {
        circle_start = atoi(argv[1]);
    }

    if (size != N || circle_start >= N) {
        if (rank == 0) {
            if (size != N) {
                printf("Задано неверное колличество процессов: ожидается 36\n");
            }
            if (circle_start >= N) {
                printf("Задан неверный номер первого процесса: ожидается номер меньше 36\n");
            }
        }
        MPI_Finalize();
        return 0;
    }

    if (rank == circle_start){
        printf("Созданные процессы транспьютерной матрицы:\n");
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    printf("%d ", rank);
    fflush(stdout);
    
    // убиваем случайные процессы
    bool state = work_init(); // состояние - работает / не работает
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == circle_start){
        state = true;
        printf("\nЖивые процессы матрицы перед началом кругового алгоритма:\n");
        fflush(stdout);
    }

    MPI_Barrier(MPI_COMM_WORLD);    
    if (state == true){
        printf("%d ", rank);
        fflush(stdout);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    
    if (state == false) {
        MPI_Finalize();
        return 0;
    }
     
    // массив для передачи сообщений
    int* array = (int*)malloc(size*sizeof(int));
    for (int i = 0; i < size; i += 1) {
        array[i] = 0;
    }
    MPI_Request request;
    MPI_Status status;
    int next;

    // отправляем сообщение от стартового процесса следующему со своим номером
    if (rank == circle_start){
        printf("\n%d: Запустил круговой алгоритм\n", rank);
        fflush(stdout);
        int sended = 0;
        array[rank] = 1;
        next = send_to_next(rank, size, array, ELECTION); // запоминаем следующий живой процесс
    }

    // Выборы
    int answer = 1;
    int new_coordinator;
    int start_coordinator_circle = 0; // флаг процесса, который начинает рассылку КООРДИНАТОР
    MPI_Irecv(array, size, MPI_INT, MPI_ANY_SOURCE, ELECTION, MPI_COMM_WORLD, &request);
    MPI_Wait(&request, &status);
    printf("%d: Получил массив от %d\n", rank, status.MPI_SOURCE);
    fflush(stdout);
    if (array[rank] == 1) {
        start_coordinator_circle = 1;
        new_coordinator = max_non_zero_index(array, size);
        printf("%d: Новый координатор: %d\n", rank, new_coordinator);
        fflush(stdout);
        MPI_Isend(&answer, 1, MPI_INT, status.MPI_SOURCE, OK, MPI_COMM_WORLD, &request);
        MPI_Isend(&new_coordinator, 1, MPI_INT, next, COORDINATOR, MPI_COMM_WORLD, &request);
    }
    else {
        array[rank] = 1;
        MPI_Isend(&answer, 1, MPI_INT, status.MPI_SOURCE, OK, MPI_COMM_WORLD, &request);
        next = send_to_next(rank, size, array, ELECTION); // запоминаем следующий живой процесс
    }
    
    // Рассылка координатора
    MPI_Irecv(&new_coordinator, 1, MPI_INT, MPI_ANY_SOURCE, COORDINATOR, MPI_COMM_WORLD, &request);
    MPI_Wait(&request, &status);
    if (!start_coordinator_circle) {
        printf("%d: Новый координатор: %d\n", rank, new_coordinator);
        fflush(stdout);
        MPI_Isend(&new_coordinator, 1, MPI_INT, next, COORDINATOR, MPI_COMM_WORLD, &request);
    }

    MPI_Finalize();
    return 0;
}
