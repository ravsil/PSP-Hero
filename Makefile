TARGET = cloneHero
OBJS = src/main.o src/chart_parser.o src/audio_player.o src/highway_renderer.o

INCDIR = src /usr/local/pspdev/psp/include/opus
CFLAGS = -Wall -O2 -G0
CXXFLAGS = $(CFLAGS) -std=c++17 -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
LIBS = -lstdc++ -lopusfile -lopus -logg -lpspaudio -lpsppower -lpspgum -lpspgu -lpsprtc -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Clone Hero PSP

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
