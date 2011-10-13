SRC=gtkbookmark.cpp
OBJ=$(SRC:.cpp=.o)
OUT=gtkbookmark

INCLUDES=-I/usr/include/g++ -I/usr/local/include -I.
CCFLAGS=-O2 `libglade-config --cflags` `gtk-config --cflags`
CCC=g++

LIBS=-lm
LDFLAGS=`gtk-config --libs` `libglade-config --libs`

##### RULES #####

.SUFFIXES: .c .c .ln

.cpp.o: 
	$(CCC) $(INCLUDES) $(CCFLAGS) -c $< -o $@

$(OUT): $(OBJ)
	$(CCC) $(OBJ) $(LDFLAGS) $(LIBS) -o $(OUT)

clean:
	/bin/rm -f $(OBJ) $(OUT)
