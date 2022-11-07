/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

/*doit, 한 개의 HTTP 트랜잭션을 처리한다.*/
void doit(int fd)// fd = file descriptor = 파일 식별자
{
  int is_static;
  struct stat sbuf;
  /*MAXLINE : 8192, 2^23, 8kbyte*/
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* Read request line and headers*/
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);    // 요청라인을 읽고 분석
  printf("Request headers: \n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET"))        // POST만 받을꺼야 
  {
    clienterror(fd, method, "501", "Not impemented",
                "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);               // POST면 무시

  /*Parse URI from GET request*/
  is_static = parse_uri(uri, filename, cgiargs);    // 정적인지 동적인지(정적이면 참?)
  if (stat(filename, &sbuf) < 0)                    // 디스크에 파일이 없다면 리턴
  {
    clienterror(fd, filename, "404", "Not found",   
                "Tiny dees not implement this method");
    return;
  }

  if (is_static)      //Serve static content
  {
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))//보통 파일이 아니면서 GET 권한을 가지고 있지 않는
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size);     // 다 참이면 정적 주셈
  }
  else                // Serve dynamic content
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))//보통 파일이 아니면서 POST 권한을 가지고 있지 않는
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);       // 다 참이면 동적 주셈
  }

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

/*요청헤더 내 정보를 읽고 무시한다?*/
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n"))
  {
    rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

/*HTTP URI를 분석한다. */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin")) //static content
  {
    strcpy(cgiargs, "");    // cgi인자 스트링을 지우고
    strcpy(filename, ".");  // URIfmf ./index.html 같은 상대 리눅스 경로이름으로 변환
    strcat(filename, uri);  // 까지
    if (uri[strlen(uri)-1] == '/')  //만약에 uri가 /로 끝난다면
      strcat(filename, "home.html");  // 기본 파일 이름을 추가
    return 1;
  }
  else                          //Dynamic content
  {
    ptr = index(uri, '?');    // 동적이라면
    if (ptr)
    {
      strcpy(cgiargs, ptr+1); // CGI 모든 인자를 추출
      *ptr = '\0';            
    }
    else 
      strcpy(cgiargs, "");    // 까지
    
    strcpy(filename, ".");    // 나머지 URI 부분을 상대 리눅스 파일 이름으로 변환
    strcat(filename, uri);    // 까지
    return 0;
  }
}

/*정적 컨텐츠를 클라이언트에게 제공*/
void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /*Send response headers to client*/
  get_filetype(filename, filetype);                         //파일의 접미어 부분을 검사해 파일 타입을 결정
  sprintf(buf, "HTTP/1.0 200 OK\r\n");                      //클라이언트에 응답줄과 응답헤더를 보낸다
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));                         //까지, 
  printf("Response headers: \n");
  printf("%s", buf);                                        //헤더 종료

  /*
  Send response body to client
  요청한 파일의 내용을 연결 식별자 fd로 복사해서 응답 본체를 보낸다.
  */
  srcfd = Open(filename, O_RDONLY, 0);    //읽기위해서 filename을 오픈하고 식별자를 얻어옴
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); //리눅스mmap함수를 요청한 파일을 가상메모리 영역으로 매핑
  // Close(srcfd);                           //파일을 메모리로 맵핑한 후 이 식별자는 필요 없어 닫는다.(메모리 누수 방지)
  // Rio_writen(fd, srcp, filesize);         //실제로 파일을 클라이언트에 전송(주소 srcp에서 시작하는 filesize바이트를 클라이언트의 연결식별자로 복사)
  // Munmap(srcp, filesize);                 //맵핑된 가상 메모리 주소를 반환(메모리 누수 방지)

  /*mmap대신 malloc으로 구현*/
  srcp = (char*)Malloc(filesize);
  Rio_readn(srcfd, srcp, filesize);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize); 
  free(srcp);
}


/*get_filetype - Derive file type from filename*/
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

/*
TINY 자식프로세스를 fork하고, 그 후에 CGI 프로그램을 자식의 컨텍스트에서 실행하며 모든 종류의 동적 컨텐츠를 제공한다.
클라이언트에 성공을 알려주는 응답 라인을 보내는 것으로 시작한다. 
CGI프로그램은 응답의 나머지 부분을 보내야 한다. 
*/
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = { NULL }; //포인터 배열

  /*Return first part of HTTP response(첫번째 응답을 보냄)*/
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0)                      // 새로운 자식을 Fork(고른다?) 한다. 골랐을때 0!
  {
    /*Real server would set all CGI vars here*/
    setenv("QUERY_STRING", cgiargs, 1);   //QUERY_STRING 환경변수를 요청URI의 CGI 인자들로 초기화(다른 CGI환경변수들도 마찬가지로 설정!)
    Dup2(fd, STDOUT_FILENO);              //자식은 자식의 표준 출력을 연결 파일 식별자로 재지정(파일을 복사하는거(앞에꺼를 뒤에로))
    Execve(filename, emptylist, environ); //CGI프로그램을 로드하고 실행
  }
  Wait(NULL); //부모는 자식이 종료되어 정리되는 것을 기다리기 위해 wait함수에서 블록

}



/*Tiny는 반복실행 서버로 명령줄에서 넘겨받은 포트로의 연결 요청을 듣는다.*/
int main(int argc, char **argv) 
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {            //입력인자
    fprintf(stderr, "usage: %s <port>\n", argv[0]);     //파일이름
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);      // 듣기 소켓 오픈!

  /*무한서버 루프, 반복적으로 연결 요청을 접수*/
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 연결 요청 접수 line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // 트랜잭션 수행
    Close(connfd);  // 자신쪽(서버?)의 연결 끝을 닫는다.
  }
}
