.PHONY += test cppcheck etests pytest todo help
.DEFAULT_GOAL := help

# Development flags (see base.mk for standard flags)
# ==================================================
CFLAGS += \
	-O0 \
	-ggdb3 \

LDFLAGS += -L"$(BUILD)" \

ifeq ($(CI_DISTRO),arch)
	CFLAGS += -fsanitize=alignment -fno-sanitize-recover=alignment
	LDFLAGS += -lubsan
endif

# Memcheck settings
# =================
ifeq ($(NO_MEMCHECK),True)
	MEMCHECK :=
else
ifneq ($(TLS),openssl)
	MEMCHECK := valgrind \
		--tool=memcheck \
		--leak-check=full \
		--errors-for-leak-kinds=all \
		--show-leak-kinds=all \
		--error-exitcode=1 \
		--suppressions=$(BASE)/ci/memcheck.supp
else
	MEMCHECK := valgrind \
		--tool=memcheck \
		--suppressions=$(BASE)/ci/memcheck.supp
endif
endif

# Binary tests to run
# ===================

stest: all
	$(MEMCHECK) $(BUILD)/src/message_etest \
			2> message_etest.log || \
		(cat message_etest.log; false)
	$(MEMCHECK) $(BUILD)/src/message_etest --always-encrypt \
			2> message_etest.log || \
		(cat message_etest.log; false)

etests: stest  ## Run binary tests
	LD_LIBRARY_PATH="$(BUILD)" $(BUILD)/src/chirp_etest
	$(BUILD)/src/quickcheck_etest
	$(MEMCHECK) $(BUILD)/src/quickcheck_etest
	$(BUILD)/src/uninit_etest
	$(MEMCHECK) $(BUILD)/src/uninit_etest
	$(BUILD)/src/serializer_etest
	$(MEMCHECK) $(BUILD)/src/serializer_etest
	$(BUILD)/src/message_etest \
			2> message_etest.log || \
		(cat message_etest.log; false)
	$(BUILD)/src/message_etest --max-msg-size 4 2> /dev/null; [ $$? -ne 0 ]
	$(MEMCHECK) $(BUILD)/src/message_etest --async --always-encrypt \
			2> message_etest.log || \
		(cat message_etest.log; false)
	$(MEMCHECK) $(BUILD)/src/message_etest --async \
			2> message_etest.log || \
		(cat message_etest.log; false)
	$(MEMCHECK) $(BUILD)/src/message_etest \
			--always-encrypt \
			--message-count 50 \
			2> message_etest.log || \
		(cat message_etest.log; false)
	$(MEMCHECK) $(BUILD)/src/message_etest \
			--always-encrypt \
			--slow \
			2> message_etest.log || \
		(cat message_etest.log; false)
	$(MEMCHECK) $(BUILD)/src/message_etest \
			--slow \
			2> message_etest.log || \
		(cat message_etest.log; false)
	$(MEMCHECK) $(BUILD)/src/message_etest \
			--async \
			--always-encrypt \
			--slow \
			--message-count 150 \
			2> message_etest.log || \
		(cat message_etest.log; false)
	$(MEMCHECK) $(BUILD)/src/message_etest \
			--async \
			--slow \
			--message-count 150 \
			2> message_etest.log || \
		(cat message_etest.log; false)
	$(MEMCHECK) $(BUILD)/src/message_etest \
			--async \
			--always-encrypt \
			--slow \
			--min-slots \
			2> message_etest.log || \
		(cat message_etest.log; false)
	$(MEMCHECK) $(BUILD)/src/message_etest \
			--async \
			--always-encrypt \
			--min-slots \
			2> message_etest.log || \
		(cat message_etest.log; false)
	$(MEMCHECK) $(BUILD)/src/message_etest \
			--always-encrypt \
			--buffer-size 1024 \
			--timeout 30 \
			2> message_etest.log || \
		(cat message_etest.log; false)

# Test target
# ===========
test: etests pytest cppcheck check-abi todo  ## Test everything

# Update abi target
# =================
update-abi: all  ## Update the ABI file
	cd "$(BUILD)" && abi-compliance-checker -lib chirp \
		-dump "$(BUILD)/abi-base.xml"
	cp $(BUILD)/abi_dumps/chirp/X/ABI.dump \
		$(BASE)/ci/abi_dumps/chirp/X/ABI.dump

# Check abi target
# ================
ifeq ($(CI_DISTRO),arch)
check-abi:
else
check-abi: $(BUILD)/abi_dumps/chirp/$(VERSION)/ABI.dump  ## Check the ABI
	cd "$(BUILD)" && abi-compliance-checker -lib chirp  \
		-old $(BASE)/ci/abi_dumps/chirp/X/ABI.dump \
		-new $(BUILD)/abi_dumps/chirp/$(VERSION)/ABI.dump
endif

# Rule to make abi dump
# =====================
$(BUILD)/abi_dumps/chirp/$(VERSION)/ABI.dump: libchirp.so
	cd "$(BUILD)" && abi-compliance-checker -lib chirp \
		-dump "$(BUILD)/abi-cur.xml"

# Pytest target
# =============
pytest: all  ## Run pytests
	pytest $(BASE)/src
ifneq ($(NO_MEMCHECK),True)
ifneq ($(TLS),openssl)
ifeq ($(CI_DISTRO),alpine)
	MPP_MC="$(BASE)/ci/memcheck.supp" pytest $(BASE)/src
endif
endif
endif

# cppcheck target
# ===============
cppcheck: all  ## Static analysis
	cppcheck -v \
		--enable=style,performance,portability \
		--suppress=unusedFunction \
		--suppress=*:*mpack_test.* \
		--suppress=*:*sds_test.* \
		--config-exclude="$(BASE)/src/mpack" \
		--error-exitcode=1 \
		--std=c99 \
		--inline-suppr \
		-I"$(BASE)/include" \
		-DCH_ACCEPT_STRANGE_PLATFORM \
		"$(BASE)/src"

# Unifdef
# =======
$(BUILD)/unifdef: $(BASE)/mk/unifdef.c
	$(V_E) CC $<
	$(V_M)$(CC) -o $@ $< $(NWCFLAGS) $(LDFLAGS) -Os

# Amalgamation
# ============
AMALB = $(BUILD)/libchirp
AMALS = $(BASE)/src
AMALI = $(BASE)/include
AMALIL = $(BASE)/include/libchirp

amalg: $(AMALB).c  ## Create amalgamation

$(AMALB).c: $(LIB_CFILES) $(HEADERS) $(BUILD)/unifdef
	$(V_E) GEN header.h
	$(V_M)cp -f $(BASE)/mk/lic-boilerplate $(BUILD)/header.h
	$(V_M)echo // ================================ >> $(BUILD)/header.h
	$(V_M)echo // libchirp $(VERSION_LONG) amalgamation >> $(BUILD)/header.h
	$(V_M)echo // ================================ >> $(BUILD)/header.h
	$(V_M)echo >> $(BUILD)/header.h
	$(V_E) AMAL libchirp.c
	$(V_M)echo '#include "libchirp.h"' >> $(AMALB).def.c
	$(V_M)cat \
		$(AMALS)/common.h \
		$(AMALS)/qs.h \
		$(AMALS)/rbtree.h \
		$(AMALS)/message.h \
		$(AMALS)/util.h \
		$(AMALS)/protocol.h \
		$(AMALS)/encryption.h \
		$(AMALS)/remote.h \
		$(AMALS)/serializer.h \
		$(AMALS)/writer.h \
		$(AMALS)/buffer.h \
		$(AMALS)/reader.h \
		$(AMALS)/connection.h \
		$(AMALS)/chirp.h \
		$(LIB_CFILES) > $(AMALB).def.c
ifeq ($(NO_UNIFDEF),True)
	$(V_M)cp -f $(AMALB).def.c $(AMALB).pre.c
else
	$(V_E) UNIFDEF libchirp.c
	$(V_M)$(BUILD)/unifdef -x 2 -DNDEBUG -o $(AMALB).pre.c $(AMALB).def.c
endif
	$(V_M)sed -E \
		's/(#include "[[:alnum:]./-]+.h")/\/* \1 *\//g' \
		< $(AMALB).pre.c >  $(AMALB).sed.c
	$(V_M)cat $(BUILD)/header.h > $(AMALB).c
	$(V_M)echo '#include "libchirp.h"' >> $(AMALB).c
	$(V_M)echo '#include "libchirp-config.h"' >> $(AMALB).c
	$(V_M)echo >> $(AMALB).c
	$(V_M)cat $(AMALB).sed.c >> $(AMALB).c
	$(V_E) AMAL libchirp.h
	$(V_M)cat \
		$(AMALIL)/const.h \
		$(AMALIL)/error.h \
		$(AMALIL)/common.h \
		$(AMALIL)/callbacks.h \
		$(AMALIL)/message.h \
		$(AMALIL)/wrappers.h \
		$(AMALIL)/chirp.h \
		$(AMALIL)/encryption.h \
		$(AMALI)/libchirp.h \
		> $(AMALB).def.h
ifeq ($(NO_UNIFDEF),True)
	cp $(AMALB).def.h $(AMALB).pre.h
else
	$(V_E) UNIFDEF libchirp.h
	$(V_M)$(BUILD)/unifdef -x 2 -DNDEBUG -o $(AMALB).pre.h $(AMALB).def.h
endif
	$(V_M)sed -E \
		's/(#include "[[:alnum:]./]+.h")/\/* \1 *\//g' \
		< $(AMALB).pre.h >  $(AMALB).sed.h
	$(V_M)cat $(BUILD)/header.h > $(AMALB).h
	$(V_M)cat $(AMALB).sed.h >> $(AMALB).h
	$(V_M)rm -f *.rg.* *.def.* *.sed.* *.pre.* header.h

# Distribution
# ============
DISTD=$(BUILD)/dist
DISTM=$(DISTD)/Makefile
DISTR=$(DISTD)/README.rst
DISTK=$(DISTD)/.keys

dist: $(DISTR)  ## Create source distribution

$(DISTR): $(AMALB).c
	$(V_E) DIST $(DISTD)
	$(V_M)mkdir -p $(DISTD)
	$(V_M)cp $(AMALB).h $(DISTD)
	$(V_M)cp $(AMALB).c $(DISTD)
ifeq ($(NO_UNIFDEF),True)
	$(V_M)cp $(BUILD)/libchirp-config.h $(DISTD)
else
	$(V_E) UNIFDEF libchirp-config.h
	$(V_M)$(BUILD)/unifdef -x 2 -DCH_REMOVE_DEBUGDEFS \
		-o $(DISTD)/libchirp-config.h $(BUILD)/libchirp-config.h
endif
	$(V_M)cp -r $(BASE)/LICENSE $(DISTD)
	$(V_M)cp $(BASE)/src/chirp_etest.c $(DISTD)/chirp_test.c
	$(V_M)cp $(BASE)/src/echo_etest.c $(DISTD)/echo_test.c
	$(V_M)cp $(BASE)/src/send_etest.c $(DISTD)/send_test.c
	$(V_M)echo 'UNAME_S := $$(shell uname -s)' > $(DISTM)
	$(V_M)echo VERSION := $(VERSION) >> $(DISTM)
	$(V_M)echo MAJOR := $(MAJOR) >> $(DISTM)
	$(V_M)echo >> $(DISTM)
	$(V_M)cat $(BASE)/mk/base-flags.mk >> $(DISTM)
	$(V_M)cat $(BASE)/mk/rel.mk >> $(DISTM)
	$(V_M)cat $(BASE)/mk/dist.mk >> $(DISTM)
	$(V_M)echo =================== > $(DISTR)
	$(V_M)echo libchirp $(VERSION_LONG) >> $(DISTR)
	$(V_M)echo =================== >> $(DISTR)
	$(V_M)cat $(BASE)/mk/DIST-README.rst >> $(DISTR)
	$(V_E) KEYS
	$(V_M)mkdir -p $(DISTK)
	$(V_M)cat $(BASE)/mk/dh.pem | tr 'a' '%' > $(DISTK)/dh.pem
	$(V_M)cat $(BASE)/mk/cert.pem | tr 'a' '%' > $(DISTK)/cert.pem

# Utility targets
# ===============
todo:  ## Show todos
	@grep -Inrs ".. todo" $(BASE)/src; true
	@grep -Inrs TODO $(BASE)/src; true

help:  ## Display this help
	@cat $(MAKEFILE_LIST) | grep -E '^[0-9a-zA-Z_.-]+:.*?## .*$$' | sort -k1,1 | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
	@echo
	@echo 'Known variables: VERBOSE=True, MACRO_DEBUG=True'

# Format target
# =============
format:
	CLANG_FORMAT=True $(MAKE) clean doc_files tutorial_files tutorial_code  ## Format code
