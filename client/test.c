#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ncurses.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

void main()
{
    srand(time(NULL));

    int q_index[10] = {0};

    for (int i = 0; i < 10; i++)
    {
        q_index[i] = (rand() % 40) + 1;

        for (int j = 0; j < i; j++)
        {
            if (q_index[i] == q_index[j])
                i--;
        }
    }

    for(int i = 0 ; i < 10 ; i++) 
    {
        printf("[%d] %d\n", i, q_index[i]);
    }
}