#include <ncurses.h>
#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <time.h>
#include <errno.h>

struct coordinates {
    int x;
    int y;
};

const chtype BORDER = '#';
const chtype PLAYER = '@';
const chtype PLAYER_BULLET = '|';
const chtype ENEMY = 'v';
const chtype ENEMY_BULLET = '+';
const chtype NOTHING = ' ';

const int HEIGHT = 30;
const int WIDTH = 60;

int PLAYER_HEIGHT = HEIGHT - 1;
int PLAYER_WIDTH = WIDTH / 2;

int read_keyboard = 0;
int *CLOCK;

const int MAX_ENEMIES = 10;
int current_enemies = 0;

int score = 0;

int TICK_LEN = 250;
int TICK_KEY = 0;

pthread_mutex_t master_mutex;
pthread_mutex_t clock_mutex;

int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

void init() {
    // border
    
    CLOCK = (int *) malloc(sizeof(int)*(WIDTH - 2));

    for (int i = 0; i < HEIGHT / 2; i++) {
        CLOCK[i] = 0;
    }

    for (int i = 0; i < HEIGHT; i++) {
        mvaddch(i, 0, BORDER);
        mvaddch(i, WIDTH, BORDER);
    }
    for (int i = 0; i < WIDTH; i++) {
        mvaddch(0, i, BORDER);
        mvaddch(HEIGHT, i, BORDER);
    }

    mvaddch(PLAYER_HEIGHT, PLAYER_WIDTH, PLAYER);
}

int get_at(int y, int x) {
    return mvinch(y, x) & A_CHARTEXT;
}

void *pew() {
    // fire a bullet
    int x = PLAYER_HEIGHT - 1;
    int y = PLAYER_WIDTH;

    pthread_mutex_lock(&master_mutex);
    mvaddch(x, y, PLAYER_BULLET);
    pthread_mutex_unlock(&master_mutex);

    while (x > 1) {
        pthread_mutex_lock(&master_mutex);

        if (get_at(x-1, y) == (int) ENEMY || get_at(x, y) == (int) NOTHING) {
            // wait till ship moves
            pthread_mutex_unlock(&master_mutex);
            return NULL;
        }

        mvaddch(x, y, NOTHING);
        x -= 1;
        mvaddch(x, y, PLAYER_BULLET);

        pthread_mutex_unlock(&master_mutex);

        msleep(TICK_LEN);
    }
    pthread_mutex_unlock(&master_mutex);

    mvaddch(x, y, NOTHING);

    pthread_mutex_unlock(&master_mutex);
}

void *ship(void *args) {
    struct coordinates *coords = args;

    int x = coords->x;
    int y = coords->y;

    // move ship
    while (x < HEIGHT - 1) {
        pthread_mutex_lock(&master_mutex);

        mvaddch(x, y, NOTHING);
        x += 1;

        if (get_at(x, y) == (int) PLAYER_BULLET) {
            // shine and del bullet
            mvaddch(x, y, NOTHING);
            pthread_mutex_unlock(&master_mutex);
            score += 1;
            current_enemies -= 1;
            return NULL;
        }

        mvaddch(x, y, ENEMY);

        pthread_mutex_unlock(&master_mutex);

        msleep(TICK_LEN);
    }
    pthread_mutex_lock(&master_mutex);

    mvaddch(x, y, NOTHING);
    current_enemies -= 1;
    score -= 5;

    pthread_mutex_unlock(&master_mutex);
}

void generate_enemy() {

    if (current_enemies >= MAX_ENEMIES) {
        return;
    }

    int y = (int) (rand() % (WIDTH - 2)) + 1;
    // upper screen
    int x = (int) ((rand() % (HEIGHT + 2)) / 2) + 1;

    pthread_mutex_lock(&clock_mutex);

    while (CLOCK[y] == 1) {
        CLOCK[y] = 0;
        if (y == (WIDTH - 2)) {
            y = 0;
        }
        y += 1;
    }

    if (y > 2) {
        CLOCK[y-2] = 1;
    }
    if (y > 1) {
        CLOCK[y-1] = 1;
    }
    CLOCK[y] = 1;
    if (y < (WIDTH - 2) - 1) {
        CLOCK[y+1] = 1;
    }
    if (y < (WIDTH / 2) - 2) {
        CLOCK[y+2] = 1;
    }

    if (y == 0) {
        y++;
    }
    if (y >= (WIDTH-2)) {
       y--;
       y--;
       y--;
    }


    struct coordinates *args = malloc(sizeof(struct coordinates));
    args->x = x;
    args->y = y;

    pthread_t ship_thread;
    pthread_create(&ship_thread, NULL, &ship, (void *) args);
    current_enemies += 1;

    pthread_mutex_unlock(&clock_mutex);
}

void *keyboard() {
    while (true) {
        int d;

        d = getch();


        if (d == KEY_LEFT && PLAYER_WIDTH > 1) {
            pthread_mutex_lock(&master_mutex);

            mvaddch(PLAYER_HEIGHT, PLAYER_WIDTH, NOTHING);
            PLAYER_WIDTH -= 1;
            mvaddch(PLAYER_HEIGHT, PLAYER_WIDTH, PLAYER);

            pthread_mutex_unlock(&master_mutex);
        }
        else if (d == KEY_RIGHT && PLAYER_WIDTH < WIDTH - 1) {
            pthread_mutex_lock(&master_mutex);

            mvaddch(PLAYER_HEIGHT, PLAYER_WIDTH, NOTHING);
            PLAYER_WIDTH += 1;
            mvaddch(PLAYER_HEIGHT, PLAYER_WIDTH, PLAYER);

            pthread_mutex_unlock(&master_mutex);
        }
        else if (d == (int) 'z') {
            pthread_mutex_lock(&master_mutex);

            // pew! pew! pew!
            mvaddch(PLAYER_HEIGHT-1, PLAYER_WIDTH, PLAYER_BULLET);

            pthread_t pew_thread;
            pthread_create(&pew_thread, NULL, pew, NULL);

            pthread_mutex_unlock(&master_mutex);
        }
        msleep(TICK_KEY);
   }
}

int main() {
    srand(time(NULL));

    WINDOW *master = stdscr;
    setlocale(LC_ALL, "C");

    // To get character-at-a-time input without echoing (most interactive, screen oriented programs want this), the following sequence should be used:
    initscr(); cbreak(); noecho(); curs_set(0);
    keypad(stdscr, true);

    init();

    pthread_mutex_init(&master_mutex, NULL);
    pthread_mutex_init(&clock_mutex, NULL);

    pthread_t keyboard_thread;
    pthread_create(&keyboard_thread, NULL, keyboard, NULL);

    msleep(30);

    while (true) {
        read_keyboard = 0;

        pthread_mutex_lock(&master_mutex);
        refresh();
        read_keyboard = 1;
        pthread_mutex_unlock(&master_mutex);
        msleep(30);
        generate_enemy();

    }
    return 0;
}
