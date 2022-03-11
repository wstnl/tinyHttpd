/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
 /* This program compiles for Sparc Solaris 2.6.
  * To compile for Linux:
  *  1) Comment out the #include <pthread.h> line.
  *  2) Comment out the line that defines the variable newthread.
  *  3) Comment out the two lines that run pthread_create().
  *  4) Uncomment the line that runs accept_request().
  *  5) Remove -lsocket from the Makefile.
  */

  /*******************************************************\
   * A frame by by J. David Blackstone.
   * Edited and adding alias by @youzi.
   * https://github.com/wstnl/tinyHttpd
   *
  \*******************************************************/

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h> // define IPv4/IPv6 address
#include <arpa/inet.h> // convert a string(char*) to in_addr(32bit unsigned int)
#include <unistd.h>
#include <ctype.h> // to use function: isspace((int)(x))
#include <strings.h>
#include <string.h>
#include <sys/stat.h> // to check file mode
  // #include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <errno.h>


#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: youzi_httpd/0.1.0\r\n"

#define	TRUE	1
#define	FALSE	0

void accept_request(int);
void bad_request(int);
void cat(int, FILE*);
void cannot_execute(int);
void error_die(const char*);
void execute_cgi(int, const char*, const char*, const char*);
int get_line(int, char*, int);
void headers(int, const char*);
void not_found(int);
void serve_file(int, const char*);
int startup(u_short*);
void unimplemented(int);

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
 /**********************************************************************/
void accept_request(int client)
{
  char buf[1024];
  int numchars; // storage the number of chars in one line
  char method[255];
  char url[255];
  char path[512];
  size_t i, j;
  struct stat st;
  int cgi = 0;      /* becomes true if server decides this is a CGI
                     * program */
  char* query_string = NULL;

  numchars = get_line(client, buf, sizeof(buf));
  i = 0; j = 0;
  while (!ISspace(buf[j]) && (i < sizeof(method) - 1))
  {
    method[i] = buf[j];
    i++; j++;
  }
  method[i] = '\0';

  if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
  { // neither "GET" nor "POST", send 501.
    unimplemented(client);
    return;
  }

  if (strcasecmp(method, "POST") == 0)
    cgi = 1;

  i = 0;
  while (ISspace(buf[j]) && (j < sizeof(buf))) // jump space
    j++;
  while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
  {
    url[i] = buf[j];
    i++; j++;
  }
  url[i] = '\0'; // url, get!

  if (strcasecmp(method, "GET") == 0)
  {
    query_string = url;
    while ((*query_string != '?') && (*query_string != '\0'))
      query_string++;
    if (*query_string == '?')
    {
      cgi = 1;
      *query_string = '\0';
      query_string++;
    }
  }

  sprintf(path, "htdocs%s", url);
  if (path[strlen(path) - 1] == '/')
    strcat(path, "index.html");
  if (stat(path, &st) == -1) { // cannot find file at "path"
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
      numchars = get_line(client, buf, sizeof(buf));
    not_found(client);
  }
  else
  {
    if ((st.st_mode & S_IFMT) == S_IFDIR)
      strcat(path, "/index.html");
    if ((st.st_mode & S_IXUSR) || // check exec permission and open cgi
      (st.st_mode & S_IXGRP) ||
      (st.st_mode & S_IXOTH))
      cgi = 1;
    if (!cgi)
      serve_file(client, path);
    else
      execute_cgi(client, path, method, query_string);
  }

  close(client);
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
 /**********************************************************************/
void bad_request(int client)
{
  char buf[1024];

  sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "Content-type: text/html\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "<P>Your browser sent a bad request, ");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "such as a POST without a Content-Length.\r\n");
  send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
 /**********************************************************************/
void cat(int client, FILE* resource)
{
  char buf[1024];

  fgets(buf, sizeof(buf), resource);
  while (!feof(resource))
  {
    send(client, buf, strlen(buf), 0);
    fgets(buf, sizeof(buf), resource);
  }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
 /**********************************************************************/
void cannot_execute(int client)
{
  char buf[1024];

  sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
  send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
 /**********************************************************************/
void error_die(const char* sc)
{
  perror(sc);
  exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
 /**********************************************************************/
void execute_cgi(int client, const char* path,
  const char* method, const char* query_string)
{
  char buf[1024];
  int cgi_output[2]; // pipe1
  int cgi_input[2]; // pipe2
  pid_t pid;
  int status;
  int i;
  char c;
  int numchars = 1;
  int content_length = -1;

  buf[0] = 'A'; buf[1] = '\0';
  if (strcasecmp(method, "GET") == 0)
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
      numchars = get_line(client, buf, sizeof(buf));
  else    /* POST */
  {
    numchars = get_line(client, buf, sizeof(buf));
    while ((numchars > 0) && strcmp("\n", buf))
    { // read post message and find if it includes "Content-Length:"
      buf[15] = '\0';
      if (strcasecmp(buf, "Content-Length:") == 0)
        content_length = atoi(&(buf[16]));
      numchars = get_line(client, buf, sizeof(buf));
    }
    if (content_length == -1) {
      bad_request(client); // never see a line started with "Content-Length:"
      return;
    }
  }

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  send(client, buf, strlen(buf), 0);

  if (pipe(cgi_output) < 0) {
    cannot_execute(client);
    return;
  }
  if (pipe(cgi_input) < 0) {
    cannot_execute(client);
    return;
  }

  if ((pid = fork()) < 0) { // fork
    cannot_execute(client);
    return;
  }
  if (pid == 0)  /* child: CGI script */
  {
    char meth_env[255];
    char query_env[255];
    char length_env[255];

    dup2(cgi_output[1], STDOUT_FILENO); //redirect output from "cgi_output" to standard output
    dup2(cgi_input[0], STDIN_FILENO);
    close(cgi_output[0]);
    close(cgi_input[1]);
    sprintf(meth_env, "REQUEST_METHOD=%s", method);
    putenv(meth_env);
    if (strcasecmp(method, "GET") == 0) {
      sprintf(query_env, "QUERY_STRING=%s", query_string);
      putenv(query_env);
    }
    else {   /* POST */
      sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
      putenv(length_env);
    }
    execl(path, path, NULL);
    error_die("execl"); // if execl() function returns, there must be something wrong.
  }
  else {    /* parent */
    close(cgi_output[1]);
    close(cgi_input[0]);
    if (strcasecmp(method, "POST") == 0)
      for (i = 0; i < content_length; i++) {
        recv(client, &c, 1, 0); // read post content one by one
        write(cgi_input[1], &c, 1);
      }
    while (read(cgi_output[0], &c, 1) > 0)
      send(client, &c, 1, 0);

    close(cgi_output[0]);
    close(cgi_input[1]);
    waitpid(pid, &status, 0);
  }
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
 /**********************************************************************/
int get_line(int sock, char* buf, int size)
{
  int i = 0;
  char c = '\0';
  int n;

  while ((i < size - 1) && (c != '\n'))
  {
    n = recv(sock, &c, 1, 0); // recive one by one
    /* DEBUG printf("%02X\n", c); */
    if (n > 0)
    {
      if (c == '\r')
      {
        n = recv(sock, &c, 1, MSG_PEEK);
        //Peeks at an incoming message. 
        //The data is treated as unread and the next recv() or similar function shall still return this data.
        /* DEBUG printf("%02X\n", c); */
        if ((n > 0) && (c == '\n'))
          recv(sock, &c, 1, 0);
        else
          c = '\n';
      }
      buf[i] = c;
      i++;
    }
    else
      c = '\n';
  }
  buf[i] = '\0';

  return(i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
 /**********************************************************************/
void headers(int client, const char* filename)
{
  char buf[1024];
  (void)filename;  /* could use filename to determine file type */

  strcpy(buf, "HTTP/1.0 200 OK\r\n");
  send(client, buf, strlen(buf), 0);
  strcpy(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  strcpy(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client)
{
  char buf[1024];

  sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<BODY><H1>404: The server could not fulfill\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "your request because the resource specified\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "is unavailable or nonexistent.\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</BODY></HTML>\r\n");
  send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
 /**********************************************************************/
void serve_file(int client, const char* filename)
{
  FILE* resource = NULL;
  int numchars = 1;
  char buf[1024];

  buf[0] = 'A'; buf[1] = '\0';
  while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
    numchars = get_line(client, buf, sizeof(buf));

  resource = fopen(filename, "r");
  if (resource == NULL)
    not_found(client); // resource not found (404)
  else
  {
    headers(client, filename); // send header
    cat(client, resource); // send body, i.e. file
  }
  fclose(resource);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
 /**********************************************************************/
int startup(u_short* port)
{
  int httpd = 0; // socket FD
  struct sockaddr_in name; // defined in <netinet/in>
  int on = 1;

  httpd = socket(PF_INET, SOCK_STREAM, 0);
  if (httpd == -1)
    error_die("socket");

  // Allow socket descriptor to be reuseable 
  if (setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) < 0)
    error_die("setsockopt");

  /*************************************************************/
  /* Set socket to be nonblocking. All of the sockets for      */
  /* the incoming connections will also be nonblocking since   */
  /* they will inherit that state from the listening socket.   */
  /*************************************************************/
  if (ioctl(httpd, FIONBIO, (char*)&on) < 0)
    error_die("itocl");

  memset(&name, 0, sizeof(name));
  name.sin_family = AF_INET; // IPv4
  name.sin_port = htons(*port); // htons() converts the unsigned integer hostlong from host byte order to network byte order.
  name.sin_addr.s_addr = htonl(INADDR_ANY); // is 0.0.0.0

  if (bind(httpd, (struct sockaddr*)&name, sizeof(name)) < 0)
    error_die("bind");
  if (*port == 0)  /* if dynamically allocating a port */
  {
    int namelen = sizeof(name);
    if (getsockname(httpd, (struct sockaddr*)&name, (socklen_t*)&namelen) == -1)
      error_die("getsockname");
    *port = ntohs(name.sin_port);
  }
  if (listen(httpd, 5) < 0)
    error_die("listen");
  return(httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
 /**********************************************************************/
void unimplemented(int client)
{
  char buf[1024];

  sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</TITLE></HEAD>\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<BODY><H1>501: HTTP request method not supported.\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</BODY></HTML>\r\n");
  send(client, buf, strlen(buf), 0);
}

int main(void)
{
  int server_sock = -1; // listen socket FD
  u_short port = 23333; // set port number, if == 0, dynamically allocating a port.
  // int client_sock = -1; // connection socket FD
  struct sockaddr_in client_name;
  // int client_name_len = sizeof(client_name);
  //  pthread_t newthread;

  struct pollfd	fds[200];
  int	nfds = 1, current_size = 0, i, j, rc;
  int timeout= (3 * 60 * 1000);
  int	new_sd = -1;
  int	end_server = FALSE, compress_array = FALSE;



  server_sock = startup(&port);
  printf("httpd running on port %d\n", port);

  memset(fds, 0, sizeof(fds));

  /*************************************************************/
  /* Set up the initial listening socket                        */
  /*************************************************************/
  fds[0].fd = server_sock;
  fds[0].events = POLLIN;

  do
  {
    printf("Waiting on poll()...\n");
    rc = poll(fds, nfds, timeout);

    if (rc < 0)
    {
      perror("  poll() failed");
      break;
    }

    if (rc == 0)
    {
      printf("  poll() timed out.  End program.\n");
      break;
    }

    current_size = nfds;
    for (i = 0; i < current_size; i++)
    {
      if (fds[i].revents == 0)
        continue;

      if (fds[i].revents != POLLIN)
      {
        printf("  Error! revents = %d\n", fds[i].revents);
        end_server = TRUE;
        break;
      }

      if (fds[i].fd == server_sock)
      {
        printf("  Listening socket is readable\n");
        do
        {
          new_sd = accept(server_sock, NULL, NULL);
          if (new_sd < 0)
          {
            if (errno != EWOULDBLOCK)
            {
              perror("  accept() failed");
              end_server = TRUE;
            }
            break;
          }
          printf("  New incoming connection - %d\n", new_sd);
          fds[nfds].fd = new_sd;
          fds[nfds].events = POLLIN;
          nfds++;
        } while (new_sd != -1);
      }
      else
      {
        printf("  Descriptor %d is readable\n", fds[i].fd);
        // close_conn = FALSE;
        accept_request(fds[i].fd);
        printf("%s is connecting\n", inet_ntoa(client_name.sin_addr)); // from u32 to char*

        close(fds[i].fd);
        fds[i].fd = -1;
        compress_array = TRUE;

      }  /* End of existing connection is readable             */
    } /* End of loop through pollable descriptors              */

    if (compress_array)
    {
      compress_array = FALSE;
      for (i = 0; i < nfds; i++)
      {
        if (fds[i].fd == -1)
        {
          for (j = i; j < nfds; j++)
          {
            fds[j].fd = fds[j + 1].fd;
          }
          nfds--;
        }
      }
    }
  } while (end_server == FALSE); /* End of serving running.    */

  for (i = 0; i < nfds; i++)
  {
    if (fds[i].fd >= 0)
    {
      close(fds[i].fd);
    }
  }

  return(0);
}
