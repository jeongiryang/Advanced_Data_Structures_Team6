// Windows 보안 경고 무시
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS // scanf 보안경고 무시
#define _CRT_NONSTDC_NO_DEPRECATE // VS환경에서 해더파일안의 내장된 함수 이름들 중복 경고 무시
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#else
#include <windows.h>
#include <conio.h>
#endif

// 맵 및 게임 요소 정의 (수정된 부분)
/*#define MAP_WIDTH 40  // 맵 너비를 40으로 변경
#define MAP_HEIGHT 20
#define MAX_STAGES 2*/
#define MAX_ENEMIES 15 // 최대 적 개수 증가
#define MAX_COINS 30   // 최대 코인 개수 증가

// 구조체 정의
typedef struct {
    int x, y;
    int dir; // 1: right, -1: left
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
int collision_count = 0; //충돌 횟수


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
#ifndef __WIN32
struct termios orig_termios;
#endif
// 함수 선언
void opening(void); // 추가: 오프닝
int ending(int result); // 추가(나가기-다시하기 선택지 구현을 위해 수정)
void reset_game(void); // 추가: 게임 재시작 시 전역변수 초기화

void check_is_null(void *); //추가
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
void load_maps2(void);
void init_stage(void) ;
void free_maps(void);
void play_jump_sound(void);

#ifdef _WIN32
void init_console_utf8(void) {
    static int first = 1;
    if (first) {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        //콘솔을 UTF-8 포맷으로 출력하게 한다. 이 과정이 없으면, 한글과 특수문자 등등 ASCII 밖 and UNICODE 안 범위의 글자들이 깨지게 된다.
        first = 0;
    }
}
void enable_ansi(void) { // 윈도우에서 ANSI 이스케이프 시퀀스를 사용하기 위한 함수
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE); // 표준 입력 핸들을 가져오기
    DWORD dwMode = 0;
    GetConsoleMode(hConsole, &dwMode); // dwMode : 0 -> 3
    // 앞전 과제에서 termios.h 헤더에 정의된 터미널 구조체를 사용했던 것처럼 (struct termios oldt, newt;)
    // 여기서는 windows.h 헤더에 정의된 unsigned long타입의 dwMode 변수와 HANDLE 포인터의 hConsole 변수를 선언, 그리고 초기화를 하며
    // unsigned long, 즉 정수형 변수 dwMode가 '플래그'가 되어 cmd의 각종 설정을 제어하게 된다.
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING; // dwMode : 3 -> 7
    // 가상화 터미널 처리 플래그를 추가한다.(비트 연산으로) 따라서 기존의 cmd 설정에 가상화 터미널 처리 기능이 활성화되어 윈도우에서도 ANSI 이스케이프 시퀀스를 사용할 수 있다.
    // * ENABLE_VIRTUAL_TERMINAL_PROCESSING 플래그 값 = 4
    SetConsoleMode(hConsole, dwMode);
    // 변경된 설정을 적용한다.
}
#endif

// 변경: 화면 지우는 기능을 단순 printf ANSI 이스케이프 시퀀스에서 함수로 변경. 디아블로, makepic에 있던 그것과 동일
void clrscr(void) {
#ifdef _WIN32
    system("cls");
    // system(<명령어>); 함수 -- 윈도우의 cmd에서 <명령어>를 실행했을 때의 결과를 반영한다.
    // 즉, system("cls");는 cmd에서 cls 명령어 -- "화면 전체 지우기"를 실행한다.
#else
    printf("\x1b[2J\x1b[H");
    // \x1b : ESCAPE (27을 Hexadecimal로 변환해 1b)
    // 2J : 2 Page 만큼 화면 지우기
    //
    // ANSI 이스케이프 시퀀스 명령어 목록:
    // <숫자><알파벳> 형태이며, <숫자>만큼 <알파벳>명령을 실행한다.
    // 설명 맨 끝의 괄호는 <숫자> 의 기본값을 나타낸다
    //
    // A : 커서를 위로 (1)
    // B : 커서를 아래로 (1)
    // C : 커서를 왼쪽으로 (1)
    // D : 커서를 오른쪽으로 (1)
    // E : 다음 줄 (1)
    // F : 이전 줄 (1)
    // G : 현재 줄 내에서 커서를 <숫자>번째 위치로 (0)
    // <열>;<행>H : 주어진 열, 행 위치로 커서이동 (0, 0)
    // J : 화면 지우기 (1)
    // K : 현재 줄 지우기 (1)
#endif
}

int main(void) {
    srand((unsigned)time(NULL));

#ifdef _WIN32
    init_console_utf8();
    // 커서 숨기기
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
#else
    printf("\x1b[?25l"); // 커서 숨기기
    fflush(stdout);
#endif
    enable_raw_mode();


    opening();  // 추가

    while (1) { // (게임 무한 루프-왜냐하면 선택지 때문에 )
        reset_game(); // 점수랑 스테이지 초기화
        load_maps2();
        if(maps == NULL){
            clrscr();
            printf("stage loading error");
            free_maps();
            exit(1);
        }
        init_stage();
        char c = '\0';
        int game_over = 0;
        int restart=0; // 재시작 여부 변수 (1: 재시작, 0: 종료)
        clrscr();
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
                    getchar(); // '['
                    switch (getchar()) {
                        case 'A': c = 'w'; break; // Up
                        case 'B': c = 's'; break; // Down
                        case 'C': c = 'd'; break; // Right
                        case 'D': c = 'a'; break; // Left
                        default: break;
                    }
                }
        #else
                if (c == -32 || c == 224) {  // Windows 방향키 시작 코드
                    c = getch();  // 실제 방향키 코드 읽기
                    switch (c) {
                        case 72: c = 'w'; break; // Up
                        case 80: c = 's'; break; // Down
                        case 77: c = 'd'; break; // Right
                        case 75: c = 'a'; break; // Left
                        default: break;
                    }
                }
        #endif
            } else {
                c = '\0';
            }

            update_game(c);
            draw_game();
        #ifndef _WIN32
            usleep(90000);
        #else
            Sleep(90);
        #endif

            // 게임 오버 처리 여기서
            if (collision_count >= 3) {
                restart=ending(0);
                break;
            }

            if (maps[player_y][player_x] == 'E') {
                curr_stage++;
                load_maps2();
                if (curr_stage != -1) {
                    score += 100;
                    init_stage();
                } else {
                    restart = ending(1); // 사용자 선택(r/q) 받기
                    break;
                }
            }
        }
        // 재시작 여부 확인
        if (restart == 1)
            continue;
        else
            break;
    }

    free_maps();
    disable_raw_mode();
    return 0;
}


// 터미널 Raw 모드 활성화/비활성화
void disable_raw_mode(void) {
    #ifndef _WIN32
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    #endif
}
void enable_raw_mode(void) {
#ifndef _WIN32
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
#endif
}

// 맵 파일 로드


/*void load_maps(void) {
    FILE *file = fopen("map.txt", "r");
    if (!file) {
        perror("map.txt 파일을 열 수 없습니다.");
        exit(1);
    }
    int s = 0, r = 0;
    char line[MAP_WIDTH + 2]; // 버퍼 크기는 MAP_WIDTH에 따라 자동 조절됨
    while (s < max_x && fgets(line, sizeof(line), file)) {
        if ((line[0] == '\n' || line[0] == '\r') && r > 0) {
            s++;
            r = 0;
            continue;
        }
        if (r < MAP_HEIGHT) {
            line[strcspn(line, "\n\r")] = 0;
            strncpy(map[s][r], line, MAP_WIDTH + 1);
            r++;
        }
    }
    fclose(file);
}*/

void load_maps2(void){
    FILE *file = fopen("map.txt", "r");
    if (!file) {
        perror("map.txt 파일을 열 수 없습니다.");
        exit(1);
    }
    free_maps();
    //printf("file x : %d", x);
    //exit(0);
    /*몇 개의 Map이 있는지 세기(max_z)*/

    int x = 0, y = 0, max_x = 0, max_y = 0, max_z = 0;
    char ch;
    while(1){
        ch = fgetc(file);
        if(ch == EOF){
            if(y > 0){
                if(max_y < y){
                    max_y = y;
                }
            }
            else{
                curr_stage = -1;
                max_width = 0;
                max_height = 0;
                fclose(file);
                return;
            }
            break;
        }
        else if(ch == '\n'){
            if(y == 0 && x == 0){
                continue;
            }
            if(x > 0){
                y++;
                if(max_x < x){
                    max_x = x;
                }
                x = 0;
            }
            else{
                if(max_y < y){
                    max_y = y;
                }
                if(max_z >= curr_stage){
                    break;
                }
                max_z++;
                //make_new_map_size(max_x, max_y, max_z);
                //printf("max_x : %d, max_y : %d, max_z : %d\n", max_x, max_y, max_z);
                x = y = max_x = max_y = 0;
            }
        }
        else if(ch != '\r'){
            x++;
        }
    }
    if(y > 0){
        max_width = max_x;
        max_height = max_y;
        //make_new_map_size(max_x, max_y, max_z);
        //printf("max_x : %d, max_y : %d, max_z : %d\n", max_x, max_y, max_z);
    }
    clrscr();
    printf("curr_z : %d, max_x : %d, max_y : %d", curr_stage, max_width, max_height);
#ifndef _WIN32
    getchar();
#else
    getch();
#endif
    int skip_data = 0, skip_until_new_line = 0;
    rewind(file);
    maps = (char **)malloc(sizeof(char *) * max_height);
    if(maps == NULL){
        fclose(file);
        return;
    }
    for(int ty = 0; ty < max_y; ty++){
        maps[ty] = (char *)malloc(sizeof(char) * (max_width + 3));
        if(maps[ty] == NULL){
            free_maps();
            fclose(file);
            return;
        }
        while(fgets(maps[ty], max_width + 3, file) != NULL){
            char temp_c = maps[ty][0];
            if(!(temp_c == '\n' || temp_c == '\r')){
                if(skip_until_new_line == 1){
                    continue;
                }
                if(skip_data < curr_stage){
                    skip_data++;
                    skip_until_new_line = 1;
                    continue;
                }
                break;
            }
            else if(skip_data > 0){
                skip_until_new_line = 0;
            }
        }
    }
    fclose(file);
}

void free_maps(void){
    if(maps != NULL){
        for(int y = 0; y < max_height; y++){
            if(maps[y] != NULL){
                free(maps[y]);
            }
            else{
                break;
            }
        }
        free(maps);
        maps = NULL;
        return;
    }
}

// 현재 스테이지 초기화
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

// 게임 화면 그리기
void draw_game(void) {
    clrscr();
    printf("Stage: %d | Score: %d | HEART: %d\n", curr_stage + 1, score, 3 - collision_count);
    printf("조작: ← → (이동), ↑ ↓ (사다리), Space (점프), q (종료)\n");
    /*char **display_map = (char **)malloc(sizeof(char *) * max_height);
    if(display_map == NULL){

    } 동적배열로 바꿔야하는지 확인 필요*/
    char display_map[max_height][max_width + 1];
    for(int y=0; y < max_height; y++) {
        for(int x=0; x < max_width; x++) {
            char cell = maps[y][x];
            if (cell == 'S' || cell == 'X' || cell == 'C') {
                display_map[y][x] = ' ';
            } else {
                display_map[y][x] = cell;
            }
        }
    }

    for (int i = 0; i < coin_count; i++) {
        if (!coins[i].collected) {
            display_map[coins[i].y][coins[i].x] = 'C';
        }
    }

    for (int i = 0; i < enemy_count; i++) {
        display_map[enemies[i].y][enemies[i].x] = 'X';
    }

    display_map[player_y][player_x] = 'P';

    for (int y = 0; y < max_height; y++) {
        for(int x=0; x< max_width; x++){
            printf("%c", display_map[y][x]);
        }
        printf("\n");
    }
}

// 게임 상태 업데이트
void update_game(char input) {
    move_player(input);
    move_enemies();
    check_collisions();
}

// 플레이어 이동 로직
void move_player(char input) {
    int next_x = player_x, next_y = player_y;
    char floor_tile = (player_y + 1 < max_height) ? maps[player_y + 1][player_x] : '#';
    char current_tile = maps[player_y][player_x];

    on_ladder = (current_tile == 'H');

    switch (input) {
        case 'a': next_x--; break;
        case 'd': next_x++; break;
        case 'w': if (on_ladder) next_y--; break;
        case 's': if (on_ladder && (player_y + 1 < max_height) && maps[player_y + 1][player_x] != '#') next_y++; break;
        case ' ':
            if (!is_jumping && (floor_tile == '#' || on_ladder)) {
                is_jumping = 1;
                velocity_y = -3;
                play_jump_sound();
            }
            break;
    }

    if (next_x >= 0 &&
        next_x < max_width){
        if (maps[player_y][next_x] != '#') {
            player_x = next_x;
            floor_tile = (player_y + 1 < max_height) ? maps[player_y + 1][player_x] : '#'; //x좌표가 변경된 후 floor_tile값도 변경 필요(!is_jumping일 때)
        }
    }

    if (on_ladder && (input == 'w' || input == 's')) {
        if(next_y >= 0 && next_y < max_height && maps[next_y][player_x] != '#') {
            player_y = next_y;
            floor_tile = (player_y + 1 < max_height) ? maps[player_y + 1][player_x] : '#'; //y좌표가 변경된 후 floor_tile값도 변경 필요(!is_jumping일 때)
            is_jumping = 0;
            velocity_y = 0;
        }
    }
    else {
        if (is_jumping) {
            next_y = player_y + ((velocity_y >= 3) ? 1 : -1);
            if(next_y < 0) next_y = 0;
            velocity_y++;
            //수정 후
            if (velocity_y >= 1 && velocity_y <= 3) return;
            if (next_y < max_height){
                if (maps[next_y][player_x] == '#'){
                    if (velocity_y <= 0){
                        if (!on_ladder){
                            velocity_y = 0;
                        }else if(velocity_y<=-2){
                            player_y = next_y-1;
                            is_jumping = 0;
                        }
                    } else if (velocity_y > 0){
                        is_jumping = 0;
                    }
                }
                else{
                    player_y = next_y;
                }
            }
            /* 수정 전
             if (velocity_y <= 0 && next_y < MAP_HEIGHT && map[stage][next_y][player_x] == '#' && !on_ladder) {
                velocity_y = 0;
            } else if (velocity_y > 0 && next_y < MAP_HEIGHT && map[stage][next_y][player_x] == '#'){
                is_jumping = 0;
            } else if (next_y < MAP_HEIGHT) {
                if(on_ladder){
                    if (map[stage][next_y][player_x] == '#'){
                        if(velocity_y<=-2){
                            player_y = next_y-1;
                            is_jumping = 0;
                        }
                    }
                    else{
                        player_y = next_y;
                    }
                }
                else{
                    player_y = next_y;
                }
            }
            */
            if ((player_y + 1 < max_height) && maps[player_y + 1][player_x] == '#') {
                is_jumping = 0;
                velocity_y = 0;
            }
        } else {
            if (floor_tile != '#' && floor_tile != 'H' && !on_ladder) {
                 if (player_y + 1 < max_height) player_y++;
                 else init_stage();
            }
        }
    }

    if (player_y >= max_height) init_stage();
}


// 적 이동 로직
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

// 충돌 감지 로직
void check_collisions(void) {
    for (int i = 0; i < enemy_count; i++) {
        if (player_x == enemies[i].x && player_y == enemies[i].y) {
            collision_count++; // 충돌 횟수 증가
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

// 비동기 키보드 입력 확인
#ifndef _WIN32
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
    if(ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;

}
#endif

// 오프닝 출력 함수
void opening(void) {
    clrscr();  // 화면 지우기

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
// 엔딩 출력 함수
int ending(int result) {
    clrscr(); // 화면 지우기
    printf("\n\n");
    printf("    ##########################################\n");
    printf("    #                                        #\n");

    if (result == 1) {
        // 게임 클리어
        printf("    #    C O N G R A T U L A T I O N S       #\n");
        printf("    #         ALL STAGES CLEARED!            #\n");
    } else {
        // 게임 오버
        printf("    #          G A M E   O V E R             #\n");
        printf("    #           Try Again ...                #\n");
    }

    printf("    #                                        #\n");
    printf("    #        FINAL SCORE: %-5d              #\n", score);
    printf("    #                                        #\n");
    printf("    ##########################################\n");
    printf("\n");

    // 추가: 선택지 안내
    printf("     Press 'r': Restart / 'q': Quit \n");

   // 추가: 입력대기
    while(1) {
        char c
#ifndef _WIN32
        = getchar();
#else
        = getch();
#endif
        if (c == 'r' || c == 'R') return 1;
        else if (c == 'q' || c == 'Q') return 0;
    }


}
void reset_game(void) { // 재시작하면 초기화
    curr_stage = 0;
    score = 0;
    collision_count = 0;
}

// 점프 효과음
void play_jump_sound(void) {
#ifdef _WIN32
    Beep(440, 45);
    fflush(stdout);
#elif __APPLE__
    system("afplay /System/Library/Sounds/Tink.aiff > /dev/null 2>&1 &"); //mac 버전 stdlib.h 사용
    fflush(stdout);
#else
    printf("\007");
    // ASCII 코드 7번 BELL (작동여부 확인 필요)
    fflush(stdout);
#endif
}

void play_nextstage_sound(void) {
    if (curr_stage == 0) return;
#ifdef _WIN32
    Beep(440, 45);
    Beep(660, 135);
#elif __APPLE__

#else

#endif
}
