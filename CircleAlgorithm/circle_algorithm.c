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

// Отображение номеров процессов на координаты траспьютерной матрицы
char* rank2position(int rank) {
    switch(rank){
        case 0: return "(0,0)"; case 1: return "(0,1)"; case 2: return "(0,2)";
        case 3: return "(0,3)"; case 4: return "(0,4)"; case 5: return "(0,5)";
        case 6: return "(1,5)"; case 7: return "(1,4)"; case 8: return "(1,3)";
        case 9: return "(1,2)"; case 10: return "(1,1)"; case 11: return "(2,1)";
        case 12: return "(2,2)"; case 13: return "(2,3)"; case 14: return "(2,4)";
        case 15: return "(2,5)"; case 16: return "(3,5)"; case 17: return "(3,4)";
        case 18: return "(3,3)"; case 19: return "(3,2)"; case 20: return "(3,1)";
        case 21: return "(4,1)"; case 22: return "(4,2)"; case 23: return "(4,3)";
        case 24: return "(4,4)"; case 25: return "(4,5)"; case 26: return "(5,5)";
        case 27: return "(5,4)"; case 28: return "(5,3)"; case 29: return "(5,2)";
        case 30: return "(5,1)"; case 31: return "(5,0)"; case 32: return "(4,0)";
        case 33: return "(3,0)"; case 34: return "(2,0)"; case 35: return "(1,0)";
    }
}

double get_uniform_rand(double a, double b) {
    return (double) rand() / RAND_MAX * (b - a) + a;
}

// Случайное выключение некотороых процессов
double work_init() {
    double r = get_uniform_rand(0, 1);
    if (r < 0.5) {
        return true;
    }
    return false;
}

// Поиск нового координотора в массиве
int max_non_zero_index(int *array, int size) {
    for (int i = size - 1; i >= 0; i -= 1) {
        if (array[i] != 0) {
            return i;
        }
    }
}

// Попытка отправить сообщение следующему процессу в траспьютерной матрице
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
            // Ждем секунду пока не придет ответ
            if (MPI_Wtime() - start >= 1) {
                printf("%s: Подтверждение от %s не пришло.\n", rank2position(rank), rank2position((rank + next)%size));
                fflush(stdout);
                MPI_Cancel(&request);
                MPI_Request_free(&request);
                break;
            }
        }
        next += 1;
    }
    // Возвращаем номер процесса с удачной посылкой (чтобы запомнить номер живого процесса после нас)
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

    // Номер инициатора
    int circle_start;
    if (argc == 2)
        circle_start = atoi(argv[1]);
    else {
        if (rank == 0)
            printf("Использовать: mpirun --oversubscribe -n 36 a.out <НОМЕР ИНИЦИАТОРА>\n");
        MPI_Finalize();
        return 0;
    }

    // Проверка корректности введеленных данных
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
    bool state = work_init(); // состояние - (работает) / (не работает)
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
     
    // Массив для передачи сообщений
    int* array = (int*)malloc(size*sizeof(int));
    for (int i = 0; i < size; i += 1) {
        array[i] = 0;
    }
    MPI_Request request;
    MPI_Status status;
    int next;

    // Отправляем сообщение от стартового процесса следующему со своим номером
    if (rank == circle_start){
        printf("\n%s: Запустил круговой алгоритм\n", rank2position(rank));
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
    printf("%s: Получил массив от %s\n", rank2position(rank), rank2position(status.MPI_SOURCE));
    fflush(stdout);
    // Если массив уже был у нас - начинаем рассылку КООРДИНАТОР
    if (array[rank] == 1) {
        start_coordinator_circle = 1;
        new_coordinator = max_non_zero_index(array, size);
        printf("%s: Новый координатор: %s\n", rank2position(rank), rank2position(new_coordinator));
        fflush(stdout);
        MPI_Isend(&answer, 1, MPI_INT, status.MPI_SOURCE, OK, MPI_COMM_WORLD, &request);
        MPI_Isend(&new_coordinator, 1, MPI_INT, next, COORDINATOR, MPI_COMM_WORLD, &request);
    }
    // Иначе продолжаем круг
    else {
        array[rank] = 1;
        MPI_Isend(&answer, 1, MPI_INT, status.MPI_SOURCE, OK, MPI_COMM_WORLD, &request);
        next = send_to_next(rank, size, array, ELECTION); // запоминаем следующий живой процесс
    }
    
    // Рассылка координатора
    MPI_Irecv(&new_coordinator, 1, MPI_INT, MPI_ANY_SOURCE, COORDINATOR, MPI_COMM_WORLD, &request);
    MPI_Wait(&request, &status);
    if (!start_coordinator_circle) {
        printf("%s: Новый координатор: %s\n", rank2position(rank), rank2position(new_coordinator));
        fflush(stdout);
        MPI_Isend(&new_coordinator, 1, MPI_INT, next, COORDINATOR, MPI_COMM_WORLD, &request);
    }

    MPI_Finalize();
    return 0;
}
