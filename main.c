#include <ncurses.h>
#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>

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

const int MAX_ENEMIES = 10;
int current_enemies = 0;

int score = 0;

pthread_mutex_t master_mutex;
pthread_mutex_t collision_mutex;

void init() {
    // border

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

    while (x > 1) {
        pthread_mutex_lock(&collision_mutex);

        mvaddch(x, y, NOTHING);
        x -= 1;
        mvaddch(x, y, PLAYER_BULLET);

        pthread_mutex_unlock(&collision_mutex);

        sleep(1);
    }
    mvaddch(x, y, NOTHING);
}

void *ship(void *args) {
    struct coordinates *coords = args;

    int x = coords->x;
    int y = coords->y;

    // move ship
    while (x < HEIGHT - 1) {
        pthread_mutex_lock(&collision_mutex);

        mvaddch(x, y, NOTHING);
        x += 1;

        if (get_at(x, y) == (int) PLAYER_BULLET) {
            // shine
            pthread_mutex_unlock(&collision_mutex);
            score += 1;
            current_enemies -= 1;
            return NULL;
        }

        mvaddch(x, y, ENEMY);

        pthread_mutex_unlock(&collision_mutex);

        sleep(1);
    }
    mvaddch(x, y, NOTHING);
}

void generate_enemy() {

    if (current_enemies >= MAX_ENEMIES) {
        return;
    }

    int y = (int) (rand() % (WIDTH - 2));
    // upper screen
    int x = (int) (rand() % (HEIGHT - 2)) / 2;

    mvaddch(x, y, ENEMY);
    struct coordinates args;
    args.x = x;
    args.y = y;

    pthread_t ship_thread;
    pthread_create(&ship_thread, NULL, &ship, (void *) &args);
    current_enemies += 1;
}

void *keyboard() {
    while (true) {
        if (!read_keyboard) {
            sleep(0.02);
            continue;
        }

        int d;
        d = getch();


        if (d == KEY_LEFT && PLAYER_WIDTH > 1) {
            mvaddch(PLAYER_HEIGHT, PLAYER_WIDTH, NOTHING);
            PLAYER_WIDTH -= 1;
            mvaddch(PLAYER_HEIGHT, PLAYER_WIDTH, PLAYER);
        }
        else if (d == KEY_RIGHT && PLAYER_WIDTH < WIDTH - 1) {
            mvaddch(PLAYER_HEIGHT, PLAYER_WIDTH, NOTHING);
            PLAYER_WIDTH += 1;
            mvaddch(PLAYER_HEIGHT, PLAYER_WIDTH, PLAYER);
        }
        else if (d == (int) 'z') {
            // pew! pew! pew!
            mvaddch(PLAYER_HEIGHT-1, PLAYER_WIDTH, PLAYER_BULLET);

            pthread_t pew_thread;
            pthread_create(&pew_thread, NULL, pew, NULL);
        }
        sleep(0.02);
   }
}

int main() {
    srand(time(NULL));

    WINDOW *master = stdscr;
    setlocale(LC_ALL, "C");

    // To get character-at-a-time input without echoing (most interactive, screen oriented programs want this), the following sequence should be used:
    initscr(); cbreak(); noecho();
    keypad(stdscr, true);

    pthread_mutex_init(&collision_mutex, NULL);

    init();

    pthread_mutex_init(&master_mutex, NULL);

    pthread_t keyboard_thread;
    pthread_create(&keyboard_thread, NULL, keyboard, NULL);

    while (true) {
        read_keyboard = 0;

        pthread_mutex_lock(&master_mutex);
        refresh();
        read_keyboard = 1;
        pthread_mutex_unlock(&master_mutex);



        generate_enemy();

    }
    return 0;
}
