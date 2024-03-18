CFLAGS=-Wall -Wunused -ffunction-sections -fdata-sections
OBJS=stack.o base.o int.o selfless.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

all: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o selfless

tricolore: $(OBJS) bitmap.c
	$(CC) $(CFLAGS) bitmap.c -o $@ `pkg-config --libs --cflags cairo gdk-3.0 gtk+-3.0`

clean:
	rm -rf *.o selfless
