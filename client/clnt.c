#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
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
    bool my_win;
    bool is_end;
    int score;
};

struct Player user;       // 본인 정보
struct Player rival_user; // 상대 정보

// 개별 문제 Struct
typedef struct Question
{
    int q_num;
    char q_text[1000];
    char a_text[200];
    char b_text[200];
    char c_text[200];
    char d_text[200];
    char q_ans;

} Question;

struct Question *questions;

// 기능
void error_handling(char *buf);
int center_alignment(char *str, int len);

// 스레드 함수
void *recv_msg(void *arg);    // 메시지 수신 스레드 함수
void *mapping(void *arg);     // 게임 스레드 함수
void *kb_handling(void *arg); // 키보드 입력 관리 스레드 함수

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

int q_index[10] = {0};
int q_count = 0;

void main()
{
    // 커서 안보이게 설정
    curs_set(0);

    int sock;                     // 소켓
    struct sockaddr_in serv_addr; // 소켓의 주소 정보

    pthread_t recv_thr;        // 메시지 수신 스레드
    pthread_t mapping_thr;     // 맵핑 스레드
    pthread_t kb_handling_thr; // 키보드 입력 제어 스레드
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
    pthread_create(&kb_handling_thr, NULL, kb_handling, (void *)&sock);

    pthread_join(recv_thr, &thread_return);
    pthread_join(mapping_thr, &thread_return);
    pthread_join(kb_handling_thr, &thread_return);

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

// 문제 로드
int q_load(int max_questions, char *filename)
{
    questions = (Question *)malloc(sizeof(Question) * max_questions);

    // CSV 파일을 읽기 모드로 열기 시도
    FILE *file = fopen(filename, "r");

    // 파일이 성공적으로 열렸는지 확인
    if (file == NULL)
    {
        printf("%s Question File Open Error.\nPlease Check The File Is Exsist\n", filename);
        return 1; // 오류 코드 반환
    }

    // 한 줄씩 CSV 데이터를 읽고 처리하기
    char line[1024];        // Maximum 줄 수
    int question_count = 0; // 질문 수를 추적하기 위한 변수

    while (fgets(line, sizeof(line), file) && question_count < max_questions)
    {
        // 토큰화된 필드를 저장하기 위한 변수
        char *q_num_str, *q_text, *a_text, *b_text, *c_text, *d_text, *q_ans_str;

        // 각 토큰별 빌드
        q_num_str = strtok(line, ",");
        q_text = strtok(NULL, ",");
        a_text = strtok(NULL, ",");
        b_text = strtok(NULL, ",");
        c_text = strtok(NULL, ",");
        d_text = strtok(NULL, ",");
        q_ans_str = strtok(NULL, ",");

        if (q_num_str && q_text && a_text && b_text && c_text && d_text && q_ans_str)
        {
            // 정수로 변환하여 구조체에 저장
            questions[question_count].q_num = atoi(q_num_str);
            strncpy(questions[question_count].q_text, q_text, sizeof(questions[question_count].q_text));
            strncpy(questions[question_count].a_text, a_text, sizeof(questions[question_count].a_text));
            strncpy(questions[question_count].b_text, b_text, sizeof(questions[question_count].b_text));
            strncpy(questions[question_count].c_text, c_text, sizeof(questions[question_count].c_text));
            strncpy(questions[question_count].d_text, d_text, sizeof(questions[question_count].d_text));
            questions[question_count].q_ans = q_ans_str[0]; // 문자열에서 첫 번째 문자만 저장

            question_count++;
        }
    }

    fclose(file);

    return 0;
}

// 메시지 수신 스레드 함수
void *recv_msg(void *arg)
{
    srand(time(NULL));

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
                tmp = strtok(NULL, " ");

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
            else if (!strcmp(tmp, "RESULT"))
            {
                tmp = strtok(NULL, " ");

                if (strcmp(tmp, user.name))
                {
                    tmp = strtok(NULL, " ");
                    rival_user.score = atol(tmp);

                    if (user.score <= rival_user.score)
                    {
                        user.my_win = true;
                    }
                    else
                    {
                        user.my_win = false;
                    }

                    tmp = strtok(NULL, " ");
                    rival_user.is_end = true;
                }
            }
        }
    }
}

// 키보드 제어
void *kb_handling(void *arg)
{
    int sock = *((int *)arg);

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
                q_load(40, "Q_Beginner.CSV");
                break;

            case '2':
                strcpy(user.difficulty, "INTERMEDIATE");
                q_load(40, "Q_Intermediate.CSV");
                break;

            case '3':
                strcpy(user.difficulty, "EXPERT");
                q_load(40, "Q_Expert.CSV");
                break;

            case ESC:
                exit(0);
                break;

            default:
                break;
            }

            for (int i = 0; i < 10; i++)
            {
                q_index[i] = (rand() % 40) + 1;

                for (int j = 0; j < i; j++)
                {
                    if (q_index[i] == q_index[j])
                        i--;
                }
            }

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
                if (questions[q_index[q_count]].q_ans == 'A')
                {
                    user.score++;
                }
                break;

            case '2':
                if (questions[q_index[q_count]].q_ans == 'B')
                {
                    user.score++;
                }
                break;

            case '3':
                if (questions[q_index[q_count]].q_ans == 'C')
                {
                    user.score++;
                }
                break;

            case '4':
                if (questions[q_index[q_count]].q_ans == 'D')
                {
                    user.score++;
                }
                break;

            case ESC:
                exit(0);
                break;

            default:
                break;
            }

            q_count++;

            if (q_count > 10)
            {
                user.is_end = true;

                sprintf(msg, "RESULT %s %d %d", user.name, user.score, user.is_end);
                write(sock, msg, strlen(msg));
                memset(msg, 0, sizeof(msg));

                q_count = 0;

                current_ui = RESULT_UI;

                kb_value = '\0';
            }
        }
        // 결과 UI 키 입력 제어
        if (current_ui == RESULT_UI)
        {
            switch (kb_value)
            {
            case '1':
                memset(q_index, 0, sizeof(q_index));
                user.score = 0;
                user.is_ready = false;
                user.is_end = false;
                rival_user.score = 0;
                rival_user.is_ready = false;
                rival_user.is_end = false;
                memset(rival_user.name, 0, sizeof(rival_user.name));

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

        sleep(1);

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
    // 게임 소개 윈도우
    WINDOW *introdution_window = newwin(15, 81, 0, 0);
    box(introdution_window, '|', '-');

    // 게임 소개 출력
    mvwprintw(introdution_window, 5, center_alignment("Project: Secureize", 81), "Project: Secureize");
    mvwprintw(introdution_window, 7, center_alignment("Advanced System Sercurity Quiz GAME!!!", 81), "Advanced System Sercurity Quiz GAME!!!");
    mvwprintw(introdution_window, 10, center_alignment("Programmed By. INHA TECHNICAL COLLEGE", 81), "Programmed By. INHA TECHNICAL COLLEGE"); 
    mvwprintw(introdution_window, 11, center_alignment("DEPARTMENT OF COMPUTER SYSTEMS 2023", 81), "DEPARTMENT OF COMPUTER SYSTEMS 2023");
    mvwprintw(introdution_window, 12, center_alignment("2-A HONG SEOK MIN", 81), "2-A HONG SEOK MIN");  

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
    wrefresh(introdution_window);
    wrefresh(beginner_window);
    wrefresh(intermediate_window);
    wrefresh(expert_window);
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
    // 퀴즈 문제 윈도우
    WINDOW *question_window = newwin(15, 82, 0, 0);
    box(question_window, '|', '-');

    mvwprintw(question_window, 1, 2, "Level : %s", user.difficulty);
    mvwprintw(question_window, 2, 2, "Score : %d", user.score);
    mvwprintw(question_window, 3, 2, "Quiz No. : %d", q_count);

    // 문제를 임시저장할 문자열
    char question_str[1100];
    strcpy(question_str, questions[q_index[q_count]].q_text);
    mvwprintw(question_window, 7, 2, "%s", question_str);

    // 선택지 1번 윈도우
    WINDOW *select1_window = newwin(5, 41, 15, 0);
    box(select1_window, '|', '-');

    mvwprintw(select1_window, 1, 1, "[1]");
    char select1_str[300];
    sprintf(select1_str, "A. %s", questions[q_index[q_count]].a_text);
    mvwprintw(select1_window, 2, 2, "%s", select1_str);

    // 선택지 2번 윈도우
    WINDOW *select2_window = newwin(5, 41, 15, 41);
    box(select2_window, '|', '-');

    mvwprintw(select2_window, 1, 1, "[2]");
    char select2_str[300];
    sprintf(select2_str, "B. %s", questions[q_index[q_count]].b_text);
    mvwprintw(select2_window, 2, 2, "%s", select2_str);

    // 선택지 3번 윈도우
    WINDOW *select3_window = newwin(5, 41, 20, 0);
    box(select3_window, '|', '-');

    mvwprintw(select3_window, 1, 1, "[3]");
    char select3_str[300];
    sprintf(select3_str, "C. %s", questions[q_index[q_count]].c_text);
    mvwprintw(select3_window, 2, 2, "%s", select3_str);

    // 선택지 4번 윈도우
    WINDOW *select4_window = newwin(5, 41, 20, 41);
    box(select4_window, '|', '-');

    mvwprintw(select4_window, 1, 1, "[4]");
    char select4_str[300];
    sprintf(select4_str, "D. %s", questions[q_index[q_count]].c_text);
    mvwprintw(select4_window, 2, 2, "%s", select4_str);

    // 화면 새로 고침
    wrefresh(question_window);
    wrefresh(select1_window);
    wrefresh(select2_window);
    wrefresh(select3_window);
    wrefresh(select4_window);
}

// 결과 UI
void result_ui()
{
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

    mvwprintw(result_window, 17, 2, "Difficulty : %s", user.difficulty);
    mvwprintw(result_window, 19, 2, "Score : %d", user.score);

    if (user.is_end == true && rival_user.is_end == true)
    {
        if (user.score > rival_user.score)
        {
            mvwprintw(result_window, 21, 2, "You Win ! ! !");
        }
        else
        {
            mvwprintw(result_window, 21, 2, "You Defeat...");
        }
    }
    else
    {
        mvwprintw(result_window, 21, 2, "Wait a Minute...");
    }

    mvwprintw(result_window, 23, 2, "Press [1] to Go Main Menu");

    // 화면 새로 고침
    wrefresh(result_window);
}
