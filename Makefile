CFLAGS=	-Wall -Wno-unused-function -g
LDFLAGS=	-lpthread -g

parser: parser.o scanner.o input.o generator.o lazyarray.o grammar.o types.o ir.o
	cc -o parser $(CFLAGS) $(LDFLAGS) parser.o scanner.o input.o generator.o lazyarray.o grammar.o types.o ir.o

scanner: scanner.o input.o generator.o grammar.o
	cc -o scanner $(CFLAGS) $(LDFLAGS) scanner.c input.o generator.o grammar.o -DSTANDALONE

parser.o: parser.c tokens.h Makefile
	cc -c $(CFLAGS) parser.c

types.o: types.c types.h ir.h Makefile
	cc -c $(CFLAGS) types.c

scanner.o: scanner.c scanner.h tokens.h Makefile
	cc -c $(CFLAGS) scanner.c

input.o: input.c Makefile
	cc -c $(CFLAGS) input.c

generator.o: generator.c generator.h Makefile
	cc -c $(CFLAGS) generator.c

lazyarray.o: lazyarray.c lazyarray.h generator.o Makefile
	cc -c $(CFLAGS) lazyarray.c

grammar.o: grammar.c grammar.h Makefile
	cc -c $(CFLAGS) grammar.c

ir.o: ir.c ir.h Makefile
	cc -c $(CFLAGS) ir.c

clean:
	rm -f parser *.o tokens.h

tokens.h: gen-tokens.sh
	sh gen-tokens.sh
