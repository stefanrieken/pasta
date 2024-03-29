CFLAGS=-Wall -Wunused -ffunction-sections -fdata-sections
OBJS=src/stack.o src/base.o src/int.o src/pasta.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ `pkg-config --cflags cairo gdk-3.0 gtk+-3.0`

all: pasta tricolore

pasta: $(OBJS) src/main.o
	$(CC) $(CFLAGS) $^ -o $@

tricolore: $(OBJS) src/bitmap.o src/tricolore.o
	$(CC) $(CFLAGS) $^ -o $@ `pkg-config --libs cairo gdk-3.0 gtk+-3.0`

clean:
	rm -rf src/*.o pasta tricolore
