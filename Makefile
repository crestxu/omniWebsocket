OBJ=WServer
TESTOBJ=utest
MCXXFLAGS =-g -pg  -pipe
INCLUDES=-I. 
PROGRAM = $(OBJ)
OBJECTS =main.o websocket.o anet.o ae.o ae_epoll.o adlist.o dict.o sds.o logs.o zmalloc.o
TESTOBJECTS =test.o websocket.o anet.o ae.o ae_epoll.o adlist.o dict.o sds.o logs.o zmalloc.o

CC = gcc

all : $(PROGRAM)

#test : $(TESTOBJ)


test : $(TESTOBJECTS)
	$(CC)   -o $(TESTOBJ) $(TESTOBJECTS) $(MCXXFLAGS) $(INCLUDES) $(LIBS)  

$(OBJ) : $(OBJECTS)
	$(CC)   -o $(OBJ) $(OBJECTS) $(MCXXFLAGS) $(INCLUDES) $(LIBS)  
	
%.o : %.c
	$(CC) $(MCXXFLAGS) $(INCLUDES)  -o $@ -c $<

clean :
	rm -fr *.o
	rm $(PROGRAM)
