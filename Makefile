# GNU make手册：http://www.gnu.org/software/make/manual/make.html
# ************ 遇到不明白的地方请google以及阅读手册 *************

# 编译器设定和编译选项
CC = gcc
FLEX = flex
BISON = bison
CFLAGS = -std=c99

# 编译目标：src目录下的所有.c文件
CFILES = $(shell find ./ -name "*.c")
# 可重定位文件临时保存目录
OBJ_DIR = obj
# Flex文件 
LFILE = $(shell find ./ -name "*.l")
# Bison文件
YFILE = $(shell find ./ -name "*.y")
# Flex生成的C文件
LFC = $(shell find ./ -name "*.l" | sed s/[^/]*\\.l/lex.yy.c/)
# Bison生成的C文件
YFC = $(shell find ./ -name "*.y" | sed s/[^/]*\\.y/syntax.tab.c/)

CFO = $(CFILES:.c=.o)
LFO = $(LFC:.c=.o)
YFO = $(YFC:.c=.o)
C_OBJS = $(addprefix $(OBJ_DIR)/, $(notdir $(CFO)))
Y_OBJ = $(addprefix $(OBJ_DIR)/, $(notdir $(YFO)))
L_OBJ = $(addprefix $(OBJ_DIR)/, $(notdir $(LFO)))

# 链接parser和外部函数
# filter-out: 去掉$(OBJS)可能含有的$(LFO)
parser: syntax $(filter-out $(L_OBJ) $(Y_OBJ),$(C_OBJS))
	$(CC) -o parser $(Y_OBJ) $(filter-out $(L_OBJ) $(Y_OBJ), $(C_OBJS)) -lfl -ly

# 生成可重定位的parser
syntax: lexical syntax-c
	$(CC) -c $(YFC) -o $(Y_OBJ)

# 执行FLex
lexical: $(LFILE)
	$(FLEX) -o $(LFC) $(LFILE)

# 执行Bison
syntax-c: $(YFILE)
	$(BISON) -o $(YFC) -d -v $(YFILE)

# Pattern target
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(OBJ_DIR)
	@$(CC) $(CFLAGS) -c -o $@ $<

-include $(patsubst %.o, %.d, $(OBJS))

# 定义的一些伪目标
.PHONY: clean test play
test:
	./parser ../Test/test1.cmm
clean:
	rm -f parser lex.yy.c syntax.tab.c syntax.tab.h syntax.output
	rm -f $(C_OBJS) $(OBJS:.o=.d)
	rm -f $(LFC) $(YFC) $(YFC:.c=.h)
	rm -f *~

# 测试
play:
	@echo $(CFO)
	@echo $(LFO)
	@echo $(YFO)
	@echo $(C_OBJS)
	@echo $(L_OBJ)
	@echo $(Y_OBJ)

AWKFLAGS := 'BEGIN{printf("syntax keyword Type\t")}{printf("%s ", $$1)}END{print ""}'
CTAGSFLAGS := --c-kinds=gstu -o- 

# prototype: gen_types
define gen_types
	-@ctags $(CTAGSFLAGS) $^ | awk $(AWKFLAGS) > $@
endef

CHFILES = $(shell find -type f -name "*.[ch]")
types: types.vim
types.vim: $(CHFILES)
	@echo $(CHFILES)
	$(call gen_types)

cmm: cmm_type.c cmm_type.h lib.h
	$(CC) $(CFLAGS) -D TEST_TYPE -D DEBUG -o cmm_test cmm_type.c lib.c
