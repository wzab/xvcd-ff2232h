LDLIBS=-lusb -lftdi
CC=$(CROSS_COMPILE)gcc
OBJS := xvcServer.o ftdi_xvc_core.o
all:		xvcd-ft2232h
xvcd-ft2232h:	$(OBJS)
	$(CC) -o xvcd-ft2232h $(OBJS) $(LDLIBS)
$(OBJS) : %.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@

