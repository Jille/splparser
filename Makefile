CFLAGS=	-Wall -g
LDFLAGS=	-lpthread -g

parser: parser.o scanner.o input.o generator.o
	cc -o parser $(CFLAGS) $(LDFLAGS) parser.o scanner.o input.o generator.o

parser.o: parser.c tokens.h Makefile
	cc -c $(CFLAGS) parser.c

scanner.o: scanner.c scanner.h tokens.h Makefile
	cc -c $(CFLAGS) scanner.c

input.o: input.c Makefile
	cc -c $(CFLAGS) input.c

generator.o: generator.c generator.h Makefile
	cc -c $(CFLAGS) generator.c

clean:
	rm -f parser *.o tokens.h

tokens.h: gen-tokens.sh
	sh gen-tokens.sh
