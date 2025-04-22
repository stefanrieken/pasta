CFLAGS=-Wall -Wunused -ffunction-sections -fdata-sections -DLEXICAL_SCOPING
OBJS=src/stack.o src/base.o src/int.o src/pasta.o src/chef.c

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ `pkg-config --cflags cairo gdk-3.0 gtk+-3.0 libpng`

all: pasta tricolore

pasta: $(OBJS) src/main.o
	$(CC) $(CFLAGS) $^ -o $@

tricolore: $(OBJS) src/bitmap.o src/tricolore.o
	$(CC) $(CFLAGS) $^ -o $@ `pkg-config --libs cairo gdk-3.0 gtk+-3.0 libpng`

rp2040: rp2040/Makefile src/*.c
	cd rp2040 && make

rp2040/Makefile:
	mkdir -p rp2040
	cd rp2040 && cmake .. -DPICO_PLATFORM=rp2040

rp2350: rp2350/Makefile src/*.c
	cd rp2350 && make

rp2350/Makefile:
	mkdir -p rp2350
	cd rp2350 && cmake .. -DPICO_PLATFORM=rp2350

clean:
	rm -rf src/*.o pasta tricolore rp2040
