ifndef cc
cc = clang
endif
ifndef ld
ld = clang
endif
sc = strip
cflags = -O2 -g0
lflags = -lc

obj = test.o config.o

test : $(obj)
	$(ld) -o $@ $(obj) $(lflags)
	$(sc) $@

test.o : test.c config.h
config.o : config.c config.h

%.o : %.c
	$(cc) $(cflags) -c $<

clean:
	if ls test > /dev/null 2>&1 ;  then rm test ; fi
	if ls *.o > /dev/null 2>&1 ; then rm *.o ; fi
