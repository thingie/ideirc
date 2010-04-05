#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>

#include <iostream>

#include "irc.h"

using namespace std;

//Signal handler for SIGTERM as an expected way to stop the server
void handle_term (int signum) {
  exit (0);
}

int main (int argc, char** argv) {
  uint16_t port = 6666;

  //connect signal handler for SIGTERM
  struct sigaction termact;
  termact.sa_handler = handle_term;
  sigaction (15, &termact, NULL);

  int irc_sock = socket (AF_INET, SOCK_STREAM, 0);
  if (irc_sock < 0) {
    cerr << "Cannot prepare IP socket, reason: " << strerror (errno);
    return -1;
  }

  struct sockaddr_in irc_addr;
  irc_addr.sin_family = AF_INET;
  irc_addr.sin_port = htons (port);
  irc_addr.sin_addr.s_addr = htonl (INADDR_ANY);

  if (bind (irc_sock, (sockaddr*) &irc_addr, sizeof (sockaddr_in)) != 0) {
    cerr << "Couldn't bind socket for IRC, reason: " << strerror (errno)
      << endl;
    return -1;
  }

  if (listen (irc_sock, 100) != 0) {
    cerr << "Cannot listen on the socket, reason: " << strerror (errno)
      << endl;
    return -1;
  }

  //listen forever for a new connections, launch new thread for each
  for (;;) {
    int connection;
    connection = accept (irc_sock, NULL, NULL); //irc_sock should be blocking
    if (connection >= 0) {
      pthread_t irct;
      pthread_create (&irct, NULL, irccon_create, (void*) connection);
      //do create a new thread
    }
    if (connection < 0) {
      cerr << "Error while trying to accept a connection: " << strerror (errno)
	<< endl;
    }
  }

  return 0;
}
