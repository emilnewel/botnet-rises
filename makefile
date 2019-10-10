compileAll: server.cpp client.cpp
	g++ -Wall -std=c++11 server.cpp -o server
	g++ -Wall -std=c++11 client.cpp -o client
