#include <stdio.h>
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *request_ip, char *port, char *filename);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);
void header_make(char *method, char *request_ip, char *user_agent_hdr, char *version, int clientfd, char *filename);
void serve_static(int clientfd, int fd);

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

/*doit, 한 개의 HTTP 트랜잭션을 처리한다.
Rio: 강제로 input,output 한다.*/

//GET http://naver.com/index.html:8000 HTTP/1.1

void doit(int fd)// fd = file descriptor = 파일 식별자
{
  int clientfd;
  // MAXLINE : 8192, 2^23, 8kbyte
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char request_ip[MAXLINE], port[MAXLINE], filename[MAXLINE];
  rio_t rio;
  rio_t rio_com;

  Rio_readinitb(&rio, fd); //fd의 주소값을 rio로!
  Rio_readlineb(&rio, buf, MAXLINE);    //한줄을 읽어 오는거 (GET /godzilla.gif HTTP/1.1)

  printf("Request headers: \n");
  printf("%s", buf);

  //자르기 시작
  sscanf(buf, "%s http://%s %s", method, uri, version);

  if (!(strcasecmp(method, "GET") == 0 || strcasecmp(method, "HEAD") == 0))
  {
    clienterror(fd, method, "501", "Not impemented",
                "Tiny does not implement this method");
    return;
  }

  //파싱 시작
  parse_uri(uri, request_ip, port, filename);

  //헤더랑 응답라인
  clientfd = Open_clientfd(request_ip, port);
  header_make(method, request_ip, user_agent_hdr, version, clientfd, filename);

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
  char buf[MAXLINE];
  sprintf(buf, "%s %s %s\r\n", method, filename, "HTTP/1.0");
  sprintf(buf, "%sHost: %s\r\n", buf, request_ip);
  sprintf(buf, "%s%s", buf, user_agent_hdr);
  sprintf(buf, "%sConnection: %s\r\n", buf, "close");
  sprintf(buf, "%sProxy-Connection: %s\r\n\r\n", buf, "close");

  Rio_writen(clientfd, buf, strlen(buf));
}

/*정적 컨텐츠를 클라이언트에게 제공*/
void serve_static(int clientfd, int fd)
{
  int srcfd;
  char *srcp, *p, con_len[MAXLINE], buf[MAXBUF];
  rio_t server_rio;

  Rio_readinitb(&server_rio, clientfd);
  Rio_readlineb(&server_rio, buf, MAXLINE);
  
  while(strcmp(buf, "\r\n"))
  {
    if (strncmp(buf, "Content-length:", 15)==0)
    {
      p = index(buf, 32); //32 아스키 코드 공백
      strcpy(con_len, p+1);
      srcfd = atoi(con_len);
    }

    Rio_writen(fd, buf, strlen(buf));
    Rio_readlineb(&server_rio, buf, MAXLINE);
  }
  Rio_writen(fd, buf, strlen(buf));
  
  /*mmap대신 malloc으로 구현*/
  srcp = malloc(srcfd);
  Rio_readnb(&server_rio, srcp, srcfd);
  Rio_writen(fd, srcp, srcfd); 
  free(srcp);
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
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {            //입력인자
    fprintf(stderr, "usage: %s <port>\n", argv[0]);     //파일이름
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);      //포트파일 

  /*무한서버 루프, 반복적으로 연결 요청을 접수*/
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 연결 요청 접수, 소켓 어드래스(SA) 
    //-> 포트번호는 내가 정하고 있고 사용자가 주소를 치면 받아서 비교 후 억셉트 -> 연결!
    
    //connfd가 뭘까 : 4
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); 
    // 받은걸로 갯네임과 호스트 네임으로 만들어 준다

    // hostname: 143.248.204.8(내아이피) , port: 49981
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    
    doit(connfd);   // 연결 ㄱ
    
    Close(connfd);  // 자신쪽(서버?)의 연결 끝을 닫는다.
  }
}
