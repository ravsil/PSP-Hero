TARGET = psphero
OBJS = src/main.o \
       src/core/chart_parser.o \
       src/core/audio_player.o \
       src/core/song_manager.o \
       src/core/config.o \
       src/core/stb_vorbis.o \
       src/render/renderer.o \
       src/game/highway_renderer.o \
       src/game/gameplay.o \
       src/ui/menu_screens.o

INCDIR = src src/core src/render src/game src/ui /usr/local/pspdev/psp/include/opus
CFLAGS = -Wall -O2 -G0
CXXFLAGS = $(CFLAGS) -std=c++17 -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
LIBS = -lstdc++ -lopusfile -lopus -logg -lpspaudio -lpsppower -lpspgum -lpspgu -lpsprtc -lm

BUILD_PRX = 1
ENCRYPT = 1

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PSP Hero
PSP_EBOOT_ICON = ICON0.PNG

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
