CFLAGS=-Wall -Wunused -ffunction-sections -fdata-sections
OBJS=stack.o base.o int.o selfless.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ `pkg-config --libs --cflags cairo gdk-3.0 gtk+-3.0`

all: selfless tricolore

selfless: $(OBJS) main.o
	$(CC) $(CFLAGS) $^ -o $@

tricolore: $(OBJS) bitmap.o tricolore.o
	$(CC) $(CFLAGS) $^ -o $@ `pkg-config --libs --cflags cairo gdk-3.0 gtk+-3.0`

clean:
	rm -rf *.o selfless
