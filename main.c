#include "scheduler.h"
#include <stdio.h>
#include <unistd.h>

// Względnie: RELATIVE yyyy dd hh mm ss plik
// Bezwzględnie: ABSOLUTE yyyy dd hh mm ss plik
// Cyklicznie: PERIODIC yyyy dd hh mm ss plik
// Lista zadań: DISPLAY
// Anulowanie zadania: CANDEL task_id
// Wyłączenie serwera: SHUTDOWN

int main(int argc, char **argv) {
    // serwer
    if (is_server_working() == 0) {
        printf("Trwa uruchamianie serwera...\n");
        if (fork() == 0) {
            int server = scheduler_server();
            if (server != 0) {
                printf("Nie udało się utworzyć serwera!\n");
                return -1;
            }
        }
        return 0;
    }
    // klient
    int client = scheduler_client(argc, argv);
    if (client == -1) {
        printf("Nie podano żadnej komendy!\n");
        return -2;
    }
    else if (client == -2) {
        printf("Niewłaściwa liczba argumentów dla komendy!\n");
        return -3;
    }
    else if (client == -3) {
        printf("Błędna komenda!\n");
        return -4;
    }
    else if (client !=0) {
        printf("Nie udało się utworzyć klienta!\n");
        return -1;
    }
    return 0;
}
