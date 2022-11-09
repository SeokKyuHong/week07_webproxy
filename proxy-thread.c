#include <stdio.h>
#include "csapp.h"
#include "sbuf.h"
/*
사전쓰레드된 동시성 서버구현
 - 한 개의 생산자와 다수의 소비자를 갖는 생산자-소비자

*/
void *thread(void *vargp);
int sbuf_remove(sbuf_t *sp);
void sbuf_init(sbuf_t *sp, int n);

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *request_ip, char *port, char *filename);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);
void header_make(char *method, char *request_ip, char *user_agent_hdr, char *version, int clientfd, char *filename);
void serve_static(int clientfd, int fd);
sbuf_t sbuf;

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define NTHREADS 4
#define SBUFSIZE 16

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

/*doit, 한 개의 HTTP 트랜잭션을 처리한다.*/
void doit(int fd)// fd = file descriptor = 파일 식별자(connfd 받아온거임)
{
  int clientfd;
  // MAXLINE : 8192, 2^23, 8kbyte
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char request_ip[MAXLINE], port[MAXLINE], filename[MAXLINE];
  rio_t rio;
  rio_t rio_com;

  Rio_readinitb(&rio, fd);            //fd의 주소값을 rio로!
  Rio_readlineb(&rio, buf, MAXLINE);  //한줄을 읽어 오는거 (GET /godzilla.gif HTTP/1.1)

  printf("Request headers: \n");
  printf("%s", buf);

  //자르기 시작
  sscanf(buf, "%s http://%s %s", method, uri, version);

  //이상한 요청 거르기
  if (!(strcasecmp(method, "GET") == 0 || strcasecmp(method, "HEAD") == 0))
  {
    clienterror(fd, method, "501", "Not impemented",
                "Tiny does not implement this method");
    return;
  }

  //파싱 시작
  parse_uri(uri, request_ip, port, filename);
  //파싱 완료

  //헤더랑 응답라인
  clientfd = Open_clientfd(request_ip, port);
  header_make(method, request_ip, user_agent_hdr, version, clientfd, filename);
  //헤더 만들고 tiny로 보냈음

  //서버에서 왔음
  serve_static(clientfd, fd);
  Close(clientfd);
}

/*클라이언트에서 온 헤더 첫 줄 파싱*/
/*
URI 파싱 조건필요한거 : 
1. method : GET (이미 있음)
2. request_ip : naver.com/ (:// 뒤에부터 다음 / 나올때까지)
3. filename : index.html (request_ip 포인터 다음 부터 : 나올때까지)
4. port : 8000 (filename 포인터 부터 공백까지)
5. version : HTTP/1.0 (마지막 글자 0으로 수정 필요)
6. 포트가 없을 수도 있음
7. 파일이 없어서 포트 뒤에 '/'가 없을 수도 있음
*/
int parse_uri(char *uri, char *request_ip, char *port, char *filename)
{
  char *ptr;

  ptr = strchr(uri, 58); // ':'아스키 코드
  if(ptr != NULL) //port가 있을때
  {
    *ptr = '\0';
    strcpy(request_ip, uri);// : 앞에 ip 가져온다.
    strcpy(port, ptr+1); // : 뒤로 다 가져온다 8000/index.html

    ptr = strchr(port, 47); // '/'아스키 코드
    if (ptr != NULL){ // '/' 있다면
      strcpy(filename, ptr);
      *ptr = '\0';
      strcpy(port, port);
    }else{ // '/' 없다면
      strcpy(port, port);
    }
  }
  else //port가 없을때
  { 
    strcpy(request_ip, uri);

    ptr = strchr(request_ip, 47);
    strcpy(filename, ptr+1);
    port = "80";
  }
}

void header_make(char *method, char *request_ip, char *user_agent_hdr, char *version, int clientfd, char *filename)
{
  char buf[MAXLINE]; //임시 버프

  /*자른 데이터를 헤더로 만들어 buf로 tiny에게 전달*/
  //앞줄을 하나씩 물고와요 한다.
  sprintf(buf, "%s %s %s\r\n", method, filename, "HTTP/1.0");
  sprintf(buf, "%sHost: %s\r\n", buf, request_ip);
  sprintf(buf, "%s%s", buf, user_agent_hdr);
  sprintf(buf, "%sConnection: %s\r\n", buf, "close");
  sprintf(buf, "%sProxy-Connection: %s\r\n\r\n", buf, "close");

  //서버로 보낸다. 
  Rio_writen(clientfd, buf, strlen(buf));
}

/*정적 컨텐츠를 클라이언트에게 제공*/
void serve_static(int clientfd, int fd)
{
  int src_size;
  char *srcp, *p, content_length[MAXLINE], buf[MAXBUF];
  rio_t server_rio;

  Rio_readinitb(&server_rio, clientfd);         //rio_t의 읽기 버퍼와 fd를 연결

  Rio_readlineb(&server_rio, buf, MAXLINE);     //rio_t에서 읽어서 버퍼에 저장(한줄씩 >_-)
  Rio_writen(fd, buf, strlen(buf));             //fd에 써서 보내준다.(클라이언트로)

  while(strcmp(buf, "\r\n"))                    //우리의 헤더는 가장 끝에 빈줄이 있으므로 그걸 만날때까지 읽어보자
  {                                             
    if (strncmp(buf, "Content-length:", 15)==0) //strncmp함수는 두 문자열의 길이를 비교한다. 참이면 같다!
    {
      p = index(buf, 32);                       //32은 아스키 코드 공백, 공백의 포인터 할당
      strcpy(content_length, p+1);              //공백뒤에 있는 '텍스트'를 임시 변수에 할당
      src_size = atoi(content_length);          //정수로 바꾸어 src_size에 사이즈 저장
    }
    
    Rio_readlineb(&server_rio, buf, MAXLINE);   //읽으면서 보내(클라이언트로
    Rio_writen(fd, buf, strlen(buf));
  }
  /*헤더끝*/
  
  /*body 보내기 시작*/
  /*mmap대신 malloc으로 구현*/
  srcp = malloc(src_size);                  //바디 사이즈 할당
  Rio_readnb(&server_rio, srcp, src_size);  //읽고
  Rio_writen(fd, srcp, src_size);           //보내
  free(srcp);                               //malloc 프리
}

/*에러 처리 함수*/
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  //MAXBUF : 8192
  char buf[MAXLINE], body[MAXBUF];

  /*Build the HTTP response body*/
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n" , body); 
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause); 
  sprintf(body, "%s<hr><em>The tiny Web server</em>\r\n", body);

  /*Print the HTTP response*/
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body)); 
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

/*argc: 함수의 전달된 인자의 개수, argv: 가변적인 개수의 문자열*/
int main(int argc, char **argv) {
  int listenfd, connfd;                   // listenfd: 프록시 듣기식별자, connfd: 프록시연결 식별자
  char hostname[MAXLINE], port[MAXLINE];  // 클라이언트에게 받은 uil 정보를 담음 공간
  socklen_t clientlen;                    // 소켓 길이를 저장할 구조체
  struct sockaddr_storage clientaddr;     // 소켓 구조체(clientaddress 들어감)
  pthread_t tid;

  if (argc != 2) {            //입력인자가 2개인지 확인
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(0);
  }
  listenfd = Open_listenfd(argv[1]);  // listen 소켓 오픈 

  /*스레드*/
  sbuf_init(&sbuf, SBUFSIZE);                 //버퍼 생성
  for (int i = 0; i < NTHREADS; i++)          //
  {
    Pthread_create(&tid, NULL, thread, NULL); //호출되는 프로세스에서 새로운 쓰레드 시작
  }

  /*무한서버 루프, 반복적으로 연결 요청을 접수*/
  while (1) {
    clientlen = sizeof(struct sockaddr_storage);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 연결 요청 접수, 소켓 어드래스(SA) 
    //-> 포트번호는 내가 정하고 있고 사용자가 주소를 치면 받아서 비교 후 트억셉 -> 연결!
    sbuf_insert(&sbuf, connfd);
  }
}

/*thread 함수*/
void *thread(void *vargp)
{
  //Pthread_detach 함수는 pthread_self가 종료되면 쓰레드를 반환 하거나 분리해야함
  //pthread_self는 스레드의 아이디를 반환해줌
  Pthread_detach(pthread_self());   
  while (1)
  {
    int connfd = sbuf_remove(&sbuf);  //가용아이템을 받으면 connfd로 연결
    doit(connfd);
    Close(connfd);
  }

}