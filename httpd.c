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
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros 


#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: youzi_httpd/0.1.0\r\n"

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
  int opt = 1;
  struct sockaddr_in name; // defined in <netinet/in>

  httpd = socket(PF_INET, SOCK_STREAM, 0);
  if (httpd == -1)
    error_die("socket");

  //set master socket to allow multiple connections , this is just a good habit, it will work without this 
  if (setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt,
    sizeof(opt)) < 0)
  {
    error_die("setsockopt");
  }


  memset(&name, 0, sizeof(name));
  name.sin_family = AF_INET; // IPv4
  name.sin_port = htons(*port); // htons() converts the unsigned integer hostlong from host byte order to network byte order.
  // name.sin_port = 6666;

  name.sin_addr.s_addr = htonl(INADDR_ANY); // is 0.0.0.0
  if (bind(httpd, (struct sockaddr*)&name, sizeof(name)) < 0)
    error_die("bind");
  if (*port == 0)  /* if dynamically allocating a port */
  {
    int namelen = sizeof(name);
    if (getsockname(httpd, (struct sockaddr*)&name, &namelen) == -1)
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

/**********************************************************************/

int main(void)
{
  int server_master = -1; // listen socket FD
  u_short port = 23333; // set port number, if == 0, dynamically allocating a port.
  // int client_sock = -1; // connection socket FD
  struct sockaddr_in client_name;
  int client_name_len = sizeof(client_name);
  //  pthread_t newthread;

  int new_socket, activity, valread, sd;
  //set of socket descriptors 
  fd_set readfds;
  int max_sd;
  const int max_clients = 20;
  int client_socket[max_clients];
  char buffer[1025];  //data buffer of 1K // ready to del

  //initialise all client_socket[] to 0 so not checked 
  for (int i = 0; i < max_clients; i++)
  {
    client_socket[i] = 0;
  }
  
  server_master = startup(&port);
  printf("httpd running on port %d, waiting for connections...\n", port);


  while (1)
  {
    //clear the socket set 
    FD_ZERO(&readfds);

    //add master socket to set 
    FD_SET(server_master, &readfds);
    max_sd = server_master;
    //add child sockets to set 
    for (int i = 0; i < max_clients; i++)
    {
      //socket descriptor 
      sd = client_socket[i];

      //if valid socket descriptor then add to read list 
      if (sd > 0)
        FD_SET(sd, &readfds);

      //highest file descriptor number, need it for the select function 
      if (sd > max_sd)
        max_sd = sd;
    }
    //wait for an activity on one of the sockets , timeout is NULL , 
    //so wait indefinitely 
    activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

    if (activity < 0)
    {
      error_die("select error");
    }

    //If something happened on the master socket , 
    //then its an incoming connection 
    if (FD_ISSET(server_master, &readfds))
    {
      if ((new_socket = accept(server_master,
        (struct sockaddr*)&client_name, (socklen_t*)&client_name_len)) < 0)
      {
        perror("accept");
        exit(EXIT_FAILURE);
      }

      //inform user of socket number - used in send and receive commands 
      printf("New connection , socket fd is %d , ip is : %s , port : %d \n",
        new_socket, inet_ntoa(client_name.sin_addr), ntohs(client_name.sin_port));

      //send new connection greeting message 
      // if (send(new_socket, message, strlen(message), 0) != strlen(message))
      // {
      //   perror("send");
      // }
      if (new_socket == -1)
        error_die("cannot accept");

      accept_request(new_socket);

      //add new socket to array of sockets 
      for (int i = 0; i < max_clients; i++)
      {
        //if position is empty 
        if (client_socket[i] == 0)
        {
          client_socket[i] = new_socket;
          printf("Adding to list of sockets as %d\n", i);

          break;
        }
      }
    }

    //else its some IO operation on some other socket
    for (int i = 0; i < max_clients; i++)
    {
      sd = client_socket[i];

      if (FD_ISSET(sd, &readfds))
      {
        //Check if it was for closing , and also read the 
        //incoming message 
        if ((valread = read(sd, buffer, 1024)) == 0)
        {
          //Somebody disconnected , get his details and print 
          getpeername(sd, (struct sockaddr*)&client_name, \
            (socklen_t*)&client_name_len);
          printf("Host disconnected , ip %s , port %d \n",
            inet_ntoa(client_name.sin_addr), ntohs(client_name.sin_port));

          //Close the socket and mark as 0 in list for reuse 
          close(sd);
          client_socket[i] = 0;
        }

        //Echo back the message that came in 
        else
        {
          //set the string terminating NULL byte on the end 
          //of the data read 
          buffer[valread] = '\0';
          send(sd, buffer, strlen(buffer), 0);
        }
      }
    }

    /*** OLD CODES
    client_sock = accept(server_master,
      (struct sockaddr*)&client_name,
      &client_name_len);
    if (client_sock == -1)
      error_die("cannot accept");

    accept_request(client_sock);
    printf("%s is connecting\n", inet_ntoa(client_name.sin_addr)); // from u32 to char*
    ***/
    //  if (pthread_create(&newthread , NULL, accept_request, client_sock) != 0)
    //    perror("pthread_create");
  }

  close(server_master);

  return(0);
}
