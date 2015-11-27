CFLAGS= -DHAVE_VA_X11 -g -DMULTI_PROCESS -DPUT_XWINDOW
LDFLAGS= -lva -lva-glx -lX11 -lglut -lGL -lpthread -lva-x11
SRC=$(wildcard ./*.c)
OBJ=$(patsubst %.c,%.o,$(SRC))
TARGET=GLXVAAPICopySurfacePerformance
All:$(TARGET)

$(TARGET):$(OBJ)
	gcc -o $@ $^ $(LDFLAGS)

.c.o:
	gcc -c $(CFLAGS) -o $@ $<
run:$(TARGET)
	./$(TARGET)
clean:
	rm -rfv *.o $(TARGET)
	rm -rfv *~
