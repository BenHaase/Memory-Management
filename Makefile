CC = gcc
CFLAGS = -g
TARGET1 = oss
TARGET2 = uproc
HFILES = page.h
OBJS1 = oss.o page.o
OBJS2 = uproc.o page.o
LIBS = -lpthread
CLFILES = oss uproc uproc.o oss.o page.o osstest
.SUFFIXES: .c .o

all: oss uproc $(MYLIBS)
	

$(TARGET1): $(OBJS1) $(HFILES)
	$(CC) -o $@ $(OBJS1) $(LIBS)

$(TARGET2): $(OBJS2) $(HFILES)
	$(CC) -o $@ $(OBJS2) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) -c $<

clean:
	/bin/rm -f $(CLFILES) cstest
