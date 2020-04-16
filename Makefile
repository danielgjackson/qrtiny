#  -fstack-usage -fdump-tree-optimized

BIN_NAME = qrtiny
CC = gcc
CFLAGS = -O1 -Wall -Wstrict-overflow=0
LIBS = 

SRC = $(wildcard *.c)
INC = $(wildcard *.h)

all: $(BIN_NAME)

$(BIN_NAME): Makefile $(SRC) $(INC)
	$(CC) -std=c99 -o $(BIN_NAME) $(CFLAGS) $(USER_DEFINES) $(SRC) -I/usr/local/include -L/usr/local/lib $(LIBS)

clean:
	rm -f *.o core $(BIN_NAME)
