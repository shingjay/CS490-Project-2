chess.so: chess.cpp runner.c
	g++ -o $@ -Wall -shared -g -O0 -D_GNU_SOURCE -fPIC -ldl $^
	gcc -o runner runner.c
