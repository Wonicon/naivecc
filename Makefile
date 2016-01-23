CC = gcc
FLEX = flex
BISON = bison
CFLAGS = -std=gnu99 -Wall -Werror -ggdb -D DEBUG

CFILES = $(shell find ./ -name "*.c")
OBJS = $(CFILES:.c=.o)
LFILE = $(shell find ./ -name "*.l")
YFILE = $(shell find ./ -name "*.y")
LFC = $(shell find ./ -name "*.l" | sed s/[^/]*\\.l/lex.yy.c/)
YFC = $(shell find ./ -name "*.y" | sed s/[^/]*\\.y/syntax.tab.c/)
LFO = $(LFC:.c=.o)
YFO = $(YFC:.c=.o)

COMPILER := cmm

$(COMPILER): syntax $(filter-out $(LFO),$(OBJS))
	$(CC) -ggdb -o $@ $(filter-out $(LFO),$(OBJS)) -lfl -ly

syntax: lexical syntax-c
	$(CC) -ggdb -c $(YFC) -o $(YFO)

lexical: $(LFILE)
	$(FLEX) -o $(LFC) $(LFILE)

syntax-c: $(YFILE)
	$(BISON) -o $(YFC) -d -v $(YFILE)

-include $(patsubst %.o, %.d, $(OBJS))

.PHONY: clean test gdb

test: $(COMPILER)
	./$(COMPILER) test.cmm test.S
	spim -file test.S

gdb: $(COMPILER)
	gdb $(COMPILER) -ex "set args test.cmm test.S"

clean:
	rm -f $(COMPILER) lex.yy.c syntax.tab.c syntax.tab.h syntax.output
	rm -f $(OBJS) $(OBJS:.o=.d)
	rm -f $(LFC) $(YFC) $(YFC:.c=.h)
	rm -f *~
