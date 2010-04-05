#include "identica.h"

int identica_send (string username, string password, string message) {
 CURL *curl;
 CURLcode res;
 
 curl = curl_easy_init ();
 if (curl) {
   curl_easy_setopt (curl, CURLOPT_URL, "identi.ca/api/statuses/update.xml");
   curl_easy_setopt (curl, CURLOPT_USERNAME, username.c_str ());
   curl_easy_setopt (curl, CURLOPT_PASSWORD, password.c_str ());
   
   struct curl_httppost* post = NULL;
   struct curl_httppost* last = NULL;

   curl_formadd (&post, &last, 
		 CURLFORM_COPYNAME, "status",
		 CURLFORM_PTRCONTENTS, message.c_str (),
		 CURLFORM_CONTENTTYPE, "text/plain", CURLFORM_END);

   curl_easy_setopt (curl, CURLOPT_HTTPPOST, post);
   res = curl_easy_perform (curl);
   
   curl_easy_cleanup (curl);
   curl_formfree (post);
   
   //TODO, we need to check at least if we were able to post sucessfully
 }
 return (int) res;
}
