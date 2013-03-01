#CFLAGS+=-Wall $(shell pkg-config --cflags openssl) 
CFLAGS+=-Wall $(shell pkg-config --cflags openssl) -DDISABLESTUFF
#DEBUGCFLAGS+=-DDEBUGCTL -DDEBUGSTUFF -DDEBUGALSA -DDDEBUGALAC -DDEBUGALSAVOL -DDEBUGBUFWRITE -g
#DEBUGCFLAGS+=-DDEBUGBUFWRITE -g
#DEBUGCFLAGS+=-DDEBUGSTUFF
USE:=alsa
USE_ALSA_VOLUME:=1
ifeq ($(USE_ALSA_VOLUME),1)
	USECFLAGS:=$(shell pkg-config --cflags alsa) 
	USELDFLAGS:=$(shell pkg-config --libs alsa)
	USEOBJS:=vol_alsa.o 
endif
LDFLAGS+=-lm -lpthread $(shell pkg-config --libs openssl)
USECFLAGS+=$(shell pkg-config --cflags $(USE)) 
USELDFLAGS+=$(shell pkg-config --libs $(USE)) 
USEOBJS+=socketlib.o shairport.o alac.o hairtunes.o audio_$(USE).o 
ifeq ($(USE_ALSA_VOLUME),1)	
	CFLAGS+=-DUSE_ALSA_VOLUME
endif
ifeq ($(USE),alsa)
        CFLAGS+=-DALSA
else
	CFLAGS+=-DAO
endif
all: shairport

hairtunes: hairtunes.c alac.o audio_$(USE).o
	$(CC) $(CFLAGS) $(DEBUGCFLAGS) $(USECFLAGS) -DHAIRTUNES_STANDALONE hairtunes.c alac.o audio_$(USE).o -o $@ $(LDFLAGS) $(USELDFLAGS)

shairport: $(USEOBJS) 
	$(CC) $(CFLAGS) $(DEBUGCFLAGS) $(USECFLAGS) $(USEOBJS) -o $@ $(LDFLAGS) $(USELDFLAGS)

clean:
	-@rm -rf hairtunes shairport *.o



%.o: %.c Makefile
	$(CC) $(CFLAGS) $(DEBUGCFLAGS) -c $< -o $@


prefix=/usr/local
install: shairport
	install -D -m 0755 shairport $(DESTDIR)$(prefix)/bin/shairport

install-pl: hairtunes shairport
	install -D -m 0755 hairtunes $(DESTDIR)$(prefix)/bin/hairtunes
	install -D -m 0755 shairport.pl $(DESTDIR)$(prefix)/bin/shairport.pl
	install -D -m 0755 shairport $(DESTDIR)$(prefix)/bin/shairport

.PHONY: all clean install

.SILENT: clean

