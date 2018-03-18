PREFIX ?= /usr/local

all: libchirp.so libchirp.a

# Library
# =======
libchirp.so: libchirp.o
	$(CC) -shared -o $@ $+ $(LDFLAGS)
ifeq ($(STRIP),True)
	$(STRPCMD) $@
endif

libchirp.a: libchirp.o
	ar $(ARFLAGS) $@ $+
ifeq ($(STRIP),True)
	$(STRPCMD) $@
endif

# Checks
# ======
chirp_test: chirp_test.o libchirp.so
	$(CC) -o $@ $< -L. -lchirp $(LDFLAGS)

echo_test: echo_test.o libchirp.so
	$(CC) -o $@ $< -L. -lchirp $(LDFLAGS)

send_test: send_test.o libchirp.so
	$(CC) -o $@ $< -L. -lchirp $(LDFLAGS)

check: chirp_test echo_test send_test
	@cat .keys/dh.pem | tr '%' 'a' > dh.pem
	@cat .keys/cert.pem | tr '%' 'a' > cert.pem
	@LD_LIBRARY_PATH="." ./chirp_test
	@TMPOUT=$$(mktemp); \
	LD_LIBRARY_PATH="." ./echo_test 3000 1 2> $$TMPOUT & \
	PID=$$!; \
	sleep 1; \
	cat $$TMPOUT; \
	LD_LIBRARY_PATH="." ./send_test 1 2 127.0.0.1:3000 || exit 1; \
	sleep 1; \
	kill $$PID; \
	grep -q "Echo message" $$TMPOUT || exit 1; \
	rm $$TMPOUT

	@rm -f *.pem

# Install
# =======
install: all
	mkdir -p $(DEST)$(PREFIX)/lib
	cp -f libchirp.a $(DEST)$(PREFIX)/lib
	cp -f libchirp.so $(DEST)$(PREFIX)/lib/libchirp.so.$(VERSION)
	mkdir -p $(DEST)$(PREFIX)/include
	cp -f libchirp.h $(DEST)$(PREFIX)/include/libchirp.h
	cd $(DEST)$(PREFIX)/lib && ln -sf libchirp.so.$(VERSION) libchirp.so
	cd $(DEST)$(PREFIX)/lib && ln -sf libchirp.so.$(VERSION) libchirp.so.$(MAJOR)

uninstall:
	rm -f $(DEST)$(PREFIX)/lib/libchirp.so
	rm -f $(DEST)$(PREFIX)/lib/libchirp.so.$(VERSION)
	rm -f $(DEST)$(PREFIX)/lib/libchirp.so.$(MAJOR)
	rm -f $(DEST)$(PREFIX)/lib/libchirp.a
	rm -f $(DEST)$(PREFIX)/include/libchirp.h

clean:
	rm -f *.o libchirp.so libchirp.a chirp_test
