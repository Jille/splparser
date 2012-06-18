PARSER=../parser

RESULTS=sumtuple sumlist printpid printrands reverseprint

all: $(RESULTS)
	@echo "Build complete. Try the following examples:"
	@echo "sumtuple - Sum both values in the tuple, print the result"
	@echo "sumlist - Sum all values in a list, print the operation and result"
	@echo "printpid - Print the process ID"
	@echo "printrands - Generate and print 10 random numbers"
	@echo "reverseprint - Reverse a list, print numbers as they are processed"

sumtuple: stdlib.ast sumtuple.spl
	$(PARSER) -o sumtuple sumtuple.spl

sumlist: stdlib.ast sumlist.spl
	$(PARSER) -o sumlist sumlist.spl

printpid: stdlib.ast printpid.spl
	$(PARSER) -o printpid printpid.spl

printrands: getrands.ast printrands.spl stdlib.ast
	$(PARSER) -o printrands getrands.ast printrands.spl

reverseprint: stdlib.ast reverseprint.spl
	$(PARSER) -o reverseprint reverseprint.spl

getrands.ast: stdlib.ast getrands.spl
	$(PARSER) -p getrands.spl

stdlib.ast: $(PARSER) stdlib.spl
	$(PARSER) -p stdlib.spl

../parser:
	cd ..; make parser; cd examples

clean:
	rm -f stdlib.ast getrands.ast $(RESULTS)

.PHONY: clean
