all: mylib.so server

mylib.o: mylib.c
	gcc -Wall -I../include -L../lib -l dirtree -fPIC -DPIC -c mylib.c

mylib.so: mylib.o
	ld -shared -o mylib.so mylib.o -ldl

server: 
	gcc -Wall -I../include server.c -o server -fPIC -DPIC -L../lib -l dirtree

clean:
	rm -f *.o *.so
	rm server

