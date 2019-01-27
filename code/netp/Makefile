CC = gcc    # gcc 编译器

# gcc带不同参数的含义:"-g"产生调试信息，"-c"编译中间目标文件，"-I"指定链接库，"-o"生成指定命名的可执行文件。

CSAPP_INC = ../include
CSAPP_SRC = ../src

CFLAGS = -Wall -O2 -I $(CSAPP_INC) -I .
LDLIBS = -lpthread

# 整个程序
# 只要前置文件的更新时间比target文件的更新时间还要晚的话，就重新编译
PROGS =	echo.o\
	dd2hex\
	hex2dd\
	hostinfo\
	hostinfo-ntop\
	echoclient\
	echoserveri\
	tiny/tiny\
	netpfragments.o\

all: $(CSAPP_SRC)/csapp.o $(PROGS) 
$(PROGS): $(CSAPP_SRC)/csapp.o
$(CSAPP_SRC)/csapp.o: $(CSAPP_SRC)/csapp.c $(CSAPP_INC)/csapp.h

tiny/tiny: tiny/tiny.c
	(cd tiny; make)	

# Programs that need more than one .o file
echoserveri: echoserveri.o echo.o
 
tinytarfile:
	(cd tiny; make clean)
	tar cvf - tiny >tiny.tar

clean:
	rm -f $(PROGS) *.o *~
	(cd tiny; make clean)
