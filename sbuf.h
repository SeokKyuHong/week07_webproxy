#include "csapp.h"

typedef struct
{
    int *buf;
    int n;
    int front;  //배열의 첫번째 추적
    int rear;   //배열의 마지막 추적
    sem_t mutex;//뮤텍스 세마포어
    sem_t slots;
    sem_t items;
}sbuf_t;


