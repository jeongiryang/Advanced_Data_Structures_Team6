#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>

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

typedef struct map_size{
    int max_x;
    int max_y;
    int curr_z;
    struct map_size *next;
}Map_size;

// 전역 변수
Map_size * map_size_head = NULL; //추가
char ***maps = NULL; //추가
int max_stages = 0; //추가
int max_height = 0;
int max_width = 0;
int player_x, player_y;
int stage = 0;
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
struct termios orig_termios;
// 함수 선언
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
void load_maps2(int *);
void free_map_size(Map_size *);
void free_memory(void);
void init_stage(void) ;
void free_maps(int);
int main(void) {
    srand((unsigned)time(NULL));
    enable_raw_mode();
    load_maps2(&max_stages);
    if(maps == NULL){
        printf("\x1b[2J\x1b[H");
        printf("stage loading error");
        free_memory();
        exit(1);
    }
    init_stage();
    char c = '\0';
    int game_over = 0;

    while (!game_over && stage < max_stages) {
        if (kbhit()) {
            c = getchar();
            if (c == 'q') {
                game_over = 1;
                free_memory();
                continue;
            }
            if (c == '\x1b') {
                getchar(); // '['
                switch (getchar()) {
                    case 'A': c = 'w'; break; // Up
                    case 'B': c = 's'; break; // Down
                    case 'C': c = 'd'; break; // Right
                    case 'D': c = 'a'; break; // Left
                }
            }
        } else {
            c = '\0';
        }

        update_game(c);
        draw_game();
        usleep(90000);

        if (collision_count >= 3) {
            game_over = 1;
            printf("\x1b[2J\x1b[H");
            printf("Game Over! 생명이 0이 되었습니다!\n");
            printf("최종 점수: %d\n", score);
            continue;
        }

        if (maps[stage][player_y][player_x] == 'E') {
            stage++;
            score += 100;
            if (stage < max_stages) {
                init_stage();
            } else {
                game_over = 1;
                printf("\x1b[2J\x1b[H");
                printf("축하합니다! 모든 스테이지를 클리어했습니다!\n");
                printf("최종 점수: %d\n", score);
            }
        }
    }
    free_memory();
    disable_raw_mode();
    return 0;
}


// 터미널 Raw 모드 활성화/비활성화

void disable_raw_mode(void) { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }
void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
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

void make_new_map_size(int max_x, int max_y, int max_z){
    Map_size *new_map_size = (Map_size *)malloc(sizeof(Map_size));
    if(new_map_size == NULL){
        free_map_size(map_size_head);
        printf("\x1b[2J\x1b[H");
        printf("메모리 할당 실패");
        exit(1); //맵 데이터를 받지 않았으므로 바로 종료
    }
    new_map_size->max_x = max_x;
    new_map_size->max_y = max_y;
    new_map_size->curr_z = max_z;
    new_map_size->next = NULL;
    Map_size *temp = map_size_head;
    if(temp){
        while(temp -> next){
            temp = temp->next;
        }
        temp->next = new_map_size;
    }
    else{
        map_size_head = new_map_size;
    }
}
void load_maps2(int *ret){
    FILE *file = fopen("map.txt", "r");
    if (!file) {
        perror("map.txt 파일을 열 수 없습니다.");
        exit(1);
    }

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
                make_new_map_size(max_x, max_y, max_z);
                printf("max_x : %d, max_y : %d, max_z : %d\n", max_x, max_y, max_z);
                max_z++;
                x = y = max_x = max_y = 0;
            }
        }
        else if(ch != '\r'){
            x++;
        }
    }
    if(y > 0){
        make_new_map_size(max_x, max_y, max_z);
        printf("max_x : %d, max_y : %d, max_z : %d\n", max_x, max_y, max_z);
        max_z++;
    }
    rewind(file);
    Map_size * temp = map_size_head;
    if(temp == NULL){
        printf("맵 불러오기 실패");
        return;
    }
    maps = (char ***)malloc(sizeof(char **) * max_z);
    if(maps == NULL){
        free_map_size(map_size_head);
        return;
    }
    while(temp!= NULL){
        maps[temp->curr_z]= (char **)malloc(sizeof(char *) * temp->max_y);
        if(maps[temp->curr_z] == NULL){
            free_maps(temp->curr_z);
            return;
        }
        
        for(int ty = 0; ty < temp-> max_y; ty++){
            maps[temp->curr_z][ty] = (char *)malloc(sizeof(char) * (temp->max_x + 3));
            if(maps[temp->curr_z][ty] == NULL){
                free_maps(temp->curr_z);
                return;
            }
            while(fgets(maps[temp->curr_z][ty], temp->max_x + 3, file) != NULL){
                char temp_c = maps[temp->curr_z][ty][0];
                if(!(temp_c == '\n' || temp_c == '\r')){
                    break;
                }
            }
        }
        temp = temp->next;
    }
    *ret = max_z;
}
void store_max_x_max_y(int* max_x, int* max_y){
    Map_size* temp = map_size_head;
    for(int i = 0; i<stage; i++){
        temp = temp -> next;
        if(temp == NULL){
            printf("\x1b[2J\x1b[H");
            printf("stage loading error");
            free_memory();
            
            exit(1);
            
        }
    }
    *max_x = temp->max_x;
    *max_y = temp->max_y;
}
void free_maps(int failed_z){
    /*
    for(int z = 0; z<max_stages; z++){
        stage = z;
        store_max_x_max_y(&max_width, &max_height);
        for(int y = 0; y<max_height; y++){
            free(maps[z][y]);
        }
        free(maps[z]);
    }
    free(maps);*/
    if(maps != NULL){
        Map_size *temp = map_size_head;
        while(temp != NULL && temp -> curr_z < failed_z){
            if(maps[temp->curr_z] != NULL){
                for(int j = 0; j < temp->max_y; j++){
                    if(maps[temp->curr_z][j] != NULL){
                        free(maps[temp->curr_z][j]);
                    }
                    else{
                        break;
                    }
                }
                free(maps[temp->curr_z]);
            }
            else{
                break;
            }
            temp = temp->next;
        }
        free(maps);
        maps = NULL;
        return;
    }
}



void free_map_size(Map_size *ms){
    if(ms != NULL){
        free_map_size(ms ->next);
    }
    free(ms);
}
void free_memory(void){
    free_maps(max_stages);
    free_map_size(map_size_head);
}
// 현재 스테이지 초기화
void init_stage(void) {
    enemy_count = 0;
    coin_count = 0;
    is_jumping = 0;
    velocity_y = 0;
    store_max_x_max_y(&max_width, &max_height);
    for (int y = 0; y < max_height; y++) {
        for (int x = 0; x < max_width; x++) {
            char cell = maps[stage][y][x];
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
    printf("\x1b[2J\x1b[H");
    printf("Stage: %d | Score: %d\n", stage + 1, score);
    printf("조작: ← → (이동), ↑ ↓ (사다리), Space (점프), q (종료)\n");
    char display_map[max_height][max_width + 1];
    for(int y=0; y < max_height; y++) {
        for(int x=0; x < max_width; x++) {
            char cell = maps[stage][y][x];
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
    char floor_tile = (player_y + 1 < max_height) ? maps[stage][player_y + 1][player_x] : '#';
    char current_tile = maps[stage][player_y][player_x];

    on_ladder = (current_tile == 'H');

    switch (input) {
        case 'a': next_x--; break;
        case 'd': next_x++; break;
        case 'w': if (on_ladder) next_y--; break;
        case 's': if (on_ladder && (player_y + 1 < max_height) && maps[stage][player_y + 1][player_x] != '#') next_y++; break;
        case ' ':
            if (!is_jumping && (floor_tile == '#' || on_ladder)) {
                is_jumping = 1;
                velocity_y = -3;
            }
            break;
    }
    
    if (next_x >= 0 &&
        next_x < max_width){
        if (maps[stage][player_y][next_x] != '#') {
            player_x = next_x;
            floor_tile = (player_y + 1 < max_height) ? maps[stage][player_y + 1][player_x] : '#'; //x좌표가 변경된 후 floor_tile값도 변경 필요(!is_jumping일 때)
        }
    }
    
    if (on_ladder && (input == 'w' || input == 's')) {
        if(next_y >= 0 && next_y < max_height && maps[stage][next_y][player_x] != '#') {
            player_y = next_y;
            floor_tile = (player_y + 1 < max_height) ? maps[stage][player_y + 1][player_x] : '#'; //y좌표가 변경된 후 floor_tile값도 변경 필요(!is_jumping일 때)
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
                if (maps[stage][next_y][player_x] == '#'){
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
            if ((player_y + 1 < max_height) && maps[stage][player_y + 1][player_x] == '#') {
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
        if (next_x < 0 || next_x >= max_width || maps[stage][enemies[i].y][next_x] == '#' || (enemies[i].y + 1 < max_height && maps[stage][enemies[i].y + 1][next_x] == ' ')) {
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
