all: master server

# master related
master: master.o master-socket.o utils.o
	g++ -g -std=c++0x -o master master.o utils.o master-socket.o -pthread

master.o: src/master.cpp inc/master.h inc/constants.h
	g++ -g -std=c++0x -c src/master.cpp -o master.o

master-socket.o: src/master-socket.cpp inc/master.h
	g++ -g -std=c++0x -c src/master-socket.cpp -o master-socket.o

# server related
server: server.o server-socket.o utils.o
	g++ -g -std=c++0x -o server server.o server-socket.o utils.o -pthread

server.o: src/server.cpp inc/server.h inc/constants.h inc/utils.h
	g++ -g -std=c++0x -c src/server.cpp -o server.o

server-socket.o: src/server-socket.cpp inc/server.h inc/constants.h
	g++ -g -std=c++0x -c src/server-socket.cpp -o server-socket.o

# #client related
# client: client.o client-socket.o utils.o
# 	g++ -g -std=c++0x -o client client.o client-socket.o utils.o -pthread

# client.o: client.cpp client.h constants.h utils.h
# 	g++ -g -std=c++0x -c client.cpp

# client-socket.o: client-socket.cpp client.h constants.h
# 	g++ -g -std=c++0x -c client-socket.cpp

#general
utils.o: src/utils.cpp inc/utils.h
	g++ -g -std=c++0x -c src/utils.cpp -o utils.o

clean:
	rm -f *.o master