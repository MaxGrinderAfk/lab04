#include "func.h"

int main() {
    srand(time(NULL));
    
    producers = (pid_t*)malloc(max_producers * sizeof(pid_t));
    consumers = (pid_t*)malloc(max_consumers * sizeof(pid_t));
    
    if (!producers || !consumers) {
        perror("malloc failed");
        exit(1);
    }
    
    init_queue();
    init_semaphores();

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("Система производителей и потребителей запущена\n");
    printf("Управление:\n");
    printf("p - создать производителя\n");
    printf("c - создать потребителя\n");
    printf("P - остановить производителя\n");
    printf("C - остановить потребителя\n");
    printf("s - показать статус\n");
    printf("q - выход\n");
    
    char cmd;
    while (!should_terminate) {
        if (kbhit()) {
            cmd = getchar();
            switch (cmd) {
                case 'p':
                    create_producer();
                    break;
                case 'c':
                    create_consumer();
                    break;
                case 'P':
                    stop_producer();
                    break;
                case 'C':
                    stop_consumer();
                    break;
                case 's':
                    show_status();
                    break;
                case 'q':
                    printf("Завершение работы...\n");
                    should_terminate = true;
                    break;
            }
        }
        
        if (num_producers == 0 && num_consumers == 0 && 
            (queue->added > 0 || queue->extracted > 0)) {
            printf("Предотвращение тупика: создание производителя и потребителя\n");
            create_producer();
            create_consumer();
        }
        
        usleep(100000); 
    }
    
    cleanup();

    printf("Программа завершена\n");
    return 0;
}
