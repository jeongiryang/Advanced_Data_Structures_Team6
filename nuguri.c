// Windows 보안 경고 무시
// 매크로 표준화 변경
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 헤더 및 OS 판별 매크로 수정
#ifndef _WIN32
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#else
#include <windows.h>
#include <conio.h>
#endif

// 맵 및 게임 요소 정의
#define MAX_ENEMIES 15
#define MAX_COINS 30

// 구조체 정의
typedef struct {
    int x, y;
    int dir;
} Enemy;

typedef struct {
    int x, y;
    int collected;
} Coin;

// 전역 변수
char **maps = NULL;
int curr_stage = 0;
int max_height = 0;
int max_width = 0;
int player_x, player_y;
int score = 0;
int collision_count = 0;

// 플레이어 상태
int is_jumping = 0;
int velocity_y = 0;
int on_ladder = 0;

// 게임 객체
Enemy enemies[MAX_ENEMIES];
int enemy_count = 0;
Coin coins[MAX_COINS];
int coin_count = 0;

// 터미널 설정
// 매크로 수정
#ifndef _WIN32
struct termios orig_termios;
#endif

// 함수 선언
void opening(void);
int ending(int result);
void reset_game(void);

void check_is_null(void *);
void disable_raw_mode(void);
void enable_raw_mode(void);
void load_maps(void);
void init_stage(void);
void draw_game(void);
void update_game(char input);
void move_player(char input);
void move_enemies(void);
void check_collisions(void);
int kbhit(void);
void load_maps3(FILE *);
void init_stage(void) ;
void free_maps(void);
void play_jump_sound(void);
void play_nextstage_sound(void);
void play_death_sound(void);

// Windows 콘솔 UTF-8 설정 함수 (원래 여기 있었으나 불필요해서 제거)
#ifdef _WIN32
void enable_ansi(void) { // 윈도우 10에서 ANSI 이스케이프 시퀀스를 사용하기 위한 함수
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hConsole, &dwMode); // dwMode : 0 -> 3
    // 앞전 과제에서 termios.h 헤더에 정의된 터미널 구조체를 사용했던 것처럼 (struct termios oldt, newt;)
    // 여기서는 windows.h 헤더에 정의된 unsigned long타입의 dwMode 변수와 HANDLE 포인터의 hConsole 변수를 선언, 그리고 초기화를 하며
    // unsigned long, 즉 정수형 변수 dwMode가 '플래그'가 되어 cmd의 각종 설정을 제어하게 된다.
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING; // dwMode : 3 -> 7
    // 가상화 터미널 처리 플래그를 추가한다.(비트 연산으로) 따라서 기존의 cmd 설정에 가상화 터미널 처리 기능이 활성화되어 윈도우에서도 ANSI 이스케이프 시퀀스를 사용할 수 있다.
    // * ENABLE_VIRTUAL_TERMINAL_PROCESSING 플래그 값 = 4
    SetConsoleMode(hConsole, dwMode);
}
#endif

// 화면 지우기 방식 개선 (화면 전체 지우기 제거 후 커서 이동 -> 깜빡임/윈도우 버벅거림 최소화)
void clrscr(void) {
    printf("\x1b[2J\x1b[H");
}

void gotofirst(void){
    printf("\x1b[H");
}
int main(void) {
    srand((unsigned)time(NULL));
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);   // Windows 콘솔 UTF-8 출력 설정 (main 상단에서 이 한 줄만으로 충분)
    enable_ansi();
    #endif

    enable_raw_mode();

    opening();

    // 초기 잔상 제거
    clrscr();

    FILE* file = fopen("map.txt", "rb");
    if (!file) {
        printf("map.txt error");
        exit(1);
    }

    while (1) {
        reset_game();
        load_maps3(file);
        if(maps == NULL){
            gotofirst();
            printf("stage loading error");
            if(file) fclose(file);
            free_maps();
            exit(1);
        }
        init_stage();
        char c = '\0';
        int game_over = 0;
        int restart=0;
        
        // 스테이지 전환 잔상 제거
        clrscr();
        
        gotofirst();
        printf("%d", curr_stage);

        while (!game_over && curr_stage != -1) {
            if (kbhit()) {
    #ifndef _WIN32
                c = getchar();
    #else
                c = getch();
    #endif
                if (c == 'q') {
                    restart=0;
                    game_over = 1;
                    continue;
                }
        #ifndef _WIN32
                if (c == '\x1b') {
                    getchar();
                    switch (getchar()) {
                        case 'A': c = 'w'; break;
                        case 'B': c = 's'; break;
                        case 'C': c = 'd'; break;
                        case 'D': c = 'a'; break;
                        default: break;
                    }
                }
        #else
                if (c == -32 || c == 224) {
                    c = getch();
                    switch (c) {
                        case 72: c = 'w'; break;
                        case 80: c = 's'; break;
                        case 77: c = 'd'; break;
                        case 75: c = 'a'; break;
                        default: break;
                    }
                }
        #endif
            } else {
                c = '\0';
            }

            update_game(c);
            draw_game();

            // 윈도우 버벅거림 해결: 20ms (약 50 FPS)로 설정하여 점프를 더 빠르고 부드럽게 만듦
#ifndef _WIN32
            usleep(90000);
#else
            Sleep(90); // 기존 90ms -> 20ms (게임 속도 및 반응성 향상)
#endif

            if (collision_count >= 3) {
                restart=ending(0);
                break;
            }

            if (maps[player_y][player_x] == 'E') {
                play_nextstage_sound();
                curr_stage++;
                load_maps3(file);
                if (curr_stage != -1) {
                    score += 100;
                    init_stage();
                    // 다음 스테이지 잔상 제거
                    clrscr();
                } else {
                    restart = ending(1);
                    break;
                }
            }
        }

        if (restart == 1){
            rewind(file);
            continue;
        }
        else{
            break;
        }
    }
    if(file) fclose(file);
    free_maps();
    disable_raw_mode();
    return 0;
}

void disable_raw_mode(void) {
    // Windows 커서 복구 및 매크로 수정
    printf("\x1b[?25h");
    fflush(stdout);
    #ifndef _WIN32
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    #endif
}

void enable_raw_mode(void) {
    // Windows 콘솔 초기화 및 커서 숨김
    printf("\x1b[?25l");
    fflush(stdout);


#ifndef _WIN32
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
#endif
}

void load_maps3(FILE *file) {
    if (!file) {
        perror("map.txt 파일을 열 수 없습니다.");
        return;
    }
    free_maps();
    long long int data_count = ftell(file);
    int x = 0, y = 0, max_x = 0, max_y = 0;
    char ch;
    while (1) {
        ch = fgetc(file);
        if (ch == EOF) {
            if (y > 0) {
                if (max_y < y) max_y = y;
            }
            else {
                curr_stage = -1;
                max_width = 0;
                max_height = 0;
                return;
            }
            break;
        }
        else if (ch == '\n') {
            if (y == 0 && x == 0) continue;
            if (x > 0) {
                y++;
                if (max_x < x) max_x = x;
                x = 0;
            }
            else {
                if (max_y < y) max_y = y;
                break;
            }
        }
        else if (ch != '\r') {
            x++;
        }
    }
    if (y > 0) {
        max_width = max_x;
        max_height = max_y;
    }
    data_count = ftell(file) - data_count;
    fseek(file, -1 * data_count, SEEK_CUR);
    maps = (char**)malloc(sizeof(char*) * max_height);
    if (maps == NULL) return;

    gotofirst();
    for (int ty = 0; ty < max_y; ty++) {
        maps[ty] = (char*)malloc(sizeof(char) * (max_width + 3));
        if (maps[ty] == NULL) {
            free_maps();
            return;
        }
        while (fgets(maps[ty], max_width + 3, file) != NULL) {
            char temp_c = maps[ty][0];
            if (!(temp_c == '\n' || temp_c == '\r')) break;
        }
    }
}


void free_maps(void){
    if(maps != NULL){
        for(int y = 0; y < max_height; y++){
            if(maps[y] != NULL) free(maps[y]);
            else break;
        }
        free(maps);
        maps = NULL;
    }
}

void init_stage(void) {
    enemy_count = 0;
    coin_count = 0;
    is_jumping = 0;
    velocity_y = 0;
    for (int y = 0; y < max_height; y++) {
        for (int x = 0; x < max_width; x++) {
            char cell = maps[y][x];
            if (cell == 'S') {
                player_x = x;
                player_y = y;
            } else if (cell == 'X' && enemy_count < MAX_ENEMIES) {
                enemies[enemy_count] = (Enemy){x, y, (rand() % 2) * 2 - 1};
                enemy_count++;
            } else if (cell == 'C' && coin_count < MAX_COINS) {
                coins[coin_count++] = (Coin){x, y, 0};
            }
        }
    }
}

// 윈도우 버벅거림 해결: 화면 그리기 (기존 방식 유지하되 커서 이동으로 깜빡임 방지)
void draw_game(void) {
    printf("\x1b[H"); // 커서를 맨 위로 이동 (덮어쓰기)
    printf("Stage: %d | Score: %d | HEART: %d\n", curr_stage + 1, score, 3 - collision_count);
    printf("조작: ← → (이동), ↑ ↓ (사다리), Space (점프), q (종료)\n");

    int isCoin, isEnemy;

    for (int y = 0; y < max_height; y++) {
        for (int x = 0; x < max_width; x++) {
            isCoin = isEnemy = 0;
            if (x == player_x && y == player_y) {
                printf("%c", 'P');
                continue;
            }
            for (int i = 0; i < coin_count; i++) {
                if (!coins[i].collected && y == coins[i].y && x == coins[i].x) {
                    isCoin = 1;
                    break;
                }
            }
            if (isCoin == 1) {
                printf("%c", 'C');
                continue;
            }
            for (int i = 0; i < enemy_count; i++) {
                if (y == enemies[i].y && x == enemies[i].x) {
                    isEnemy = 1;
                    break;
                }
            }
            if (isEnemy) {
                printf("%c", 'X');
                continue;
            }
            if (maps[y][x] == 'S' || maps[y][x] == 'X' || maps[y][x] == 'C') {
                printf("%c", ' ');
            }
            else {
                printf("%c", maps[y][x]);
            }
        }
        printf("\n");
    }
}

void update_game(char input) {
    move_player(input);
    move_enemies();
    check_collisions();
}

void move_player(char input) {
    // 플레이어 이동 로직 (기존 유지)
    int next_x = player_x, next_y = player_y;
    char floor_tile = (player_y + 1 < max_height) ? maps[player_y + 1][player_x] : '#';
    char current_tile = maps[player_y][player_x];

    on_ladder = (current_tile == 'H');

    switch (input) {

        case 'a': next_x--; break;
        case 'd': next_x++; break;
        case 'w': if (on_ladder) next_y--; break;
        case 's': if (player_y + 1 < max_height) next_y++; break;
        case ' ':
            if (!is_jumping && (floor_tile == '#' || on_ladder || floor_tile == 'H')) {
                is_jumping = 1;
                velocity_y = -3;
                play_jump_sound();
            }
            break;
    }

    if (next_x >= 0 && next_x < max_width) {
        if (maps[player_y][next_x] != '#') {
            player_x = next_x;
            floor_tile = (player_y + 1 < max_height) ? maps[player_y + 1][player_x] : '#';
        }
    }

    if (input == 'w' || input == 's') {
        if(on_ladder){
            if (maps[next_y][player_x] != '#') {
                if((next_y >= 0 && next_y < max_height)){
                    player_y = next_y;
                }
                
            }
            else if(input == 'w'){
                next_y = player_y + (next_y - player_y)*2;
                if(next_y >= 0 && next_y < max_height){
                    player_y = next_y;
                }
            }
            is_jumping = 0;
            velocity_y = 0;
        }
        else{
            if (input == 's' && floor_tile == 'H'){
                player_y = next_y;
            }
        }
        floor_tile = (player_y + 1 < max_height) ? maps[player_y + 1][player_x] : '#';
    }
    else {
        if (is_jumping) {
            next_y = player_y + ((velocity_y >= 3) ? 1 : -1);
            if (next_y < 0) next_y = 0;
            velocity_y++;
            if (velocity_y >= 1 && velocity_y <= 3) return;
            if (next_y < max_height) {
                if (maps[next_y][player_x] == '#') {
                    if (velocity_y <= 0) {
                        if (!on_ladder) velocity_y = 0;
                        /*else if (velocity_y <= -2) {
                            if(next_y - 1 >= 0 && maps[next_y - 1][player_x] != '#'){
                                player_y = next_y - 1;
                                is_jumping = 0;
                            }
                        }//점프로 사다리 올라가기 대신 화살표키로 변경. 위아래 가능*/
                    }
                    else if (velocity_y > 0) is_jumping = 0;
                }
                else if(maps[next_y][player_x] == 'H'){
                    is_jumping = 0;
                }else{
                    player_y = next_y;
                }
            }
            if ((player_y + 1 < max_height) && maps[player_y + 1][player_x] == '#') {
                is_jumping = 0;
                velocity_y = 0;
            }
        }
        else {
            if (floor_tile != '#' && floor_tile != 'H' && !on_ladder) {
                if (player_y + 1 < max_height) player_y++;
                else init_stage();
            }
        }
    }
    if (player_y >= max_height) init_stage();
}

void move_enemies(void) {
    for (int i = 0; i < enemy_count; i++) {
        int next_x = enemies[i].x + enemies[i].dir;
        if (next_x < 0 || next_x >= max_width || maps[enemies[i].y][next_x] == '#' || (enemies[i].y + 1 < max_height && maps[enemies[i].y + 1][next_x] == ' ')) {
            enemies[i].dir *= -1;
        } else {
            enemies[i].x = next_x;
        }
    }
}

void check_collisions(void) {
    for (int i = 0; i < enemy_count; i++) {
        if (player_x == enemies[i].x && player_y == enemies[i].y) {
            play_death_sound();
            collision_count++;
            score = (score > 50) ? score - 50 : 0;
            init_stage();
            return;
        }
    }
    for (int i = 0; i < coin_count; i++) {
        if (!coins[i].collected && player_x == coins[i].x && player_y == coins[i].y) {
            coins[i].collected = 1;
            score += 20;
        }
    }
}

#ifdef _WIN32
int kbhit(void) { return _kbhit(); }
#else
int kbhit(void) {
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
    if(ch != EOF) { ungetc(ch, stdin); return 1; }
    return 0;
}
#endif

void opening(void) {
    // 화면 잔상 제거
    clrscr();
    printf("\n\n");
    printf("    ##########################################\n");
    printf("    #                                        #\n");
    printf("    #         N  U  G  U  R  I               #\n");
    printf("    #             G  A  M  E                 #\n");
    printf("    #                                        #\n");
    printf("    #       - Press Any Key to Start -       #\n");
    printf("    #                                        #\n");
    printf("    ##########################################\n");
    printf("\n");
#ifndef _WIN32
    getchar();
#else
    getch();
#endif
}

int ending(int result) {
    // 화면 잔상 제거
    clrscr();
    printf("\n\n");
    printf("    ##########################################\n");
    printf("    #                                        #\n");
    if (result == 1) {
        printf("    #    C O N G R A T U L A T I O N S       #\n");
        printf("    #         ALL STAGES CLEARED!            #\n");
    } else {
        printf("    #           G A M E   O V E R            #\n");
        printf("    #           Try Again ...                #\n");
    }
    printf("    #                                        #\n");
    printf("    #        FINAL SCORE: %-5d              #\n", score);
    printf("    #                                        #\n");
    printf("    ##########################################\n");
    printf("\n");
    printf("     Press 'r': Restart / 'q': Quit \n");

    while(1) {
    #ifndef _WIN32
        char c = getchar();
    #else
        char c = getch();
    #endif
        if (c == 'r' || c == 'R') return 1;
        else if (c == 'q' || c == 'Q') return 0;
    }
}

void reset_game(void) {
    curr_stage = 0;
    score = 0;
    collision_count = 0;
}

// 점프 효과음 (윈도우 버벅거림 해결: 재생 시간 10ms로 더 단축)
void play_jump_sound(void) {
#ifdef _WIN32
    Beep(800, 10); // 20ms -> 10ms
#else
    printf("\7");
    fflush(stdout);
#endif
}

void play_nextstage_sound(void) {
#ifdef _WIN32
    Beep(440, 50);
    Beep(660, 150);
#else
    printf("\7");
    fflush(stdout);
#endif
}

void play_death_sound(void) {
#ifdef _WIN32
    Beep(220, 100);
    Beep(220, 100);
    Beep(220, 100);
#else
    printf("\7");
    fflush(stdout);
#endif
}
