CC = gcc
CFLAGS = -std=c99 -g -I.
TARGET = ../example
#SRCDIRS = .

OBJS = ae.o example.o zmalloc.o anet.o rbtree.o

all: dep $(TARGET)
.PHONY: all

$(TARGET):$(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

include Makefile.dep
dep:
	$(CC) -MM *.c > Makefile.dep
.PHONY: dep

rb_test: dep rbtree.o rb_test.o
	$(CC) $(CFLAGS) rbtree.o rb_test.o -o $@

clean:
	rm -f $(OBJS) $(TARGET)
