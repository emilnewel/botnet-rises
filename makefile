compileAll: server.cpp client.cpp
	g++ -Wall -std=c++11 server.cpp -lpthread -o P3_GROUP_2
	g++ -Wall -std=c++11 client.cpp -lpthread -o client
