IPATH=$(DESTDIR)/usr/local/sbin

all: pps2gpio

pps2gpio: error.o gpio-int-test.o pps2gpio.o

install: all
	cp pps2gpio $(IPATH)

uninstall :
	rm $(IPATH)/pps2gpio

clean :
	rm -f *.o pps2gpio
