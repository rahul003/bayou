all: bin/master bin/server

# master related
bin/master: bin/master.o bin/master-socket.o bin/utils.o
	g++ -g -std=c++0x -o bin/master bin/master.o bin/utils.o bin/master-socket.o -pthread

bin/master.o: src/master.cpp inc/master.h inc/constants.h inc/utils.h
	g++ -g -std=c++0x -c src/master.cpp -o bin/master.o

bin/master-socket.o: src/master-socket.cpp inc/master.h inc/constants.h
	g++ -g -std=c++0x -c src/master-socket.cpp -o bin/master-socket.o

# server related
bin/server: bin/server.o bin/server-socket.o bin/utils.o
	g++ -g -std=c++0x -o bin/server bin/server.o bin/server-socket.o bin/utils.o -pthread

bin/server.o: src/server.cpp inc/server.h inc/constants.h inc/utils.h
	g++ -g -std=c++0x -c src/server.cpp -o bin/server.o

bin/server-socket.o: src/server-socket.cpp inc/server.h inc/constants.h
	g++ -g -std=c++0x -c src/server-socket.cpp -o bin/server-socket.o

# #client related
# client: client.o client-socket.o utils.o
# 	g++ -g -std=c++0x -o client client.o client-socket.o utils.o -pthread

# client.o: client.cpp client.h constants.h utils.h
# 	g++ -g -std=c++0x -c client.cpp

# client-socket.o: client-socket.cpp client.h constants.h
# 	g++ -g -std=c++0x -c client-socket.cpp

#general
bin/utils.o: src/utils.cpp inc/utils.h
	g++ -g -std=c++0x -c src/utils.cpp -o bin/utils.o

clean:
	rm -f bin/*