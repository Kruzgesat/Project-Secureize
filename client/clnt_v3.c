#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ncurses.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

// MESSAGE BUFFER, SOCKET BUFFER, USER NAME INPUT
#define ST_SIZE 12
#define BUF_SIZE 1024
#define NAME_SIZE 20

// 키보드 키 상수값
#define UP 65
#define DOWN 66
#define LEFT 68
#define RIGHT 67
#define ENTER 10
#define BACKSPACE 127
#define DELETE 126
#define SPACEBAR 32
#define TAB 9
#define ESC 27
#define HOME 72
#define END 70

// UI 상수
#define MAIN_UI 200
#define READY_UI 300
#define QUIZ_UI 400
#define RESULT_UI 500

// 난이도 상수
#define BEGINNER 600
#define INTERMEDIATE 700
#define EXPERT 800

// 사용자의 정보
struct Player
{
    char name[NAME_SIZE];
    char difficulty[50];
    bool is_ready;
    int score;
};

struct Player user;       // 본인 정보
struct Player rival_user; // 상대 정보

// 기능
void error_handling(char *buf);
void kb_handling(int sock); // 키보드 입력 제어
void game_manager();        // 게임 매니져
int center_alignment(char *str, int len);

// 스레드 함수
void *recv_msg(void *arg); // 메시지 수신 스레드 함수
void *mapping(void *arg); // 게임 스레드 함수

// UI
void start_ui();  // 접속 UI
void main_ui();   // 메인 UI
void ready_ui();  // 준비 UI
void quiz_ui();   // 퀴즈 UI
void result_ui(); // 결과 UI

int current_ui = MAIN_UI;

char my_name[20];  // 유저 이름
char ip_addr[20];  // IP 주소
char port_num[20]; // 포트 번호

void main()
{
    // 커서 안보이게 설정
    curs_set(0);

    int sock;                     // 소켓
    struct sockaddr_in serv_addr; // 소켓의 주소 정보

    pthread_t recv_thr; // 메시지 수신 스레드
    pthread_t mapping_thr; // 맵핑 스레드
    void *thread_return;

    // 접속 UI
    start_ui();

    // 소켓 생성
    sock = socket(PF_INET, SOCK_STREAM, 0);

    // 서버 주소 정보 입력
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip_addr);
    serv_addr.sin_port = htons(atoi(port_num));

    // 서버 접속
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        error_handling("connect() error");
    }

    pthread_create(&recv_thr, NULL, recv_msg, (void *)&sock);
    pthread_create(&mapping_thr, NULL, mapping, (void *)&sock);

    pthread_join(recv_thr, &thread_return);
    pthread_join(mapping_thr, &thread_return);

    close(sock);
}

// 에러 제어
void error_handling(char *buf)
{
    fputs(buf, stderr);
    fputc('\n', stderr);
    exit(1);
}

// 가운데 정렬 기준 값 반환 함수
int center_alignment(char *str, int len)
{
    // 가운제 정렬 기준 값 계산
    int center_alignment_value = (len - strlen(str)) / 2;

    return center_alignment_value;
}

// 메시지 수신 스레드 함수
void *recv_msg(void *arg)
{
    int sock = *((int *)arg);
    char recv_buf[ST_SIZE + NAME_SIZE + BUF_SIZE]; // 수신 메세지 원본
    char buf_copy[ST_SIZE + NAME_SIZE + BUF_SIZE]; // 수신 메세지 복사본
    char *tmp;

    int str_len;

    // 초기화
    memset(buf_copy, 0, sizeof(buf_copy));
    memset(recv_buf, 0, sizeof(recv_buf));

    while (1)
    {
        // 메시지 읽어오기
        read(sock, recv_buf, ST_SIZE + NAME_SIZE + BUF_SIZE - 1);

        if (str_len == -1)
        {
            return (void *)-1;
        }
        else
        {
            tmp = strtok(recv_buf, " ");

            if (!strcmp(tmp, "READY"))
            {
                if (strcmp(tmp, user.name))
                {
                    // 대결 상대 정보 초기화
                    memset(rival_user.name, 0, sizeof(rival_user.name));
                    memset(rival_user.difficulty, 0, sizeof(rival_user.difficulty));
                    rival_user.is_ready = false;
                    rival_user.score = 0;

                    // 대결 상대 정보 입력
                    strcpy(rival_user.name, tmp);
                    tmp = strtok(NULL, " ");
                    strcpy(rival_user.difficulty, tmp);
                    tmp = strtok(NULL, " ");
                    rival_user.is_ready = atoi(tmp);
                    tmp = strtok(NULL, " ");
                    rival_user.score = atoi(tmp);
                }
            }
        }
    }
}

// 키보드 제어
void kb_handling(int sock)
{
    char msg[ST_SIZE + NAME_SIZE + BUF_SIZE]; // 송신할 메세지 내용
    memset(msg, 0, sizeof(msg));

    while (1)
    {
        // 키보드 입력
        int kb_value = getch();

        // 메인 UI 키 입력 제어
        if (current_ui == MAIN_UI)
        {
            switch (kb_value)
            {
            case '1':
                strcpy(user.difficulty, "BEGINNER");
                break;

            case '2':
                strcpy(user.difficulty, "INTERMEDIATE");
                break;

            case '3':
                strcpy(user.difficulty, "EXPERT");
                break;

            case ESC:
                exit(0);
                break;

            default:
                break;
            }

            // 서버에 BEGINNER 문제 요청
            sprintf(msg, "REQUEST %s", user.difficulty);
            write(sock, msg, strlen(msg));
            memset(msg, 0, sizeof(msg));

            current_ui = READY_UI;
        }
        // 준비 UI 키 입력 제어
        if (current_ui == READY_UI)
        {
            // 사용자 입력 정답 전송
            switch (kb_value)
            {
            case 'y':
                user.is_ready = true;

                sprintf(msg, "READY %s %s %d %d", user.name, user.difficulty, user.is_ready, user.score);
                write(sock, msg, strlen(msg));
                memset(msg, 0, sizeof(msg));

                if (user.is_ready == true & rival_user.is_ready == true)
                {
                    current_ui = QUIZ_UI;
                }
                break;

            case 'n':
                current_ui = MAIN_UI;
                break;

            case ESC:
                exit(0);
                break;

            default:
                break;
            }
        }
        // 퀴즈 UI 키 입력 제어
        if (current_ui == QUIZ_UI)
        {
            // 사용자 입력 정답 전송
            switch (kb_value)
            {
            case '1':
                break;

            case '2':
                break;

            case '3':
                break;

            case '4':
                break;

            case ESC:
                exit(0);
                break;

            default:
                break;
            }

            // memset(question.q_text, 0, sizeof(question.q_text));
            // memset(question.a_text, 0, sizeof(question.a_text));
            // memset(question.b_text, 0, sizeof(question.b_text));
            // memset(question.c_text, 0, sizeof(question.c_text));
            // memset(question.d_text, 0, sizeof(question.d_text));
        }
        // 결과 UI 키 입력 제어
        if (current_ui == RESULT_UI)
        {
            switch (kb_value)
            {
            case '1':
                current_ui = MAIN_UI;
                break;

            case '2':
                current_ui = MAIN_UI;
                break;

            case ESC:
                exit(0);
                break;

            default:
                break;
            }
        }
    }
}

// 게임 시스템
void *mapping(void *arg)
{
    int sock = *((int *)arg);

    while (1)
    {

        // 화면 초기화
        clear();
        initscr();

        if (current_ui == MAIN_UI)
        {
            main_ui();
        }
        else if (current_ui == READY_UI)
        {
            ready_ui();
        }
        else if (current_ui == QUIZ_UI)
        {
            quiz_ui();
        }
        else if (current_ui == RESULT_UI)
        {
            result_ui();
        }

        // 화면 새로 고침
        refresh();
        endwin();
    }
}

// 시작 UI
void start_ui()
{
    // 화면 초기화
    clear();
    initscr();

    // 접속 윈도우
    WINDOW *connect_window = newwin(15, 82, 0, 0);
    box(connect_window, '|', '-');

    // IP 주소 입력 부분
    mvwprintw(connect_window, 1, 2, "Enter Server's IP Address");
    mvwgetstr(connect_window, 2, 2, ip_addr);

    // 포트 번호 입력 부분
    mvwprintw(connect_window, 4, 2, "Enter Server's Port Number");
    mvwgetstr(connect_window, 5, 2, port_num);

    // 유저 이름 입력 부분
    mvwprintw(connect_window, 7, 2, "Enter Your Name");
    mvwgetstr(connect_window, 8, 2, my_name);

    // 본인 정보 초기화
    strcpy(user.name, my_name);
    memset(user.difficulty, 0, sizeof(user.difficulty));
    user.is_ready = false;
    user.score = 0;

    current_ui = MAIN_UI;

    // 화면 새로 고침
    refresh();
    wrefresh(connect_window);

    endwin();
}

// 메인 UI
void main_ui()
{
    // 화면 초기화
    clear();
    initscr();

    // 게임 소개 윈도우
    WINDOW *introdution_window = newwin(15, 81, 0, 0);
    box(introdution_window, '|', '-');

    // 게임 소개 출력
    mvwprintw(introdution_window, 6, center_alignment("Project Secureize", 81), "Project Secureize");
    mvwprintw(introdution_window, 8, center_alignment("Advanced Systdfem Sercurity Quiz Program!!!", 81), "Advanced Systdfem Sercurity Quiz Program!!!");

    // 난이도 BEGINNER 윈도우
    WINDOW *beginner_window = newwin(5, 27, 15, 0);
    box(beginner_window, '|', '-');

    mvwprintw(beginner_window, 2, center_alignment("[1] LV. BEGINNER", 27), "[1] LV. BEGINNER");

    // 난이도 INTERMEDIATE 윈도우
    WINDOW *intermediate_window = newwin(5, 27, 15, 27);
    box(intermediate_window, '|', '-');

    mvwprintw(intermediate_window, 2, center_alignment("[2] LV. INTERMEDIATE", 27), "[2] LV. INTERMEDIATE");

    // 난이도 EXPERT 윈도우
    WINDOW *expert_window = newwin(5, 27, 15, 54);
    box(expert_window, '|', '-');

    mvwprintw(expert_window, 2, center_alignment("[3] LV. EXPERT", 27), "[3] LV. EXPERT");

    // 화면 새로 고침
    refresh();
    wrefresh(introdution_window);
    wrefresh(beginner_window);
    wrefresh(intermediate_window);
    wrefresh(expert_window);

    // 키보드 입력
    int kb_value = getch();

    switch (kb_value)
    {
    case '1':
        current_ui = QUIZ_UI;
        break;

    case '2':
        current_ui = QUIZ_UI;
        break;

    case '3':
        current_ui = QUIZ_UI;
        break;

    case ESC:
        exit(0);
        break;

    default:
        break;
    }

    endwin();
}

// 준비 UI
void ready_ui()
{
    // 준비 윈도우
    WINDOW *ready_window = newwin(10, 82, 0, 0);
    box(ready_window, '|', '-');

    mvwprintw(ready_window, 4, center_alignment("Waiting for the next Challenger ! ! !", 82), "Waiting for the next Challenger ! ! !");
    mvwprintw(ready_window, 5, center_alignment("Press Key [y] to Ready and Matching", 82), "Press Key [y] to Ready and Matching");

    // 플레이어 1 윈도우
    WINDOW *ready_player1_window = newwin(15, 41, 10, 0);
    box(ready_player1_window, '|', '-');

    // 플레이어 1 (본인 플레이어) 준비 상태 출력
    mvwprintw(ready_player1_window, 4, 2, "Player 1 [%s]", user.name);
    mvwprintw(ready_player1_window, 6, 2, "Difficulty [%s]", user.difficulty);
    if (user.is_ready)
    {
        mvwprintw(ready_player1_window, 8, center_alignment("= Ready =", 41), "= Ready =");
    }
    else
    {
        mvwprintw(ready_player1_window, 8, center_alignment("= Not Ready =", 41), "= Not Ready =");
    }

    // 플레이어 2 윈도우
    WINDOW *ready_player2_window = newwin(15, 41, 10, 41);
    box(ready_player2_window, '|', '-');

    // 플레이어 2 (상대 플레이어) 준비 상태 출력
    mvwprintw(ready_player2_window, 4, 2, "Player 2 [%s]", rival_user.name);
    mvwprintw(ready_player2_window, 6, 2, "Difficulty [%s]", rival_user.difficulty);
    if (rival_user.is_ready)
    {
        mvwprintw(ready_player2_window, 8, center_alignment("= Ready =", 41), "= Ready =");
    }
    else
    {
        mvwprintw(ready_player2_window, 8, center_alignment("= Not Ready =", 41), "= Not Ready =");
    }

    wrefresh(ready_window);
    wrefresh(ready_player1_window);
    wrefresh(ready_player2_window);
}

// 퀴즈 UI
void quiz_ui()
{
    // 화면 초기화
    clear();
    initscr();

    // 게임 소개 윈도우
    WINDOW *question_window = newwin(15, 82, 0, 0);
    box(question_window, '|', '-');

    mvwprintw(question_window, 1, 2, "Level : ");
    mvwprintw(question_window, 2, 2, "Score : ");

    // 문제를 임시저장할 문자열
    char question_str[79] = "please enter question string";
    mvwprintw(question_window, 7, center_alignment(question_str, 81), "%s", question_str);

    // 선택지 1번 윈도우
    WINDOW *select1_window = newwin(5, 41, 15, 0);
    box(select1_window, '|', '-');

    mvwprintw(select1_window, 1, 1, "[1]");
    char select1_str[41] = "Select 1";
    mvwprintw(select1_window, 2, center_alignment(select1_str, 41), "%s", select1_str);

    // 선택지 2번 윈도우
    WINDOW *select2_window = newwin(5, 41, 15, 41);
    box(select2_window, '|', '-');
    mvwprintw(select2_window, 1, 1, "[2]");
    char select2_str[41] = "Select 2";
    mvwprintw(select2_window, 2, center_alignment(select2_str, 41), "%s", select2_str);

    // 선택지 3번 윈도우
    WINDOW *select3_window = newwin(5, 41, 20, 0);
    box(select3_window, '|', '-');

    mvwprintw(select3_window, 1, 1, "[3]");
    char select3_str[41] = "Select 3";
    mvwprintw(select3_window, 2, center_alignment(select3_str, 41), "%s", select3_str);

    // 선택지 4번 윈도우
    WINDOW *select4_window = newwin(5, 41, 20, 41);
    box(select4_window, '|', '-');

    mvwprintw(select4_window, 1, 1, "[4]");
    char select4_str[41] = "Select 4";
    mvwprintw(select4_window, 2, center_alignment(select4_str, 41), "%s", select4_str);

    // 화면 새로 고침
    refresh();
    wrefresh(question_window);
    wrefresh(select1_window);
    wrefresh(select2_window);
    wrefresh(select3_window);
    wrefresh(select4_window);

    // 키보드 입력
    int kb_value = getch();

    switch (kb_value)
    {
    case '1':
        current_ui = RESULT_UI;
        break;

    case '2':
        current_ui = RESULT_UI;
        break;

    case '3':
        current_ui = RESULT_UI;
        break;

    case '4':
        current_ui = RESULT_UI;
        break;

    case ESC:
        exit(0);
        break;

    default:
        break;
    }

    endwin();
}

// 결과 UI
void result_ui()
{
    // 화면 초기화
    clear();
    initscr();

    // 게임 소개 윈도우
    WINDOW *result_window = newwin(25, 82, 0, 0);
    box(result_window, '|', '-');

    mvwprintw(result_window, 1, 2, "#####    ######    #####   ##  ##   ##       ######    #######################");
    mvwprintw(result_window, 2, 2, "######   ######   ######   ##  ##   ##       ######    #######################");
    mvwprintw(result_window, 3, 2, "##   #   ##       ##       ##  ##   ##         ##      #######################");
    mvwprintw(result_window, 4, 2, "##   #   ##       ##       ##  ##   ##         ##      #######################");
    mvwprintw(result_window, 5, 2, "#####    ####      ##      ##  ##   ##         ##      #######################");
    mvwprintw(result_window, 6, 2, "#####    ####       ##     ##  ##   ##         ##      #######################");
    mvwprintw(result_window, 7, 2, "## ##    ##          ##    ##  ##   ##         ##      #######################");
    mvwprintw(result_window, 8, 2, "##  ##   ##           ##   ##  ##   ##         ##      #######################");
    mvwprintw(result_window, 9, 2, "##  ##   ##       ######   ######   ##         ##      #######################");
    mvwprintw(result_window, 10, 2, "##  ##   ######   #####     ####    ######     ##      #######################");

    mvwprintw(result_window, 17, 2, "Number of questions answered\t");
    mvwprintw(result_window, 19, 2, "Number of incorrect questions\t");

    // 화면 새로 고침
    refresh();
    wrefresh(result_window);

    // 키보드 입력
    int kb_value = getch();

    switch (kb_value)
    {
    case '1':
        current_ui = MAIN_UI;
        break;

    case '2':
        current_ui = MAIN_UI;
        break;

    case ESC:
        exit(0);
        break;

    default:
        break;
    }

    endwin();
}
