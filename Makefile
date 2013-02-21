#CFLAGS+=-Wall $(shell pkg-config --cflags openssl) -DDEBUGCTL -DDEBUGSTUFF -DDEBUGALSA -DUSE_ALSA_VOLUME -DDEBUGALAC -g
CFLAGS+=-Wall $(shell pkg-config --cflags openssl) 
USE:=alsa
LDFLAGS+=-lm -lpthread $(shell pkg-config --libs openssl)
ifeq ($(USE),ao)
  USECFLAGS:=$(shell pkg-config --cflags ao)
  USELDFLAGS:=$(shell pkg-config --libs ao)
  USEOBJS=socketlib.o shairport.o alac.o hairtunes.o audio_ao.o
else
  USELDFLAGS:=$(shell pkg-config --libs alsa)
  USECFLAGS:=$(shell pkg-config --cflags alsa)
  USEOBJS=socketlib.o shairport.o alac.o hairtunes.o audio_alsa.o
endif
all: hairtunes shairport

hairtunes: hairtunes.c alac.o audio_$(USE).o
	$(CC) $(CFLAGS) $(USECFLAGS) -DHAIRTUNES_STANDALONE hairtunes.c alac.o audio_$(USE).o -o $@ $(LDFLAGS) $(USELDFLAGS)

shairport: $(USEOBJS)
	$(CC) $(CFLAGS) $(USECFLAGS) $(USEOBJS) -o $@ $(LDFLAGS) $(USELDFLAGS)

hairtunes.ao: hairtunes.c alac.o audio_ao.o
	$(CC) $(CFLAGS) $(AOCFLAGS) -DHAIRTUNES_STANDALONE hairtunes.c alac.o audio_ao.o -o $@ $(LDFLAGS) $(AOLDFLAGS)

shairport.ao: $(AOOBJS)
	$(CC) $(CFLAGS) $(AOCFLAGS) $(AOOBJS) -o $@ $(LDFLAGS) $(AOLDFLAGS)
clean:
	-@rm -rf hairtunes shairport *.o



%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


prefix=/usr/local
install: hairtunes shairport
	install -D -m 0755 hairtunes $(DESTDIR)$(prefix)/bin/hairtunes
	install -D -m 0755 shairport.pl $(DESTDIR)$(prefix)/bin/shairport.pl
	install -D -m 0755 shairport $(DESTDIR)$(prefix)/bin/shairport

.PHONY: all clean install

.SILENT: clean

