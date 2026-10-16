#include "snake.h"
#include "graphics.h"
#include "timer.h"
#include "desktop.h"

#define SNAKE_CELL 16
#define SNAKE_MAX_LENGTH 128
#define SNAKE_START_LENGTH 4
#define SNAKE_START_TICKS 12
#define SNAKE_HEADER_HEIGHT 22

struct snake_segment {
    int x;
    int y;
};

struct snake_segment snake_body[SNAKE_MAX_LENGTH];
int snake_length;
int snake_direction_x;
int snake_direction_y;
int snake_next_direction_x;
int snake_next_direction_y;
int snake_food_x;
int snake_food_y;
int snake_board_width;
int snake_board_height;
int snake_score;
unsigned int snake_last_tick;
unsigned int snake_seed = 0x51A9;
int snake_game_over;
int snake_won;
int snake_paused;

int snake_random(int limit) {
    snake_seed = snake_seed * 1103515245 + 12345;
    return (snake_seed >> 16) % limit;
}

int snake_hits_body(int x, int y, int count) {
    for (int i = 0; i < count; i++) {
        if (snake_body[i].x == x && snake_body[i].y == y) return 1;
    }
    return 0;
}

void snake_place_food(void) {
    int board_size = snake_board_width * snake_board_height;
    if (board_size <= 0) {
        snake_food_x = -1;
        snake_food_y = -1;
        snake_game_over = 1;
        return;
    }

    int start = snake_random(board_size);
    for (int offset = 0; offset < board_size; offset++) {
        int cell = (start + offset) % board_size;
        int x = cell % snake_board_width;
        int y = cell / snake_board_width;
        if (!snake_hits_body(x, y, snake_length)) {
            snake_food_x = x;
            snake_food_y = y;
            return;
        }
    }

    snake_food_x = -1;
    snake_food_y = -1;
    snake_won = 1;
    snake_game_over = 1;
}

void snake_draw_text(int x, int y, char* text, unsigned char r, unsigned char g, unsigned char b) {
    for (int i = 0; text[i]; i++) {
        draw_char(x + i * 9, y, text[i], r, g, b, 1);
    }
}

void snake_draw(void) {
    int board_top = SNAKE_HEADER_HEIGHT;
    fill_screen(0x05, 0x08, 0x05);
    draw_rect(0, 0, screen.width, SNAKE_HEADER_HEIGHT, 0x00, 0x00, 0x00);
    snake_draw_text(4, 1, "SNAKE", 0x00, 0xFF, 0x00);
    char score_label[16] = "SCORE: 0000";
    int score = snake_score;
    for (int i = 10; i >= 7; i--) {
        score_label[i] = '0' + score % 10;
        score /= 10;
    }
    snake_draw_text(screen.width / 2 - 45, 1, score_label, 0xFF, 0xFF, 0x00);
    snake_draw_text(4, 12, "WASD/ARROWS:MOVE  P:PAUSE  CTRL+H:EXIT",
                    0xAA, 0xAA, 0xAA);

    for (int i = 0; i < snake_length; i++) {
        unsigned char r = i == 0 ? 0x80 : 0x20;
        unsigned char g = i == 0 ? 0xFF : 0xCC;
        draw_rect(snake_body[i].x * SNAKE_CELL,
                  board_top + snake_body[i].y * SNAKE_CELL,
                  SNAKE_CELL - 1, SNAKE_CELL - 1, r, g, 0x20);
    }
    if (snake_food_x >= 0 && snake_food_y >= 0) {
        draw_rect(snake_food_x * SNAKE_CELL, board_top + snake_food_y * SNAKE_CELL,
                  SNAKE_CELL - 1, SNAKE_CELL - 1, 0xFF, 0x30, 0x30);
    }

    if (snake_game_over || snake_paused) {
        char* message = snake_won ? "YOU WIN - ENTER TO RESTART" :
                        snake_game_over ? "GAME OVER - ENTER TO RESTART" :
                        "PAUSED - P TO CONTINUE";
        int message_length = 0;
        while (message[message_length]) message_length++;
        int message_x = (screen.width - 9 * message_length) / 2;
        snake_draw_text(message_x, screen.height / 2, message, 0xFF, 0xFF, 0x00);
        if (snake_game_over) {
            snake_draw_text((screen.width - 9 * 27) / 2, screen.height / 2 + 12,
                            "CTRL+H TO RETURN TO DESKTOP", 0xAA, 0xAA, 0xAA);
        }
    }
    screen_swap();
}

void snake_start(void) {
    snake_board_width = screen.width / SNAKE_CELL;
    snake_board_height = (screen.height - SNAKE_HEADER_HEIGHT) / SNAKE_CELL;
    snake_length = SNAKE_START_LENGTH;
    snake_direction_x = 1;
    snake_direction_y = 0;
    snake_next_direction_x = 1;
    snake_next_direction_y = 0;
    snake_game_over = 0;
    snake_won = 0;
    snake_paused = 0;
    snake_score = 0;
    snake_last_tick = timer_get_ticks();
    for (int i = 0; i < snake_length; i++) {
        snake_body[i].x = snake_board_width / 2 - i;
        snake_body[i].y = snake_board_height / 2;
    }
    snake_place_food();
    snake_draw();
}

void snake_key_handler(char c) {
    if (snake_game_over) {
        if (c == '\n') snake_start();
        return;
    }
    if (c == 'p' || c == 'P') {
        snake_paused = !snake_paused;
        snake_draw();
        return;
    }

    int requested_x = snake_next_direction_x;
    int requested_y = snake_next_direction_y;
    if (c == 1 || c == 'w' || c == 'W') {
        requested_x = 0; requested_y = -1;
    } else if (c == 2 || c == 's' || c == 'S') {
        requested_x = 0; requested_y = 1;
    } else if (c == 3 || c == 'a' || c == 'A') {
        requested_x = -1; requested_y = 0;
    } else if (c == 4 || c == 'd' || c == 'D') {
        requested_x = 1; requested_y = 0;
    } else {
        return;
    }

    if ((requested_x == -snake_direction_x && requested_y == -snake_direction_y) ||
        (requested_x == -snake_next_direction_x && requested_y == -snake_next_direction_y)) {
        return;
    }

    snake_next_direction_x = requested_x;
    snake_next_direction_y = requested_y;
}

void snake_update(void) {
    if (snake_game_over || snake_paused || timer_get_ticks() - snake_last_tick < SNAKE_START_TICKS) return;
    snake_last_tick = timer_get_ticks();
    snake_direction_x = snake_next_direction_x;
    snake_direction_y = snake_next_direction_y;

    int next_x = snake_body[0].x + snake_direction_x;
    int next_y = snake_body[0].y + snake_direction_y;
    int growing = next_x == snake_food_x && next_y == snake_food_y;
    int checked_length = growing ? snake_length : snake_length - 1;

    if (next_x < 0 || next_x >= snake_board_width || next_y < 0 ||
        next_y >= snake_board_height || snake_hits_body(next_x, next_y, checked_length)) {
        snake_game_over = 1;
        snake_draw();
        return;
    }

    if (growing && snake_length < SNAKE_MAX_LENGTH) snake_length++;
    for (int i = snake_length - 1; i > 0; i--) snake_body[i] = snake_body[i - 1];
    snake_body[0].x = next_x;
    snake_body[0].y = next_y;
    if (growing) {
        snake_score++;
        snake_place_food();
    }
    snake_draw();
}