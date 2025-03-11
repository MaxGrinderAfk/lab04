#include "libs.h"

typedef struct {
    unsigned char type;        
    unsigned short hash;       
    unsigned char size;        
    unsigned char data[256];   
} Message;

typedef struct {
    Message buffer[100];
    int head;
    int tail;
    int free;
    unsigned long added;
    unsigned long extracted;
} Queue;

extern Queue* queue;                  
extern int semid;                     
extern pid_t* producers;              
extern pid_t* consumers;              
extern int num_producers;         
extern int num_consumers;         
extern int max_producers;        
extern int max_consumers;        
extern bool should_terminate; 
extern int queue_shmid;

#define SEM_MUTEX 0            
#define SEM_EMPTY 1            
#define SEM_FULL 2             

#define QUEUE_SIZE 100

void sem_operation(int semid, int sem_num, int op);

unsigned short calculate_hash(Message* msg);

void create_message(Message* msg);

void producer(int id);

void consumer(int id);

void signal_handler(int sig);

int kbhit();

void init_queue();

void init_semaphores();

void create_producer();

void create_consumer();

void show_status();

void stop_producer();

void stop_consumer();
    
void cleanup();

