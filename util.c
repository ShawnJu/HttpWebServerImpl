/* CSci 4061 F2014 Assignment 5
 * Section 3
 * Date: 12/9/2014
 * Name: Jialun Jiang, Yaozhang Xu, Shang Ju
 * ID:   4467970,      4944346,     4455103
 */

#ifndef _REENTRANT
#define _REENTRANT
#endif

#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>


static int master_fd = -1;
pthread_mutex_t accept_con_mutex = PTHREAD_MUTEX_INITIALIZER;


// this function takes a hostname and returns the IP address
int lookup_host (const char *host)
{
  struct addrinfo hints, *res;
  int errcode;
  char addrstr[100];
  void *ptr;

  memset (&hints, 0, sizeof (hints));
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags |= AI_CANONNAME;

  errcode = getaddrinfo (host, NULL, &hints, &res);
  if (errcode != 0)
    {
      perror ("getaddrinfo");
      return -1;
    }

  printf ("Host: %s\n", host);
  while (res)
    {
      inet_ntop (res->ai_family, res->ai_addr->sa_data, addrstr, 100);

      switch (res->ai_family)
        {
        case AF_INET:
          ptr = &((struct sockaddr_in *) res->ai_addr)->sin_addr;
          break;
        case AF_INET6:
          ptr = &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr;
          break;
        }
      inet_ntop (res->ai_family, ptr, addrstr, 100);
      printf ("IPv%d address: %s (%s)\n", res->ai_family == PF_INET6 ? 6 : 4,
              addrstr, res->ai_canonname);
      res = res->ai_next;
    }

  return 0;
}

int makeargv(const char *s, const char *delimiters, char ***argvp) {
   int error;
   int i;
   int numtokens;
   const char *snew;
   char *t;

   if ((s == NULL) || (delimiters == NULL) || (argvp == NULL)) {
      errno = EINVAL;
      return -1;
   }
   *argvp = NULL;
   snew = s + strspn(s, delimiters);
   if ((t = malloc(strlen(snew) + 1)) == NULL)
      return -1;
   strcpy(t,snew);
   numtokens = 0;
   if (strtok(t, delimiters) != NULL)
      for (numtokens = 1; strtok(NULL, delimiters) != NULL; numtokens++) ;

   if ((*argvp = malloc((numtokens + 1)*sizeof(char *))) == NULL) {
      error = errno;
      free(t);
      errno = error;
      return -1;
   }

   if (numtokens == 0)
      free(t);
   else {
      strcpy(t,snew);
      **argvp = strtok(t,delimiters);
      for (i=1; i<numtokens; i++)
         *((*argvp) +i) = strtok(NULL,delimiters);
   }
   *((*argvp) + numtokens) = NULL;
   return numtokens;
}

void freemakeargv(char **argv) {
   if (argv == NULL)
      return;
   if (*argv != NULL)
      free(*argv);
   free(argv);
}

/**********************************************
 * init
   - port is the number of the port you want the server to be
     started on
   - initializes the connection acception/handling system
   - YOU MUST CALL THIS EXACTLY ONCE (not once per thread,
     but exactly one time, in the main thread of your program)
     BEFORE USING ANY OF THE FUNCTIONS BELOW
   - if init encounters any errors, it will call exit().
************************************************/
enum boolean {FALSE, TRUE};
void init(int port) {

  struct sockaddr_in address;
  int enable = 1;

  if ((master_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    perror("ERROR: Failed to open socket.");
    exit(-1);
  }

  if (setsockopt(master_fd, SOL_SOCKET, SO_REUSEADDR, (char*) &enable, sizeof(int)) == -1) {
    perror("ERROR: Failed to set socket options.");
    exit(-1);
  }

  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = INADDR_ANY;

  if (bind(master_fd, (struct sockaddr*) &address, sizeof(address)) == -1) {
    perror("ERROR: Failed to bind address.");
    exit(-1);
  }

  if(listen(master_fd, 5) == -1) {
    perror("ERROR: Failed to start listening.");
    exit(-1);
  }

}

/**********************************************
 * accept_connection - takes no parameters
   - returns a file descriptor for further request processing.
     DO NOT use the file descriptor on your own -- use
     get_request() instead.
   - if the return value is negative, the thread calling
     accept_connection must exit by calling pthread_exit().
***********************************************/
int accept_connection(void) {

  struct sockaddr address;
  int sSock, addressLen;
  
  addressLen = sizeof(address);
  
  if(pthread_mutex_lock(&accept_con_mutex) != 0) {
    perror("ERROR: Failed to lock mutex.");
    exit(-1);
  }

  sSock = accept(master_fd, (struct sockaddr*) &address, &addressLen);

  if(pthread_mutex_unlock(&accept_con_mutex) != 0) {
    perror("ERROR: Failed to unlock mutex.");
    exit(-1);
  }

  if (sSock == -1) {
    perror("ERROR: Failed to accept connection");
    exit(-1);
  }

  return sSock;

}

/**********************************************
 * get_request
   - parameters:
      - fd is the file descriptor obtained by accept_connection()
        from where you wish to get a request
      - filename is the location of a character buffer in which
        this function should store the requested filename. (Buffer
        should be of size 1024 bytes.)
   - returns 0 on success, nonzero on failure. You must account
     for failures because some connections might send faulty
     requests. This is a recoverable error - you must not exit
     inside the thread that called get_request. After an error, you
     must NOT use a return_request or return_error function for that
     specific 'connection'.
************************************************/
int get_request(int fd, char *filename) {
  FILE* fin;
  size_t length;
  char* fname;
  char** argv;

  if ((fin = fdopen(fd, "r")) == NULL) {
    return -1;
  }

  if (getline(&fname, &length, fin) == -1) {
    return -1;
  }

  if (makeargv(fname, " ", &argv) < 1) {
    return -1;
  }

  strncpy(filename, argv[1], 1024);
  freemakeargv(argv);


  return 0;
}

/**********************************************
 * return_result
   - returns the contents of a file to the requesting client
   - parameters:
      - fd is the file descriptor obtained by accept_connection()
        to where you wish to return the result of a request
      - content_type is a pointer to a string that indicates the
        type of content being returned. possible types include
        "text/html", "text/plain", "image/gif", "image/jpeg" cor-
        responding to .html, .txt, .gif, .jpg files.
      - buf is a pointer to a memory location where the requested
        file has been read into memory (the heap). return_result
        will use this memory location to return the result to the
        user. (remember to use -D_REENTRANT for CFLAGS.) you may
        safely deallocate the memory after the call to
        return_result (if it will not be cached).
      - numbytes is the number of bytes the file takes up in buf
   - returns 0 on success, nonzero on failure.
************************************************/
int return_result(int fd, char *content_type, char *buf, int numbytes) {
  char* result;

  if ((result = (char*)calloc(numbytes + 1000, sizeof(char))) == NULL) {
    return -1;
  }

  sprintf(result, "HTTP/1.1 200 OK\nContent-Type: %s\nContent-Length: %d\nConnection: Close\n\n", content_type, numbytes);

  write(fd, result, strlen(result));
  write(fd, buf, numbytes);
  
  close(fd);
  
  return 0;
  
}


/**********************************************
 * return_error
   - returns an error message in response to a bad request
   - parameters:
      - fd is the file descriptor obtained by accept_connection()
        to where you wish to return the error
      - buf is a pointer to the location of the error text
   - returns 0 on success, nonzero on failure.
************************************************/
int return_error(int fd, char *buf) {
  char* result;
  int numbytes = strlen(buf);

  if ((result = (char*)calloc(numbytes + 1000, sizeof(char))) == NULL) {
    return -1;
  }

  sprintf(result, "HTTP/1.1 404 Not Found\nContent-Type: text/html\nContent-Length: %d\nConnection: Close\n\n", numbytes);
  strncat(result, buf, numbytes);

  write(fd, result, strlen(result));

  return 0;

}

