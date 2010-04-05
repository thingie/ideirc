#ifndef _IRC_H
#define _IRC_H

#include <string>
#include <time.h>
#include <queue>
#include <vector>

#include "identica.h"

using namespace std;

enum server_state {DEAD, ANON, LOGEDIN};

class irc_message {
  public:
    irc_message (string msg);
    irc_message (string hostname, string numeric, string user_nick,
  string reply);
    string get_string ();
    void parse ();

    string command;
    string prefix;
    string parameters;

    bool parsed;
    bool valid; //valid syntax, doesn't mean it makes sense
    string message;
};

class irc_connection {
  public:
    irc_connection (int socket);
    //listen on the socket and copy anything that comes in into the buffer
    void do_loop ();
  private:
    //break buffer into single irc messages and put them into a queue
    void parse_buffer ();
    //parse queue and process all the messages there, empty it
    void parse_q ();
    //process a single message
    void process_msg (irc_message& msg);
    int send_msg (irc_message& msg);

    void dispatch_privmsgs (string& addr, string &msg);

    void send_motd ();
    void ping ();

    string user_nick;
    string user_pass;
  
    string server_host;
    string created;

    time_t last_act; //last activity, to watch if the client died

    server_state s_state;
    int irc_socket;
    string buffer;
    queue<irc_message*> msg_q;
};


//irc channel from identica group
class irc_channel {
  public:
    irc_channel (string name);
    void get_new_dents ();
  
  private:
    time_t last_update;
    string address;
};

void* irccon_create (void* irc_socket);
#endif
