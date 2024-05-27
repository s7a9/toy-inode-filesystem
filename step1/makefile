all: server client_cmd clean

client_cmd: client_cmd.c
	gcc -o ../bin/BDC_command client_cmd.c ../bin/network.o ../bin/bytepack.o -O2 -Wall

server: server.c disksim.o
	gcc -o ../bin/BDS -I.. server.c ../bin/network.o disksim.o ../bin/bytepack.o -O2 -Wall

disksim.o: disksim.c disksim.h
	gcc -c disksim.c -o disksim.o -O2 -Wall

clean: server
	rm -f *.o
