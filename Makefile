
#CFLAGS= -Wall -Werror
#LDFLAGS= -luv
OBJS=camforder.o log.o noly.o
EXEC= camforder.exe

all: $(OBJS)
	$(CC) -o $(EXEC) $(OBJS) $(CFLAGS) $(LDFLAGS)
clean:
	rm -rf *.o *.exe *.a *.so
