#include <stdio.h>
#include <pthread.h>
#include <string.h>

#define MAX_STRING_LENGTH 100

char sharedString[MAX_STRING_LENGTH] = "BBBBB";
pthread_mutex_t mutex;

void* writeThread(void* arg) {
    for (int i = 1; i <= 5; ++i) {
        // Захватываем мьютекс перед записью
        pthread_mutex_lock(&mutex);

        // Записываем данные в общую строку
        snprintf(sharedString, MAX_STRING_LENGTH, "Write Thread: Iteration %d", i);

        // Освобождаем мьютекс после записи
        pthread_mutex_unlock(&mutex);

        // Приостанавливаем выполнение на случайный промежуток времени
        usleep(100000);
    }

    return NULL;
}

void* readThread(void* arg) {
    for (int i = 1; i <= 5; ++i) {
        // Захватываем мьютекс перед чтением
        pthread_mutex_lock(&mutex);

        // Читаем данные из общей строки
        printf("Read Thread: %s\n", sharedString);

        // Освобождаем мьютекс после чтения
        pthread_mutex_unlock(&mutex);

        // Приостанавливаем выполнение на случайный промежуток времени
        usleep(150000);
    }

    return NULL;
}

int main() {
    // Инициализация мьютекса
    pthread_mutex_init(&mutex, NULL);

    // Создание потоков
    pthread_t writer, reader;
    pthread_create(&writer, NULL, writeThread, NULL);
    pthread_create(&reader, NULL, readThread, NULL);

    // Ожидание завершения потоков
    pthread_join(writer, NULL);
    pthread_join(reader, NULL);

    // Уничтожение мьютекса
    pthread_mutex_destroy(&mutex);

    return 0;
}

