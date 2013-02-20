CFLAGS+=-Wall $(shell pkg-config --cflags openssl) -DDEBUGCTL -DDEBUGSTUFF -DDEBUGALSA -DUSE_ALSA_VOLUME
LDFLAGS+=-lm -lpthread $(shell pkg-config --libs openssl)
AOCFLAGS:=$(shell pkg-config --cflags ao)
AOLDFLAGS:=$(shell pkg-config --libs ao)
ALSALDFLAGS:=$(shell pkg-config --libs alsa)
ALSACFLAGS:=$(shell pkg-config --cflags alsa)
AOOBJS=socketlib.o shairport.o alac.o hairtunes.o audio_ao.o
ALSAOBJS=socketlib.o shairport.o alac.o hairtunes.o audio_alsa.o
all: hairtunes shairport
ao: hairtunes.ao shairport.ao

hairtunes: hairtunes.c alac.o audio_alsa.o
	$(CC) $(CFLAGS) $(ALSACFLAGS) -DHAIRTUNES_STANDALONE hairtunes.c alac.o audio_alsa.o -o $@ $(LDFLAGS) $(ALSALDFLAGS)

shairport: $(ALSAOBJS)
	$(CC) $(CFLAGS) $(ALSACFLAGS) $(ALSAOBJS) -o $@ $(LDFLAGS) $(ALSALDFLAGS)

hairtunes.ao: hairtunes.c alac.o audio_ao.o
	$(CC) $(CFLAGS) $(AOCFLAGS) -DHAIRTUNES_STANDALONE hairtunes.c alac.o audio_ao.o -o $@ $(LDFLAGS) $(AOLDFLAGS)

shairport.ao: $(AOOBJS)
	$(CC) $(CFLAGS) $(AOCFLAGS) $(AOOBJS) -o $@ $(LDFLAGS) $(AOLDFLAGS)
clean:
	-@rm -rf hairtunes shairport hairtunes.ao shairport.ao $(ALSAOBJS) $(AOOBJS)


%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


prefix=/usr/local
install: hairtunes shairport
	install -D -m 0755 hairtunes $(DESTDIR)$(prefix)/bin/hairtunes
	install -D -m 0755 shairport.pl $(DESTDIR)$(prefix)/bin/shairport.pl
	install -D -m 0755 shairport $(DESTDIR)$(prefix)/bin/shairport

.PHONY: all clean install

.SILENT: clean

