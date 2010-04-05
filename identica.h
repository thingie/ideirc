#ifndef _IDENTICA_H
#define _IDENTICA_H

#include <string>
#include <curl/curl.h>
using namespace std;

class identica_user {
 public:
  string get_name ();

 private:
  string username;
};

int identica_send (string username, string password, string message);

#endif
