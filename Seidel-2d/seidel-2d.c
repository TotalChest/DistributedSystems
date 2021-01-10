/* Include benchmark-specific header. */
#include "seidel-2d.h"
#include <mpi.h>
#include <mpi-ext.h>
#include <signal.h>

double bench_t_start, bench_t_end;

int ranksize, id;
MPI_Status status;
MPI_Request request;

// Информамация о сломанном процессе
int break_id = 5;
int break_k = 0;
int break_i = 7;

static
double rtclock() {
    struct timeval Tp;
    int stat;
    stat = gettimeofday (&Tp, NULL);
    if (stat != 0)
        printf ("Error return from gettimeofday: %d", stat);
    return (Tp.tv_sec + Tp.tv_usec * 1.0e-6);
}

void bench_timer_start() {
    bench_t_start = rtclock ();
}

void bench_timer_stop() {
    bench_t_end = rtclock ();
}

void bench_timer_print() {
    printf ("Time in seconds = %0.6lf\n", bench_t_end - bench_t_start);
}

static void init_array (int n, float A[ n][n]) {
    int i, j;

    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            A[i][j] = ((float) i*(j+2) + 2) / n;
}

static void print_array(int n, float A[ n][n]) {
    int i, j;

    fprintf(stderr, "==BEGIN DUMP_ARRAYS==\n");
    fprintf(stderr, "begin dump: %s", "A");
    for (i = 0; i < n; i++)
    for (j = 0; j < n; j++) {
    if ((i * n + j) % 20 == 0) fprintf(stderr, "\n");
    fprintf(stderr, "%0.2f ", A[i][j]);
    }
    fprintf(stderr, "\nend   dump: %s\n", "A");
    fprintf(stderr, "==END   DUMP_ARRAYS==\n");
}

static void save_control_point(int n, float a[n], int id, int k, int i) {
    char file_name[50];
    snprintf(file_name, 50, "CP/DATA_%d_%d_%d.txt", id, k, i);
    FILE *control_point_file = fopen(file_name, "w");

    // Записываем строку массива в файл
    for (int i = 0; i < n; i++)
        fprintf(control_point_file, "%f ", a[i]);
    fclose(control_point_file);
}

static void load_control_point(int n, float a[n], int id, int k, int i) {
    char file_name[50];
    snprintf(file_name, 50, "CP/DATA_%d_%d_%d.txt", id, k, i);
    FILE *control_point_file = fopen(file_name, "r");

    // Читаем строку массива из файла
    for (int i = 0; i < n; i++)
        fscanf(control_point_file, "%f", &a[i]);

    fclose(control_point_file);
    printf("Загружена контрольная точка %s\n", file_name);
}

static void recovery_process(int id, int k, int i) {
    int recovery_info[3], answer = 1;
    recovery_info[0] = id;
    recovery_info[1] = k;
    recovery_info[2] = i;
    // Посылаем сообщение новому процессу
    MPI_Isend(recovery_info, 3, MPI_INT, ranksize, 1220, MPI_COMM_WORLD, &request);
    // Получаем подтверждение о старте нового процесса
    MPI_Irecv(&answer, 1, MPI_INT, ranksize, 1221, MPI_COMM_WORLD, &request);
    MPI_Wait(&request, &status);
}

static void backup_process(int *recovery_info) {
    // Узнаем id сломанного процесса
    int test[ranksize], answer = 0;
    MPI_Status status_test;
    MPI_Request request_test[ranksize];
    for (int i = 0; i < ranksize; i++) {
        MPI_Irecv(&test[i], 1, MPI_INT, i, 400, MPI_COMM_WORLD, &request_test[i]);
    }
    int idx_failure = -1;
    MPI_Waitany(ranksize, request_test, &idx_failure, &status_test);

    // Ждем 2 сообщения от соседей сломанного процесса
    MPI_Status S[2];
    MPI_Request R[2];
    MPI_Irecv(recovery_info, 3, MPI_INT, idx_failure-1, 1220, MPI_COMM_WORLD, &R[1]);
    MPI_Irecv(recovery_info, 3, MPI_INT, idx_failure+1, 1220, MPI_COMM_WORLD, &R[0]);
    MPI_Waitall(2, R, S);
    // Отправить подтверждение о старте нового процесса
    MPI_Isend(&answer, 1, MPI_INT, idx_failure-1, 1221, MPI_COMM_WORLD, &request);
    MPI_Isend(&answer, 1, MPI_INT, idx_failure+1, 1221, MPI_COMM_WORLD, &request);
}

static void kernel_seidel_2d(int tsteps, int n, float A[n][n]) {

	int i, j, k, process_per_iteration, err, prev, next;

    MPI_Comm_size(MPI_COMM_WORLD, &ranksize);
    MPI_Comm_rank(MPI_COMM_WORLD, &id);
    MPI_Barrier(MPI_COMM_WORLD);

    prev = id - 1;
    next = id + 1;
    // Последний процеес ждет пока что-то не сломается
    ranksize = ranksize - 1;

    // Количество прохождений массива всеми процессами
	int iterations = (tsteps % ranksize == 0) ? (int)(tsteps / ranksize) : (int)(tsteps / ranksize + 1);

    // Ожидающий процесс
    if (id == ranksize) {
        int recovery_info[3];
        backup_process(recovery_info);
        id = recovery_info[0];
        k = recovery_info[1];
        i = recovery_info[2];
        prev = id - 1;
        next = id + 1;

        // Восстановить 2 строки из файла DATA_(id-1)_k_i.txt и DATA_id_k_(i-1).txt
        load_control_point(n, A[i], id-1, k, i);
        load_control_point(n, A[i-1], id, k, i-1);

        printf ("Процесс %d заменил процесс %d\n", ranksize, id);
        goto start_point;
    }

	for (k = 0; k < iterations; k++){

        // На всех, кроме первой итерации нулевому процессу пересылается весь массив
		if (id == 0 && k != 0)
			MPI_Recv(&A[0][0], n * n, MPI_FLOAT, ranksize - 1, 1216, MPI_COMM_WORLD, &status);

        // Каждому процессу нужно пройти все строки массива
        for (i = 0; i <= n - 2; i++){
            // Имитация сбоя
            if (id == break_id && k == break_k && i == break_i) {
                printf("Процесс %d сломался на итерации: %d, на строке: %d\n", id, k, i);
                raise(SIGKILL);
            }
            
            // Точка входа для нового процесса
            start_point:
            
            // На последней итерации может понадобиться меньше процессов
            process_per_iteration = (k + 1 == iterations) ? ((tsteps % ranksize == 0) ? ranksize : tsteps % ranksize) : ranksize;
            // Получить от предыдущего процесса обработанную строку
			if (id != 0 && i <= n - 3 && id < process_per_iteration) {
				err = MPI_Recv(&A[i+1][1], n - 2, MPI_FLOAT, prev, 1215, MPI_COMM_WORLD, &status);
                // В случае сбоя сообщить ждущему процессу
                if (err) {
                    printf ("Процесс %d обнаружил ошибку\n", id);
                    recovery_process(id-1, k, i+1);
                    MPI_Recv(&A[i+1][1], n - 2, MPI_FLOAT, ranksize, 1215, MPI_COMM_WORLD, &status);
                    prev = ranksize;
                }
            }

			if (i >= 1 && id < process_per_iteration){

                // Обработать строку
				for (j = 1; j <= n - 2; j++)
			  		A[i][j] = (A[i-1][j-1] + A[i-1][j] + A[i-1][j+1] + A[i][j-1] + A[i][j] + A[i][j+1] + A[i+1][j-1] + A[i+1][j] + A[i+1][j+1])/9.0f;

                // Сохранить строку в файл DATA_id_k_i.txt
                save_control_point(n, A[i], id, k, i);

                // Отправить обработанную строку следующему процессу
			  	if (id != process_per_iteration - 1) {
    			    err = MPI_Send(&A[i][1], n - 2, MPI_FLOAT, next, 1215, MPI_COMM_WORLD);
                    // В случае сбоя сообщить ждущему процессу
                    if (err) {
                        printf ("Процесс %d обнаружил ошибку\n", id);                       
                        recovery_process(id+1, k, i-1);
                        MPI_Send(&A[i][1], n - 2, MPI_FLOAT, ranksize, 1215, MPI_COMM_WORLD);
                        next = ranksize;
                    }
                }
			}
		}
        // Последний процесс отправляет обновленный массив нулевому процессу (кроме последней итерации)
		if (id == process_per_iteration - 1 && k + 1 != iterations)
			MPI_Send(&A[0][0], n * n, MPI_FLOAT, 0, 1216, MPI_COMM_WORLD);
	}
    
    // Переслать обработанный массив в нулевой процесс
	if (id == 0)
	    MPI_Recv(&A[0][0], n * n, MPI_FLOAT, process_per_iteration - 1, 1217, MPI_COMM_WORLD, &status);
	if (id == process_per_iteration - 1)
		MPI_Send(&A[0][0], n * n, MPI_FLOAT, 0, 1217, MPI_COMM_WORLD);
}

int main(int argc, char** argv) {
    int n = N;
    int tsteps = TSTEPS;

    MPI_Init(&argc, &argv);
    MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);

    float (*A)[n][n]; A = (float(*)[n][n])malloc ((n) * (n) * sizeof(float));;

    init_array (n, *A);

    bench_timer_start();;

    kernel_seidel_2d (tsteps, n, *A);

    bench_timer_stop();;
    bench_timer_print();;

    if (argc > 42 && ! strcmp(argv[0], "")) print_array(n, *A);

    free((void*)A);;

    MPI_Finalize();

    return 0;
}
