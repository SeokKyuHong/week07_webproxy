/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  /* $end adder */
  char *buf, *p, *method;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;

  if ((buf = getenv("QUERY_STRING")) != NULL)
  {
    p = strchr(buf, '&');
    *p = '\0';
    strcpy(arg1, buf);
    strcpy(arg2, p+1);

    p = strchr(arg1, '=');
    strcpy(arg1, p+1);
    
    p = strchr(arg2, '=');
    strcpy(arg2, p+1);

    n1 = atoi(arg1);
    n2 = atoi(arg2);

  }

  method = getenv("REQUEST_METHOD");
  
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%s THE Internet addintion portal. \r\n<p>", content);
  sprintf(content, "%s The answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1+n2);
  sprintf(content, "%s Thanks for visiting!\r\n", content);

  printf("connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");  

  /*GET일 때만 Body보냄*/
  if (strcasecmp(method, "GET") == 0)
  {
    printf("%s", content);
  }
  fflush(stdout);
  
  exit(0);
}

