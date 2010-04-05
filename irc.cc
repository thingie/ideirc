#include "irc.h"

#include <iostream>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>

using namespace std;

irc_message::irc_message (string msg) {
  message = msg;
  parsed = false;
}

string irc_message::get_string () {
  if (parsed) {
    if (valid) {
      return "PARSED:" + prefix + " " + command + " " + parameters;
    } else {
      return "GARBAGE";
    }
  } else {
    return message;
  }
}

irc_message::irc_message (string hostname, string numeric, string user_nick,
			  string reply) {
  message = ":" + hostname + " " + numeric + " " + user_nick + " " + reply;
}

void irc_message::parse () {
  parsed = true;
  if (message.size () == 0) {
    valid = false;
    return;
  }

  size_t pos = 0;

  prefix = "";
  if (message[0] == ':') {
    pos = message.find (' ');
    if (pos == string::npos) {
      valid = false;
      return;
    }
    prefix = message.substr (1, pos);

    pos = pos + 1;
  }

  command = "";
  //here must be an IRC command -- either a single WORD, or a number
  if (isalpha (message[pos])) {
    //if this byte is an (ASCII) letter, all others before the first space must be
    //too (or until the end of the message)
    command.push_back (toupper (message[pos]));
    pos++;
    while (pos < message.size ()) {
      if (message[pos] == ' ') {
	pos++;
	break;
      }
      if (isalpha (message[pos])) {
	command.push_back (toupper (message[pos]));
      } else {
	valid = false;
	return;
      }
      pos++;
    }
  } else if (isdigit (message[pos])) {
    command.push_back (message[pos]);
    pos++;
    while (pos < message.size ()) {
      if (message[pos] == ' ') {
	pos++;
	break;
      }
      if (isdigit (message[pos])) {
	command.push_back (message[pos]);
      } else {
	valid = false;
	return;
      }
      pos++;
    }
  } else {
    //there is neither letter nor digit
    valid = false;
    return;
  }

  //all the rest should be just a parameter[s]
  parameters = message.substr (pos);
  valid = true;
}

irc_connection::irc_connection (int socket) {
  irc_socket = socket;
  s_state = ANON;
  buffer = "";
  buffer.reserve (512);
  //This is quite a brave assumption that we'll never get an irc message
  //longer than 512 bytes which is something, that RFC 2813 in theory
  //allows us; anyway, we won't be able to handle larger messages

  char hostname [256];
  if (gethostname (hostname, 255) != 0) {
    cerr << "Cannot get hostname " << strerror (errno) << endl;
    return;
  }
  server_host = string (hostname);

  time_t lt;
  time (&lt);
  created = string (ctime (&lt));
}

void irc_connection::do_loop () {
  char int_buf [512];
  time (&last_act);
  struct timeval tv;
  time_t last_ping = 0;
  int rv;
  tv.tv_sec = 30;
  tv.tv_usec = 0;
  fd_set selfds;

  for (;;) {
    if (s_state == DEAD) {
      break;
    }
    //try to fill buffer up to 512 bytes
    //we should spend most of the time here, blocking
    int_buf[0] = NULL;

    FD_ZERO (&selfds);
    FD_SET (irc_socket, &selfds);
    rv = select (irc_socket + 1, &selfds, NULL, NULL, &tv);
    if (rv == -1) {
      cerr << "Connection problem: " << strerror (errno) << endl;
      return;
    }

    //check time
    time_t now;
    time (&now);
    if (now - last_act > 300) {
      s_state = DEAD;
      return;
    }
    if ((now - last_act > 60) && (now - last_ping > 15)) {
      ping ();
      time (&last_ping);
    }
    
    if (rv) {
      if (FD_ISSET (irc_socket, &selfds)) {
	ssize_t readed = read (irc_socket, int_buf, 512 - buffer.size ());
	if (readed == -1) {
	  //TODO this is not enough to break the connection
	  cerr << "Connection problem: " << strerror (errno) << endl;
	  return;
	}
	if (readed == 0) {
	  //connection is over
	  return;
	}
	time (&last_act);
	buffer.append (int_buf, readed);
	
	parse_buffer ();
	parse_q ();
      }
      //... check other fds.
    }
  }
}

void irc_connection::parse_buffer () {
  if (buffer.empty ()) {
    return;
  }

  size_t pos;
  //while ((pos = buffer.find ("\x0D\x0A")) != string::npos) {
  while ((pos = buffer.find ("\x0A")) != string::npos) {
    msg_q.push (new irc_message (buffer.substr (0, pos)));
    string nbuffer = buffer.substr (pos + 1);
    buffer = nbuffer;
  }
}

void irc_connection::parse_q () {
  while (!msg_q.empty ()) {
    irc_message *msg = msg_q.front ();
    msg->parse ();
    cout << msg->get_string () << endl;
    process_msg (*msg);
    msg_q.pop ();
    delete msg;
  }
}

void irc_connection::ping () {
  irc_message p (":" + server_host + " PING 0");
  send_msg (p);
}

int irc_connection::send_msg (irc_message& msg) {
  cout << "SENT: " + msg.message << endl;
  msg.message.append ("\r\n");
  //return write (irc_socket, (char*) msg.message.c_str (), msg.message.size ());
  return send (irc_socket, (char*) msg.message.c_str (), msg.message.size (),
	       0);
}

void irc_connection::process_msg (irc_message& msg) {
  if (msg.command == "NICK") {
    if ((msg.parameters.find (' ') != string::npos) ||
	(msg.parameters.size () > 64)) {
      irc_message invnick (":" + server_host + " 432 :Erroneous nickname");
      send_msg (invnick);
      return;
    }
    if (msg.parameters.empty ()) {
      irc_message err (":" + server_host + " 431 :No nickname given");
      send_msg (err);
      return;
    }
    if (s_state == ANON) {
      user_nick = msg.parameters;
      //user is logging to the server
      irc_message reply (":" + server_host + " 001 " + user_nick +
		     " :Welcome to the identica gate, " + user_nick +
		     "@client");
      send_msg (reply);
      irc_message reply2 (":" + server_host + " 002 " + user_nick +
		    " :Your host is " + server_host + ", running" +
		    " identica gate 0");
      send_msg (reply2);
      irc_message reply3 (":" + server_host + " 003 " + user_nick + 
			  " :This server was created " + 
			  created);
      //TODO put a real date here
      send_msg (reply3);
      irc_message reply4 (":" + server_host + " 004 " + user_nick +
			  " :" + server_host + " identica gate 0");
      send_msg (reply4);

      send_motd ();

      irc_message mode (":" + server_host + " MODE " + user_nick +
			" :+i");
      send_msg (mode);
      
      s_state = LOGEDIN;

      irc_message controljoin (":" + user_nick + "!" + user_nick + "@" +
			       server_host + " JOIN " + "#control");
      send_msg (controljoin);
    }
    if (s_state == LOGEDIN) {
      string new_user_nick = msg.parameters;
      irc_message nick_change (":" + user_nick + "!" + user_nick +
			       "@" + server_host + " NICK " + new_user_nick);
      send_msg (nick_change);
      user_nick = msg.parameters;
      user_pass = "";
    }
    return;
  }
  if (msg.command == "QUIT") {
    s_state = DEAD;
    return;
  }
  if (msg.command == "JOIN") {
    //TODO
    return;
  }
  if (msg.command == "PART") {
    //TODO
    return;
  }
  if (msg.command == "TOPIC") {
    irc_message ntopic (server_host, "331", user_nick, " :No topic is set");
    send_msg (ntopic);
    return;
  }
  if (msg.command == "NAMES") {
    if (msg.parameters.empty () || (msg.parameters.find (',') != string::npos)) {
      irc_message too_mm (server_host, "416", user_nick, " :Too many matches");
      send_msg (too_mm);
      return;
    }
    if (msg.parameters == "#control") {
      irc_message nlist (server_host, "353", user_nick, " = #control :" + 
			   user_nick + " keeper");
      send_msg (nlist);
      irc_message nlist_end (server_host, "366", user_nick, " #control :End "
			     "of NAMES list");
      send_msg (nlist_end);
    }
    return;
  }
  if (msg.command == "PRIVMSG") {
    string address = "";
    string mesg = "";
    size_t pos = 0;
    while (pos < msg.parameters.size ()) {
      if (isalpha (msg.parameters[pos]) || msg.parameters[pos] == '#') {
	address.push_back (msg.parameters[pos]);
	pos++;
      } else {
	return; //ignore...
      }
      if (msg.parameters[pos] == ' ' && msg.parameters[pos+1] == ':') {
	mesg = msg.parameters.substr (pos+2);
	break;
      }
    }
    cout << "Got message for " << address << " saying: " << mesg << endl;
    dispatch_privmsgs (address, mesg);
    return;
  }
  if (msg.command == "USERHOST") {
    if (msg.parameters == user_nick) {
      irc_message ureply (server_host, "302", user_nick, ":" + msg.parameters +
			"=+" + user_nick + "@" + server_host);
      send_msg (ureply);
    }
    return;
  }
  if (msg.command == "MOTD") {
    if (msg.parameters.empty ()) {
      send_motd ();
    }
    return;
  }
  if (msg.command == "WHO") {
    if (msg.parameters == user_nick) {
      irc_message who_reply (server_host, "352", user_nick, "#control ~" 
			     + user_nick + " " + server_host + " localhost " +
			     user_nick + " H 0 " + user_nick);
      send_msg (who_reply);
      irc_message who_end (server_host, "315", user_nick, user_nick +
			   " :End of WHO list");
      send_msg (who_end);
    } else {
      irc_message nosuchs (server_host, "402", user_nick, msg.parameters + 
			   " :No such server");
      send_msg (nosuchs);
    }
    return;
  }
  if (msg.command == "PING") {
    irc_message pong (":" + server_host + " PONG " + server_host);
    send_msg (pong);
    return;
  }
  if (msg.command == "MODE") {
    string channel_q;
    size_t pos;
    if ((pos = msg.parameters.find (' ')) == string::npos) {
      channel_q = msg.parameters;
    } else {
      channel_q = msg.parameters.substr (0, pos);
    }

    irc_message no_modes (":" + server_host + " 324 " + channel_q);
    send_msg (no_modes);
    return;
  }

if (msg.command == "PONG") {
  return;
 }

  //if this command is not implemented, send at least something
  irc_message default_reply (":" + server_host + " 421 " + msg.command +
			     " :Unknown command");
  send_msg (default_reply);
}

void irc_connection::dispatch_privmsgs (string& addr, string& msg) {
  //special cases first (everything within #control)
  if (addr == "#control") {
    size_t pos;
    string command;
    string parm;
    if ((pos = msg.find (' ')) != string::npos) {
      command = msg.substr (0, pos);
      parm = msg.substr (pos + 1);
    } else {
      command = msg;
  }

    //check commands
    
    if (command == "password") {
      if (parm.empty ()) {
	irc_message no_p (":keeper!keeper@" + server_host + " PRIVMSG #control"
			  " :Please, enter your password.");
	send_msg (no_p);
	return;
      } else {
	user_pass = parm;
	cout << "User's password is " << user_pass << endl;
	irc_message p (":keeper!keeper@" + server_host + " PRIVMSG #control"
			  " :Password recorded.");
	send_msg (p);
      }
      return;
    }
    if (command == "post") {
      if (parm.empty ()) {
	irc_message noth (":keeper!keeper@" + server_host + " PRIVMSG #control"
			  " :Nothing to post.");
	send_msg (noth);
	return;
      }
      if (parm.size () > 560) {
	//We're not checking if there are less than 140 letters
	//because we know don't want to bother with multibyte chars
	//560 is 140-times 4 bytes, so we're doing at least some check,
	//even though pretty useless, as 560 is much larger than possible
	//irc message with our "server"...
	irc_message noth (":keeper!keeper@" + server_host + " PRIVMSG #control"
			  " :Message too long.");
	send_msg (noth);
	return;
      }
      if (user_pass.empty ()) {
	irc_message noth (":keeper!keeper@" + server_host + " PRIVMSG #control"
			  " :You have to enter your password in order to post.");
	send_msg (noth);
	return;
      }
      if (identica_send (user_nick, user_pass, parm) != 0) {
	irc_message noth (":keeper!keeper@" + server_host + " PRIVMSG #control"
			  " :Failed to send the message.");
	send_msg (noth);
	return;
      }
      return;
    }
    //no known command given
    irc_message p (":keeper!keeper@" + server_host + " PRIVMSG #control"
		   " :Unknown command.");
    send_msg (p);
  }
}

void irc_connection::send_motd () {
  irc_message motd_s (":" + server_host + " 375 " + user_nick + " :- " 
		    + server_host + " Message of the day - ");
  send_msg (motd_s);
  irc_message motd (server_host, "372", user_nick, ":- For more info, see $URL");
  send_msg (motd);
  irc_message motd_e (server_host, "376", user_nick, ":End of MOTD command");
  send_msg (motd_e);
}

void* irccon_create (void* irc_socket) {
  //irc_socket is an IP socket in ready state after accept
  irc_connection* server = new irc_connection ((int) irc_socket);
  server->do_loop ();

  delete server;

  shutdown ((int) irc_socket, SHUT_RDWR);
  close ((int) irc_socket);

  pthread_exit ((void*) 0);
  return (void*) 0;
}
