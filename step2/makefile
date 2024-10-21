all: blockmgr.o inodefile.o idisk.o filesystem.o userfile.o directory.o server fstest client clean

blockmgr.o: blockmgr.cc blockmgr.h
	g++ -c blockmgr.cc -O2 -Wall -std=c++17

inodefile.o: inodefile.cc inodefile.h
	g++ -c inodefile.cc -O2 -Wall -std=c++17

idisk.o: idisk.cc idisk.h
	g++ -c idisk.cc -I.. -O2 -Wall -std=c++17

filesystem.o: filesystem.cc filesystem.h
	g++ -c filesystem.cc -O2 -Wall -std=c++17

userfile.o: userfile.cc userfile.h
	g++ -c userfile.cc -O2 -Wall -std=c++17

directory.o: directory.cc directory.h
	g++ -c directory.cc -O2 -Wall -std=c++17

fstest: fstest.cc blockmgr.o inodefile.o idisk.o filesystem.o userfile.o directory.o
	g++ -o ../bin/fstest fstest.cc filesystem.o userfile.o directory.o inodefile.o blockmgr.o idisk.o ../bin/bytepack.o ../bin/network.o -O2 -Wall -fsanitize=address -std=c++17

server: server.cc filesystem.o blockmgr.o inodefile.o idisk.o userfile.o directory.o
	g++ -o ../bin/FS -I.. server.cc filesystem.o userfile.o directory.o inodefile.o blockmgr.o idisk.o ../bin/bytepack.o ../bin/network.o -O2 -Wall -fsanitize=address -std=c++17

client: client.cc
	g++ -o ../bin/FC -I.. client.cc ../bin/bytepack.o ../bin/network.o -O2 -Wall -std=c++17

clean: fstest server
	rm -f *.o
