# Standard flags
# ==============
# Will be extended depending on build-mode, system and compiler
CFLAGS += \
	-std=gnu99 \
	-fPIC \
	-Wall \
	-Wextra \
	-Werror \
	-Wstrict-prototypes \
	-pedantic \
	-Wno-unused-function \
	$(VISIBLITYFLAG)

LDFLAGS += \
	$(VISIBLITYFLAG) \
	-luv \
	-lssl \
	-lcrypto \
	-lm \
	-lpthread \


# Export API symbols only
# =======================
CFLAGS += -DCH_BUILD

# Only define for known compilers
ifeq ($(CC),clang)
	VISIBLITYFLAG := -fvisibility=hidden
endif
ifeq ($(CC),gcc)
	VISIBLITYFLAG := -fvisibility=hidden
endif
ifeq ($(CC),cc)
	_CC_OUT := $(shell cc -v 2>&1)
ifneq (,$(findstring gcc version,$(_CC_OUT)))
	VISIBLITYFLAG := -fvisibility=hidden
endif
ifneq (,$(findstring clang version,$(_CC_OUT)))
	VISIBLITYFLAG := -fvisibility=hidden
endif
endif

# Additional flags
# ================
ifeq ($(UNAME_S),Darwin)
# Homebrew include path
CFLAGS += -I/usr/local/opt/openssl/include
LDFLAGS += -L/usr/local/opt/openssl/lib
else
# Linux specific
CFLAGS += -pthread
LDFLAGS += -lrt
endif

# Strip command
# =============
ifeq ($(UNAME_S),Darwin)
STRPCMD := strip -S
else
STRPCMD:= strip --strip-debug
endif
