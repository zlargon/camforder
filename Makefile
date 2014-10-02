
#CFLAGS= -Wall -Werror
LDFLAGS= -lssl
OBJS=camforder.o log.o noly.o easyssl.o
EXEC= camforder.exe

all: $(OBJS)
	$(CC) -o $(EXEC) $(OBJS) $(CFLAGS) $(LDFLAGS)
clean:
	rm -rf *.o *.exe *.a *.so
