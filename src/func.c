#include "func.h"

Queue* queue;                  
int semid;                     
pid_t* producers;              
pid_t* consumers;              
int num_producers = 0;         
int num_consumers = 0;         
int max_producers = 10;        
int max_consumers = 10;        
bool should_terminate = false; 
int queue_shmid;

#define SEM_MUTEX 0            
#define SEM_EMPTY 1            
#define SEM_FULL 2             

#define QUEUE_SIZE 100

void sem_operation(int semid, int sem_num, int op) {
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op = op;
    sb.sem_flg = 0;
    if (semop(semid, &sb, 1) == -1) {
        perror("semop");
        exit(1);
    }
}

unsigned short calculate_hash(Message* msg) {
    unsigned short hash = 0;
    
    hash = (hash << 5) + hash + msg->type;
    hash = (hash << 5) + hash + msg->size;
    
    for (int i = 0; i < msg->size; i++) {
        hash = (hash << 5) + hash + msg->data[i];
    }
    
    return hash;
}

void create_message(Message* msg) {
    if (!msg) return;
    
    msg->type = rand() % 10;
    msg->size = rand() % 256;
    
    for (int i = 0; i < msg->size; i++) {
        msg->data[i] = rand() % 256;
    }
    
    int aligned_size = ((msg->size + 3) / 4) * 4;
    for (int i = msg->size; i < aligned_size; i++) {
        msg->data[i] = 0;
    }
    
    msg->hash = 0;
    msg->hash = calculate_hash(msg);
}

void producer(int id) {
    srand(time(NULL) ^ getpid());

    while (!should_terminate) {
        Message local_msg;
        create_message(&local_msg);

        sem_operation(semid, SEM_EMPTY, -1);

        if (should_terminate) {
            break;
        }

        sem_operation(semid, SEM_MUTEX, -1);

        memcpy(&queue->buffer[queue->tail], &local_msg, sizeof(Message));
        queue->tail = (queue->tail + 1) % QUEUE_SIZE;
        queue->free--;
        queue->added++;

        unsigned long current_added = queue->added;

        sem_operation(semid, SEM_MUTEX, 1);

        sem_operation(semid, SEM_FULL, 1);

        printf("Производитель %d: добавлено сообщение (тип=%d, размер=%d, хеш=%d), всего добавлено: %lu\n",
               id, local_msg.type, local_msg.size, local_msg.hash, current_added);

        usleep(rand() % 500000 + 100000);
    }

    printf("Производитель %d завершился\n", id);
    exit(0);
}

void consumer(int id) {
    srand(time(NULL) ^ getpid());

    while (!should_terminate) {
        sem_operation(semid, SEM_FULL, -1);

        if (should_terminate) {
            break;
        }

        sem_operation(semid, SEM_MUTEX, -1);

        Message local_msg;
        memcpy(&local_msg, &queue->buffer[queue->head], sizeof(Message));
        
        queue->head = (queue->head + 1) % QUEUE_SIZE;
        queue->free++;
        queue->extracted++;

        unsigned long current_extracted = queue->extracted;

        sem_operation(semid, SEM_MUTEX, 1);

        sem_operation(semid, SEM_EMPTY, 1);

        unsigned short original_hash = local_msg.hash;
        local_msg.hash = 0;
        unsigned short calculated_hash = calculate_hash(&local_msg);

        bool hash_valid = (original_hash == calculated_hash);

        printf("Потребитель %d: извлечено сообщение (тип=%d, размер=%d, хеш=%d), проверка хеша: %s, всего извлечено: %lu\n",
               id, local_msg.type, local_msg.size, original_hash, hash_valid ? "OK" : "FAILED", current_extracted);

        usleep(rand() % 500000 + 200000);
    }

    printf("Потребитель %d завершился\n", id);
    exit(0);
}

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\nПолучен сигнал завершения.\n");
        should_terminate = true;
        
        for (int i = 0; i < num_producers + num_consumers; i++) {
            sem_operation(semid, SEM_EMPTY, 1);
            sem_operation(semid, SEM_FULL, 1);
        }
    }
}

int kbhit() {
    struct termios oldt, newt;
    int ch;
    int oldf;
    
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    
    ch = getchar();
    
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    
    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    
    return 0;
}

void init_queue() {
    int shmid = shmget(IPC_PRIVATE, sizeof(Queue), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }

    queue = (Queue*)shmat(shmid, NULL, 0);
    if (queue == (void*)-1) {
        perror("shmat failed");
        exit(1);
    }

    queue->head = 0;
    queue->tail = 0;
    queue->free = QUEUE_SIZE;
    queue->added = 0;
    queue->extracted = 0;

    queue_shmid = shmid;
}

void init_semaphores() {
    semid = semget(IPC_PRIVATE, 3, 0666 | IPC_CREAT);
    if (semid == -1) {
        perror("semget");
        exit(1);
    }
    
    if (semctl(semid, SEM_MUTEX, SETVAL, 1) == -1) {
        perror("semctl SEM_MUTEX");
        exit(1);
    }
    
    if (semctl(semid, SEM_EMPTY, SETVAL, QUEUE_SIZE) == -1) {
        perror("semctl SEM_EMPTY");
        exit(1);
    }
    
    if (semctl(semid, SEM_FULL, SETVAL, 0) == -1) {
        perror("semctl SEM_FULL");
        exit(1);
    }
}

void create_producer() {
    if (num_producers >= max_producers) {
        printf("Достигнуто максимальное количество производителей\n");
        return;
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    } else if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        producer(num_producers);
        exit(0);
    } else {
        producers[num_producers++] = pid;
        printf("Создан производитель #%d (PID: %d)\n", num_producers, pid);
    }
}

void create_consumer() {
    if (num_consumers >= max_consumers) {
        printf("Достигнуто максимальное количество потребителей\n");
        return;
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    } else if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        consumer(num_consumers);
        exit(0);
    } else {
        consumers[num_consumers++] = pid;
        printf("Создан потребитель #%d (PID: %d)\n", num_consumers, pid);
    }
}

void show_status() {
    sem_operation(semid, SEM_MUTEX, -1);
    
    printf("\n=== Состояние очереди ===\n");
    printf("Размер очереди: %d\n", QUEUE_SIZE);
    printf("Занято: %d\n", QUEUE_SIZE - queue->free);
    printf("Свободно: %d\n", queue->free);
    printf("Добавлено сообщений: %lu\n", queue->added);
    printf("Извлечено сообщений: %lu\n", queue->extracted);
    printf("Количество производителей: %d\n", num_producers);
    printf("Количество потребителей: %d\n", num_consumers);
    printf("==========================\n");
    
    sem_operation(semid, SEM_MUTEX, 1);
}

void stop_producer() {
    if (num_producers > 0) {
        pid_t pid = producers[--num_producers];
        kill(pid, SIGTERM);
        printf("Остановлен производитель (PID: %d)\n", pid);
    } else {
        printf("Нет работающих производителей\n");
    }
}

void stop_consumer() {
    if (num_consumers > 0) {
        pid_t pid = consumers[--num_consumers];
        kill(pid, SIGTERM);
        printf("Остановлен потребитель (PID: %d)\n", pid);
    } else {
        printf("Нет работающих потребителей\n");
    }
}
    
void cleanup() {
    for (int i = 0; i < num_producers; i++) {
        kill(producers[i], SIGTERM);
    }
    for (int i = 0; i < num_consumers; i++) {
        kill(consumers[i], SIGTERM);
    }

    while (wait(NULL) > 0);

    if (semctl(semid, 0, IPC_RMID) == -1) {
        perror("semctl IPC_RMID");
    }

    if (shmdt(queue) == -1) {
        perror("shmdt");
    }

    if (shmctl(queue_shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
    }

    free(producers);
    free(consumers);
}
