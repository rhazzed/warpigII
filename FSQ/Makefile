CFLAGS= -Wall -fno-strict-aliasing -fwrapv -Wstrict-prototypes -std=c99 -lm -lwiringPi
all: si5351.o KJ6FOWSPR.o KJ6FO_FS2

si5351.o: si5351.cpp si5351.h
	g++ -g -c si5351.cpp -o si5351.o

KJ6FOWSPR.o: KJ6FOWSPR.cpp KJ6FOWSPR.h
	g++ -g -c KJ6FOWSPR.cpp -o KJ6FOWSPR.o

KJ6FO_FS2.o: KJ6FO_FS2.cpp
	g++ -g -c KJ6FO_FS2.cpp -o KJ6FO_FS2.o

KJ6FO_FS2: KJ6FO_FS2.o KJ6FOWSPR.o si5351.o
	g++ -g $(CFLAGS) KJ6FO_FS2.o KJ6FOWSPR.o si5351.o -o KJ6FO_FS2

clean:
	rm -f *.o KJ6FO_FS2
