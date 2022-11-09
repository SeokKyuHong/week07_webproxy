#include "csapp.h"
#include "sbuf.h"

/*
버퍼를 위한 힙 메모리 할당, 
front, rear가 빈 버퍼를 가리키도록 설정
초기값을 위 3개의 세마포어에 할당
*/
void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;
    sp->front = sp->rear = 0;
    Sem_init(&sp->mutex, 0, 1);
    Sem_init(&sp->slots, 0, n);
    Sem_init(&sp->items, 0, 0);
}

/*
어플리케이션이 사용을 다하면 버퍼 반환
*/
void sbuf_deinit(sbuf_t *sp)
{
    Free(sp->buf);
}

/*
1. 가용한 슬롯을 기다리고 뮤텍스를 잠금 
2. 아이템을 추가
3. 뮤텍스를 풂
4. 새로운 아이템 사용을 알림
*/
void sbuf_insert(sbuf_t *sp, int item)
{
    P(&sp->slots);
    P(&sp->mutex);
    sp->buf[(++sp->rear) % (sp->n)] = item;
    V(&sp->mutex);
    V(&sp->items);
}

/*
1. 가용버퍼 아이템을 기다림
2. 뮤텍스를 잠금
3. 아이템을 버퍼의 front에서 제거
4. 뮤텍스 풂
5. 새로운 슬롯 사용을 알림
*/
int sbuf_remove(sbuf_t *sp)
{
    int item;
    P(&sp->items);
    P(&sp->mutex);
    item = sp->buf[(++sp->front) % (sp->n)];
    V(&sp->mutex);
    V(&sp->slots);
    return item;
}
