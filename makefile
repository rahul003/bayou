all: bin/master bin/server bin/client

# master related
bin/master: bin/master.o bin/master-socket.o bin/utils.o bin/data_classes.o
	g++ -g -std=c++0x -o bin/master bin/master.o bin/utils.o bin/master-socket.o bin/data_classes.o -pthread

bin/master.o: src/master.cpp inc/master.h inc/constants.h inc/utils.h
	g++ -g -std=c++0x -c src/master.cpp -o bin/master.o

bin/master-socket.o: src/master-socket.cpp inc/master.h inc/constants.h
	g++ -g -std=c++0x -c src/master-socket.cpp -o bin/master-socket.o

# server related
bin/server: bin/server.o bin/server-socket.o bin/utils.o bin/data_classes.o
	g++ -g -std=c++0x -o bin/server bin/server.o bin/server-socket.o bin/utils.o bin/data_classes.o -pthread

bin/server.o: src/server.cpp inc/server.h inc/constants.h inc/utils.h inc/data_classes.h
	g++ -g -std=c++0x -c src/server.cpp -o bin/server.o

bin/server-socket.o: src/server-socket.cpp inc/server.h inc/constants.h
	g++ -g -std=c++0x -c src/server-socket.cpp -o bin/server-socket.o

#client related
bin/client: bin/client.o bin/client-socket.o bin/utils.o
	g++ -g -std=c++0x -o bin/client bin/client.o bin/client-socket.o bin/utils.o bin/data_classes.o -pthread

bin/client.o: src/client.cpp inc/client.h inc/constants.h inc/utils.h
	g++ -g -std=c++0x -c src/client.cpp -o bin/client.o

bin/client-socket.o: src/client-socket.cpp inc/client.h inc/constants.h
	g++ -g -std=c++0x -c src/client-socket.cpp -o bin/client-socket.o

#general
bin/utils.o: src/utils.cpp inc/utils.h inc/constants.h inc/data_classes.h
	g++ -g -std=c++0x -c src/utils.cpp -o bin/utils.o

bin/data_classes.o: src/data_classes.cpp inc/data_classes.h inc/constants.h
	g++ -g -std=c++0x -c src/data_classes.cpp -o bin/data_classes.o

clean:
	rm -f bin/*