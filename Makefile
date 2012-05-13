CFLAGS=	-Wall -Wno-unused-function -g
LDFLAGS=	-lpthread -g

parser: parser.o scanner.o input.o generator.o lazyarray.o grammar.o types.o ir.o ssm.o
	$(CC) -o parser $(CFLAGS) $(LDFLAGS) parser.o scanner.o input.o generator.o lazyarray.o grammar.o types.o ir.o ssm.o

scanner: scanner.o input.o generator.o grammar.o
	$(CC) -o scanner $(CFLAGS) $(LDFLAGS) scanner.c input.o generator.o grammar.o -DSTANDALONE

parser.o: parser.c tokens.h Makefile
	$(CC) -c $(CFLAGS) parser.c

types.o: types.c types.h ir.h Makefile
	$(CC) -c $(CFLAGS) types.c

scanner.o: scanner.c scanner.h tokens.h Makefile
	$(CC) -c $(CFLAGS) scanner.c

input.o: input.c Makefile
	$(CC) -c $(CFLAGS) input.c

generator.o: generator.c generator.h Makefile
	$(CC) -c $(CFLAGS) generator.c

lazyarray.o: lazyarray.c lazyarray.h generator.o Makefile
	$(CC) -c $(CFLAGS) lazyarray.c

grammar.o: grammar.c grammar.h Makefile
	$(CC) -c $(CFLAGS) grammar.c

ir.o: ir.c ir.h Makefile
	$(CC) -c $(CFLAGS) ir.c

ssm.o: ssm.c ssm.h Makefile
	$(CC) -c $(CFLAGS) ssm.c

clean:
	rm -f parser *.o tokens.h

tokens.h: gen-tokens.sh
	sh gen-tokens.sh
