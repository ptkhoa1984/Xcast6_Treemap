CFLAGS=		-I. -Wall -D_BSD_SOURCE
SRCS=		group.c member.c send.c sockopt.c hdrlen.c cksum.c
OBJS=		$(SRCS:.c=.o)
SOBJS=		$(SRCS:.c=.so)

SHLIB_MAJOR=	0
SHLIB_MINOR=	0

TARGET=		libxcast
TARGET_A=	$(TARGET).a
TARGET_SO=	$(TARGET).so
TARGET_SONAME=	$(TARGET_SO).$(SHLIB_MAJOR)
TARGET_SO_VER=	$(TARGET_SONAME).$(SHLIB_MINOR)

all: $(TARGET_A) $(TARGET_SO_VER)

$(TARGET_A): $(OBJS)
	$(AR) rc $@ $(OBJS)

$(TARGET_SO_VER): $(SOBJS)
	$(CC) -shared -Wl,-soname -Wl,$(TARGET_SONAME) \
		-o $@ $(SOBJS)

clean:
	rm -f $(TARGET_A) $(TARGET_SO) $(OBJS) $(SOBJS)

install:
	cp $(TARGET_A) $(OBJS) $(SOBJS) /usr/lib
	cp $(TARGET_SO).$(SHLIB_MAJOR).0 /usr/lib/libxcast.so
	cp libxcast.h /usr/include/
	ldconfig

.c.o:
	$(CC) $(CFLAGS) -c $<

.c.so:
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

.SUFFIXES: $(.SUFFIXES) .so
