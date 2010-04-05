all: ideirc

ideirc: irc.o ideirc.o identica.o
	g++ -Wall -lcurl -lpthread -o ideirc identica.o ideirc.o irc.o -ggdb3

irc.o: irc.cc irc.h
	g++ -Wall -c -o irc.o irc.cc -ggdb3

ideirc.o: ideirc.cc
	g++ -Wall -c -o ideirc.o ideirc.cc -ggdb3

identica.o: identica.cc identica.h
	g++ -Wall -c -o identica.o identica.cc -ggdb3
clean:
	@rm *.o ideirc
