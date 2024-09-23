#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <string.h>

#define MAX_PLAYERS 10
#define SHM_SIZE sizeof(TournamentData)

enum sign {
    ROCK,
    PAPER,
    SCISSORS
};

typedef struct {
    int scores[MAX_PLAYERS];
    int wins[MAX_PLAYERS];
    int draws[MAX_PLAYERS];
    int played[MAX_PLAYERS][MAX_PLAYERS];
    sem_t sem;
} TournamentData;

TournamentData *data;

const char* const sign_str[] = {
        [ROCK] = "rock",
        [PAPER] = "paper",
        [SCISSORS] = "scissors",
};

void cleanup() {
    sem_destroy(&data->sem);
    munmap(data, SHM_SIZE);
    shm_unlink("/tournament_shm");
}

void handle_signal(int sig) {
    cleanup();
    exit(0);
}

int battle(int a, int b) {
    if (a == b)
        return 1;

    if (a == ROCK) {
        if (b == PAPER)
            return 2;
        return 0;
    } else if (a == PAPER) {
        if (b == SCISSORS)
            return 2;
        return 0;
    } else if (a == SCISSORS) {
        if (b == ROCK)
            return 2;
        return 0;
    }

    return 0;
}

int get_random_choice() {
    return rand() % 3;
}

void play_game(int player_id, TournamentData *data) {
    for (int opponent_id = 0; opponent_id < MAX_PLAYERS; opponent_id++) {
        if (opponent_id != player_id && data->played[player_id][opponent_id] == 0 && data->played[opponent_id][player_id] == 0) {
            sleep(1);
            int choice1 = get_random_choice();
            int choice2 = get_random_choice();

            int result = battle(choice1, choice2);

            sem_wait(&data->sem);

            data->played[player_id][opponent_id] = 1;
            data->played[opponent_id][player_id] = 1;

            if (result == 1) {
                printf("%d VS %d – draw\n", player_id, opponent_id);
                data->draws[player_id]++;
                data->draws[opponent_id]++;
                data->scores[player_id]++;
                data->scores[opponent_id]++;
            } else if (result == 2) {
                printf("%d VS %d – %d wins\n", player_id, opponent_id, opponent_id);
                data->wins[opponent_id]++;
                data->scores[opponent_id] += 2;
            } else {
                printf("%d VS %d – %d wins\n", player_id, opponent_id, player_id);
                data->wins[player_id]++;
                data->scores[player_id] += 2;
            }

            sem_post(&data->sem);
        }
    }
}

int main() {
    signal(SIGINT, handle_signal);
    srand(time(NULL));

    int shm_fd = shm_open("/tournament_shm", O_CREAT | O_RDWR, 0666);

    if (shm_fd < 0) {
        perror("shm_open failed");
        exit(EXIT_FAILURE);
    }
    sem_unlink("/tournament_sem");
    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
        perror("ftruncate failed");
        exit(EXIT_FAILURE);
    }

    data = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    if (data == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }

    memset(data, 0, SHM_SIZE);

    sem_init(&data->sem, 1, 1);

    pid_t pids[MAX_PLAYERS];

    for (int i = 0; i < MAX_PLAYERS; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            play_game(i, data);
            exit(0);
        }
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        wait(NULL);
    }

    printf("Results of the Tournament:\n");
    for (int i = 0; i < MAX_PLAYERS; i++) {
        printf("Player %d: Score = %d, Wins = %d, Draws = %d\n",
               i, data->scores[i], data->wins[i], data->draws[i]);
    }

    cleanup();

    return 0;
}
