#ifndef PROJECT2_SCHEDULER_H
#define PROJECT2_SCHEDULER_H

#include <time.h>
#include <mqueue.h>

#define QUEUE_NAME "/mq_query_queue"
#define INITIAL_CAPACITY 10

enum command_t {
    RELATIVE,
    ABSOLUTE,
    PERIODIC,
    DISPLAY,
    CANCEL,
    SHUTDOWN
};

// Zapytanie do serwera
struct query_t{
    enum command_t command;
    int task_id;
    char exec_file_name[256];
    char reply_name[256];
    int years;
    int days;
    int hours;
    int minutes;
    int seconds;
};

// Odpowiedź serwera
struct reply_t{
    char data[256];
    int status;
};

// Zadanie
struct task_t{
    enum command_t command;
    int task_id;
    char exec_file_name[256];
    timer_t timer;
    time_t execution_time;
    int is_active;
    char arguments[256];
};

// Lista zadań
struct task_list_t{
    struct task_t *tasks;
    int size;
    int capacity;
};

int is_server_working();
int scheduler_server();
int scheduler_client(int argc, char **argv);
int scheduler_add_task(struct query_t *query);
void scheduler_display_tasks(struct query_t *query);
int scheduler_cancel_task(int task_id);
void scheduler_shutdown(mqd_t queue_id);

int handle_program_arguments(int argc, char** argv, struct query_t *query);

int init_task_list(struct task_list_t *list);
int expand_task_list(struct task_list_t *list);
void free_task_list(struct task_list_t *list);

#endif
