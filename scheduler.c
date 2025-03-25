#include "scheduler.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

pthread_mutex_t task_mutex = PTHREAD_MUTEX_INITIALIZER;
struct task_list_t *scheduler_list = NULL;
int ID = 0;

// Sprawdzenie czy serwer działa
int is_server_working() {
    mqd_t queue = mq_open(QUEUE_NAME, O_WRONLY);
    if (queue == -1) {
        return 0;
    }
    mq_close(queue);
    return 1;
}

// Serwer
int scheduler_server() {
    init_logger();
    write_log(MAX, "Uruchomienie harmonogramu.");

    scheduler_list = calloc(1, sizeof(struct task_list_t));
    if (scheduler_list == NULL) {
        write_log(MIN, "Błąd alokacji pamięci dla listy zadań!");
        close_logger();
        return -1;
    }
    if (init_task_list(scheduler_list) != 0) {
        write_log(MIN, "Błąd alokacji pamięci dla listy zadań!");
        free(scheduler_list);
        scheduler_list = NULL;
        close_logger();
        return -1;
    }

    struct mq_attr queue_attr;
    queue_attr.mq_flags = 0;
    queue_attr.mq_maxmsg = INITIAL_CAPACITY;
    queue_attr.mq_msgsize = sizeof(struct query_t);
    queue_attr.mq_curmsgs = 0;

    mqd_t queue_id = mq_open(QUEUE_NAME, O_RDONLY | O_CREAT | O_EXCL, 0666, &queue_attr);
    if (queue_id == -1) {
        write_log(MIN, "Błąd otwierania kolejki!");
        free_task_list(scheduler_list);
        free(scheduler_list);
        scheduler_list = NULL;
        close_logger();
        return -2;
    }

    struct query_t scheduler_query;
    while(1) {
        ssize_t bytes = mq_receive(queue_id, (char *)&scheduler_query, sizeof(struct query_t), NULL);
        if (bytes == -1) {
            write_log(MIN, "Błąd odbioru wiadomosci!");
            continue;
        }

        if (scheduler_query.command == RELATIVE) {
            int result = scheduler_add_task(&scheduler_query);
            if (result >= 0) {
                write_log(STANDARD, "Zadanie %d dodane pomyslnie.", result);
            }
            else {
                write_log(STANDARD, "Nie udało się dodać nowego zadania.");
            }
        }
        else if (scheduler_query.command == ABSOLUTE) {
            int result = scheduler_add_task(&scheduler_query);
            if (result >= 0) {
                write_log(STANDARD, "Zadanie %d dodane pomyslnie.", result);
            }
            else {
                write_log(STANDARD, "Nie udało się dodać nowego zadania.");
            }
        }
        else if (scheduler_query.command == PERIODIC) {
            int result = scheduler_add_task(&scheduler_query);
            if (result >= 0) {
                write_log(STANDARD, "Zadanie %d dodane pomyslnie.", result);
            }
            else {
                write_log(STANDARD, "Nie udało się dodać nowego zadania.");
            }
        }
        else if (scheduler_query.command == DISPLAY) {
            scheduler_display_tasks(&scheduler_query);
        }
        else if (scheduler_query.command == CANCEL) {
            int result = scheduler_cancel_task(scheduler_query.task_id);
            if ( result == 0) {
                write_log(STANDARD, "Nie udało się usunąć zadania o numerze %d", scheduler_query.task_id);
            }
            else {
                write_log(STANDARD, "Usunięto zadania o numerze %d.", scheduler_query.task_id);
            }
        }
        else if (scheduler_query.command == SHUTDOWN) {
            write_log(MAX, "Zamknięcie harmonogramu.");
            scheduler_shutdown(queue_id);
            break;
        }
        else {
            write_log(MIN, "Błędna komenda!");
        }
    }
    return 0;
}

// Klient
int scheduler_client(int argc, char **argv) {
    struct query_t scheduler_query;
    int arguments = handle_program_arguments(argc, argv, &scheduler_query);
    if (arguments != 0) {
        return arguments;
    }

    mqd_t queue_id = mq_open(QUEUE_NAME, O_WRONLY);
    if (queue_id == -1) {
        write_log(MIN, "Błąd otwierania kolejki!");
        return -4;
    }

    mqd_t reply_id;
    if (scheduler_query.command == DISPLAY) {
        sprintf(scheduler_query.reply_name, "/reply_queue_%d", getpid());
        struct mq_attr reply_attr;
        reply_attr.mq_flags = 0;
        reply_attr.mq_maxmsg = INITIAL_CAPACITY;
        reply_attr.mq_msgsize = sizeof(struct reply_t);
        reply_attr.mq_curmsgs = 0;

        reply_id = mq_open(scheduler_query.reply_name, O_RDONLY | O_CREAT | O_EXCL, 0666, &reply_attr);
        if (reply_id == -1) {
            mq_close(queue_id);
            write_log(MIN, "Błąd tworzenia kolejki odpowiedzi!");
            return -5;
        }
    }

    if (mq_send(queue_id, (const char*)&scheduler_query, sizeof(struct query_t), 0) == -1) {
        write_log(MIN, "Błąd wysyłania zapytania!");
        mq_close(queue_id);
        return -6;
    }

    if (scheduler_query.command == DISPLAY) {
        struct reply_t reply;
        while (1) {
            if (mq_receive(reply_id, (char *)&reply, sizeof(struct reply_t), NULL) == -1) {
                write_log(MIN, "Błąd odbierania odpowiedzi z kolejki!");
                break;
            }
            // Sygnał końcowy
            if (strlen(reply.data) == 0) {
                break;
            }
            printf("%s\n", reply.data);
        }
    } else {
        mq_close(queue_id);
        return 0;
    }

    mq_close(queue_id);
    mq_close(reply_id);
    mq_unlink(scheduler_query.reply_name);
    return 0;
}

// Uruchomienie zaplanowanego zadania
void *scheduler_execute_task(void *new_task) {
    struct task_t *task = (struct task_t*) new_task;
    if (task == NULL) {
        return NULL;
    }
    write_log(STANDARD, "Uruchomiono zadanie %d: %s.", task->task_id, task->exec_file_name);
    if (!fork()) {
        execlp(task->exec_file_name, task->arguments, NULL);
    }
    if (task->command == RELATIVE || task->command == ABSOLUTE) {
        scheduler_cancel_task(task->task_id);
    }
    return NULL;
}

// Dodanie zadania
int scheduler_add_task(struct query_t *query) {
    pthread_mutex_lock(&task_mutex);
    struct task_t *new_task = calloc(1, sizeof(struct task_t));
    if (new_task == NULL) {
        write_log(MIN, "Błąd alokacji pamięci dla nowego zadania!");
        pthread_mutex_unlock(&task_mutex);
        return -1;
    }

    strcpy(new_task->exec_file_name, query->exec_file_name);
    new_task->command = query->command;
    new_task->task_id = ID++;
    new_task->is_active = 1;
    time_t cur_time = time(NULL);
    struct tm time_struct;
    time_t exec_time;
    if (new_task->command == ABSOLUTE) {
        memset(&time_struct, 0, sizeof(struct tm));
        time_struct.tm_year = query->years - 1900;
        time_struct.tm_mday = query->days;
        time_struct.tm_hour = query->hours;
        time_struct.tm_min = query->minutes;
        time_struct.tm_sec = query->seconds;
        time_struct.tm_isdst = -1;
        exec_time = mktime(&time_struct);
    } else {
        localtime_r(&cur_time, &time_struct);
        time_struct.tm_year = time_struct.tm_year + query->years;
        time_struct.tm_mday = time_struct.tm_mday + query->days;
        time_struct.tm_hour = time_struct.tm_hour + query->hours;
        time_struct.tm_min = time_struct.tm_min  + query->minutes;
        time_struct.tm_sec = time_struct.tm_sec + query->seconds;
        exec_time = mktime(&time_struct);
    }
    new_task->execution_time = exec_time;

    struct sigevent sev;
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = (void*)scheduler_execute_task;
    sev.sigev_value.sival_ptr = new_task;
    sev.sigev_notify_attributes = NULL;
    if (timer_create(CLOCK_REALTIME, &sev, &new_task->timer) != 0) {
        write_log(MIN, "Błąd timera!");
        free(new_task);
        pthread_mutex_unlock(&task_mutex);
        return -2;
    }

    struct itimerspec timer_spec;
    timer_spec.it_value.tv_sec = new_task->execution_time - cur_time;
    timer_spec.it_value.tv_nsec = 0;
    if (new_task->command == PERIODIC) {
        time_t interval = query->years * 31536000 + query->days * 86400 + query->hours * 3600 + query->minutes * 60 + query->seconds;
        timer_spec.it_interval.tv_sec = interval;
    } else {
        timer_spec.it_interval.tv_sec = 0;
    }
    timer_spec.it_interval.tv_nsec = 0;

    if (timer_settime(new_task->timer, 0, &timer_spec, NULL) != 0) {
        write_log(MIN, "Błąd timera!");
        timer_delete(new_task->timer);
        free(new_task);
        pthread_mutex_unlock(&task_mutex);
        return -3;
    }

    if (scheduler_list->size >= scheduler_list->capacity) {
        if (expand_task_list(scheduler_list) != 0) {
            free(new_task);
            pthread_mutex_unlock(&task_mutex);
            return -4;
        }
    }
    scheduler_list->tasks[scheduler_list->size] = *new_task;
    scheduler_list->size++;
    pthread_mutex_unlock(&task_mutex);
    return new_task->task_id;
}

// Wyświetlenie listy zadań
void scheduler_display_tasks(struct query_t *query) {
    pthread_mutex_lock(&task_mutex);
    mqd_t reply_queue = mq_open(query->reply_name, O_WRONLY);
    if (reply_queue == -1) {
        write_log(MIN, "Błąd otwierania kolejki odpowiedzi!");
        pthread_mutex_unlock(&task_mutex);
        return;
    }

    for (int i = 0; i < scheduler_list->size; i++) {
        struct task_t *task = &scheduler_list->tasks[i];
        char time_str[26];
        struct tm *time_info = localtime(&task->execution_time);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);
        struct reply_t reply;
        snprintf(reply.data, sizeof(reply.data), "ID: %d | Program: %s | Czas: %s", task->task_id, task->exec_file_name, time_str);

        if (mq_send(reply_queue, (const char *)&reply, sizeof(struct reply_t), 0) == -1) {
            write_log(MIN, "Błąd wysyłania odpowiedzi do klienta!");
        } else {
            write_log(STANDARD, "%s", reply.data);
        }
    }

    // Wysłanie pustej wiadomości jako sygnał końca
    struct reply_t end_signal;
    memset(&end_signal, 0, sizeof(struct reply_t));
    mq_send(reply_queue, (const char *)&end_signal, sizeof(struct reply_t), 0);

    mq_close(reply_queue);
    pthread_mutex_unlock(&task_mutex);
}

// Anulowanie zadania
int scheduler_cancel_task(int task_id) {
    pthread_mutex_lock(&task_mutex);
    for (int i = 0; i < scheduler_list->size; i++) {
        if (scheduler_list->tasks[i].task_id == task_id) {
            timer_delete(scheduler_list->tasks[i].timer);
            for (int j = i; j < scheduler_list->size - 1; j++) {
                scheduler_list->tasks[j] = scheduler_list->tasks[j + 1];
            }
            scheduler_list->size--;
            pthread_mutex_unlock(&task_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&task_mutex);
    return 0;
}

// Zakończenie pracy programu
void scheduler_shutdown(mqd_t queue_id) {
    pthread_mutex_lock(&task_mutex);
    for (int i = 0; i < scheduler_list->size; i++) {
        timer_delete(scheduler_list->tasks[i].timer);
    }
    free_task_list(scheduler_list);
    scheduler_list = NULL;
    mq_close(queue_id);
    mq_unlink(QUEUE_NAME);
    pthread_mutex_unlock(&task_mutex);
}

// Obsługa argumenßów programu
int handle_program_arguments(int argc, char** argv, struct query_t *query) {
    if (argc < 2) {
        return -1;
    }
    if (strcmp(argv[1], "RELATIVE") == 0) {
        if (argc < 8) {
            return -2;
        }
        query->command = RELATIVE;
        query->years = atoi(argv[2]);
        query->days = atoi(argv[3]);
        query->hours = atoi(argv[4]);
        query->minutes = atoi(argv[5]);
        query->seconds = atoi(argv[6]);
        strcpy(query->exec_file_name, argv[7]);
    }
    else if (strcmp(argv[1], "ABSOLUTE") == 0) {
        if (argc < 8) {
            return -2;
        }
        query->command = ABSOLUTE;
        query->years = atoi(argv[2]);
        query->days = atoi(argv[3]);
        query->hours = atoi(argv[4]);
        query->minutes = atoi(argv[5]);
        query->seconds = atoi(argv[6]);
        strcpy(query->exec_file_name, argv[7]);
    }
    else if (strcmp(argv[1], "PERIODIC") == 0) {
        if (argc < 8) {
            return -2;
        }
        query->command = PERIODIC;
        query->years = atoi(argv[2]);
        query->days = atoi(argv[3]);
        query->hours = atoi(argv[4]);
        query->minutes = atoi(argv[5]);
        query->seconds = atoi(argv[6]);
        strcpy(query->exec_file_name, argv[7]);
    }
    else if (strcmp(argv[1], "DISPLAY") == 0) {
        query->command = DISPLAY;
    }
    else if (strcmp(argv[1], "CANCEL") == 0) {
        if (argc < 3) {
            return -2;
        }
        query->command = CANCEL;
        query->task_id = atoi(argv[2]);
    }
    else if (strcmp(argv[1], "SHUTDOWN") == 0) {
        query->command = SHUTDOWN;
    }
    else {
        return -3;
    }
    return 0;
}

// Inicjalizacja listy zadań
int init_task_list(struct task_list_t *list) {
    if (list == NULL) {
        return -1;
    }
    list->size = 0;
    list->capacity = INITIAL_CAPACITY;
    list->tasks = (struct task_t *)calloc(list->capacity, sizeof(struct task_t));
    if (list->tasks == NULL) {
        return -2;
    }
    return 0;
}

// Dynamiczne zwiększanie rozmiaru listy
int expand_task_list(struct task_list_t *list) {
    if (list == NULL || list->tasks == NULL) {
        return -1;
    }
    list->capacity *= 2;
    struct task_t *new_tasks = (struct task_t *)realloc(list->tasks, list->capacity * sizeof(struct task_t));
    if (new_tasks == NULL) {
        return -2;
    }
    list->tasks = new_tasks;
    return 0;
}

// Zwalnianie pamięci listy zadań
void free_task_list(struct task_list_t *list) {
    if (list == NULL) {
        return;
    }
    free(list->tasks);
    list->tasks = NULL;
    list->size = 0;
    list->capacity = 0;
}
