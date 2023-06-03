all:main 
main:main.cpp proxy.hpp http.hpp
	g++ -ggdb3 -std=c++17 -o main main.cpp proxy.hpp http.hpp -lpthread
#server:server.cpp
#	g++ -ggdb3 -std=c++17 -I /usr/local/boost_1_81_0 -L/usr/local/boost_1_81_0/stage/lib/ -o server server.cpp -lpthread

clean:
	rm main
servertest.cpp:http.hpp server.hpp 