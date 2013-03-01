#CFLAGS+=-Wall $(shell pkg-config --cflags openssl) 
CFLAGS+=-Wall $(shell pkg-config --cflags openssl) -DDISABLESTUFF
DEBUGCFLAGS+=-DDEBUGCTL 
#DEBUGCFLAGS+=-DDEBUGCTL -DDEBUGSTUFF -DDEBUGALSA -DDEBUGBUFWRITE -g
#DEBUGCFLAGS+=-DDEBUGBUFWRITE -g
#DEBUGCFLAGS+=-DDEBUGSTUFF
AUDIO:=alsa
VOL:=alsa
ifneq ($(VOL),soft)
	USECFLAGS:=$(shell pkg-config --cflags alsa) 
	USELDFLAGS:=$(shell pkg-config --libs alsa)
else
	CFLAGS+=-DSOFT_VOL
endif
LDFLAGS+=-lm -lpthread $(shell pkg-config --libs openssl)
USECFLAGS+=$(shell pkg-config --cflags $(AUDIO)) 
USELDFLAGS+=$(shell pkg-config --libs $(AUDIO)) 
USEOBJS+=socketlib.o shairport.o alac.o hairtunes.o audio_$(AUDIO).o  vol_$(VOL).o
all: shairport

shairport: $(USEOBJS) 
	$(CC) $(CFLAGS) $(DEBUGCFLAGS) $(USECFLAGS) $(USEOBJS) -o $@ $(LDFLAGS) $(USELDFLAGS)

clean:
	-@rm -rf hairtunes shairport *.o



%.o: %.c Makefile
	$(CC) $(CFLAGS) $(DEBUGCFLAGS) -c $< -o $@


prefix=/usr/local
install: shairport
	install -D -m 0755 shairport $(DESTDIR)$(prefix)/bin/shairport

.PHONY: all clean install

.SILENT: clean

