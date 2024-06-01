all: blockmgr.o inodefile.o idisk.o filesystem.o userfile.o directory.o server fstest client clean

blockmgr.o: blockmgr.cc blockmgr.h
	g++ -c blockmgr.cc -O2 -Wall

inodefile.o: inodefile.cc inodefile.h
	g++ -c inodefile.cc -O2 -Wall

idisk.o: idisk.cc idisk.h
	g++ -c idisk.cc -I.. -O2 -Wall

filesystem.o: filesystem.cc filesystem.h
	g++ -c filesystem.cc -O2 -Wall

userfile.o: userfile.cc userfile.h
	g++ -c userfile.cc -O2 -Wall

directory.o: directory.cc directory.h
	g++ -c directory.cc -O2 -Wall

fstest: fstest.cc blockmgr.o inodefile.o idisk.o filesystem.o userfile.o directory.o
	g++ -o ../bin/fstest fstest.cc filesystem.o userfile.o directory.o inodefile.o blockmgr.o idisk.o ../bin/bytepack.o ../bin/network.o -O2 -Wall -fsanitize=address

server: server.cc filesystem.o blockmgr.o inodefile.o idisk.o userfile.o directory.o
	g++ -o ../bin/FS -I.. server.cc filesystem.o userfile.o directory.o inodefile.o blockmgr.o idisk.o ../bin/bytepack.o ../bin/network.o -O2 -Wall -fsanitize=address

client: client.cc
	g++ -o ../bin/FC -I.. client.cc ../bin/bytepack.o ../bin/network.o -O2 -Wall

clean: fstest server
	rm -f *.o