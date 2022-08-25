#Butilca Rares

SERVER_PORT=5555
SUB_ID="C1"
SUB_IP=127.0.0.1
SUB_PORT=7777

build: server subscriber

server: server.cpp client_server.cpp client_server.h
	g++ server.cpp client_server.cpp -o server

subscriber: subscriber.cpp client_server.cpp client_server.h
	g++ subscriber.cpp client_server.cpp -o subscriber

run-server:
	./server SERVER_PORT

run-client:
	./subscriber SUB_USER SUB_IP SUB_PORT

clean:
	rm subscriber server