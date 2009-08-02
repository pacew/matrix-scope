CFLAGS = -g -Wall `pkg-config --cflags gtk+-2.0`
LIBS = `pkg-config --libs gtk+-2.0` -llapack -lopus -lm

all: matrix-scope

matrix-scope: matrix-scope.o
	$(CC) $(CFLAGS) -o matrix-scope matrix-scope.o $(LIBS)

clean:
	rm -f *.o *~ matrix-scope
