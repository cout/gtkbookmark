SRC=gtkbookmark.cpp
OBJ=$(SRC:.cpp=.o)
OUT=gtkbookmark

INCLUDES=-I/usr/include/g++ -I/usr/local/include -I.
CCFLAGS=-g `libglade-config --cflags` `gtk-config --cflags` \
	`glib-config --cflags` `imlib-config --cflags` -Wall
CCC=g++

LIBS=-lGLU -lGL -lgtkgl
LDFLAGS=`gtk-config --libs` `libglade-config --libs` \
	`glib-config --libs` `imlib-config --libs` -lgdk_imlib -lm

##### RULES #####

.SUFFIXES: .c .c .ln

.cpp.o: 
	$(CCC) $(INCLUDES) $(CCFLAGS) -c $< -o $@

$(OUT): $(OBJ)
	$(CCC) $(OBJ) $(LDFLAGS) $(LIBS) -o $(OUT)

clean:
	/bin/rm -f $(OBJ) $(OUT)
