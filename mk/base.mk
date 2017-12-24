.PHONY += install uninstall testlibuv testopenssl clean tutorial_files tutorial_code format

# Verbose mode
# ============
ifeq ($(VERBOSE),True)
	V_M:=
	V_E:=@true
else
	V_M:=@
	V_E:=@echo
endif

include $(BASE)/mk/base-flags.mk

# Include flags
# ==============
CFLAGS += \
	-I"$(BASE)/include" \
	-I"$(BUILD)/src" \
	-I"$(BUILD)"


# Override targets
# ================
$(BUILD)/src/mpack_test.o: CFLAGS=$(NWCFLAGS)
$(BUILD)/src/rbtree.h.rst: $(BASE)/src/rbtree.h
	@mkdir -p "$(dir $@)"
	$(V_E) RST $<
	$(V_M)$(BASE)/mk/c2rst $< $@
$(BUILD)/src/mpipe_test.h.rst: $(BASE)/src/mpipe_test.h
	@mkdir -p "$(dir $@)"
	$(V_E) RST $<
	$(V_M)$(BASE)/mk/c2rst $< $@
$(BUILD)/src/qs.h.rst: $(BASE)/src/qs.h
	@mkdir -p "$(dir $@)"
	$(V_E) RST $<
	$(V_M)$(BASE)/mk/c2rst $< $@
$(BUILD)/src/mpipe_test.c.rst:
	@true
$(BUILD)/src/mpack_test.h.rst:
	@true
$(BUILD)/src/mpack_test.c.rst:
	@true
$(BUILD)/src/mpack-config.h.rst:
	@true
$(BUILD)/src/sds_test.c.rst:
	@true
$(BUILD)/src/sds_test.h.rst:
	@true


# Non Werror flags for external modules
# =====================================
# We do not enforce Werror, Wall and pedantic for external modules
# NOTE: DO NOT CHANGE CFLAGS AFTER THIS OR IN rules.mk, because
# this is not a lazy assignment
NWCFLAGS:=$(filter-out -Werror,$(CFLAGS))
NWCFLAGS:=$(filter-out -Wall,$(NWCFLAGS))
NWCFLAGS:=$(filter-out -Wextra,$(NWCFLAGS))
NWCFLAGS:=$(filter-out -pedantic,$(NWCFLAGS))

# Configure check targets
# =======================
testlibuv:
	@$(CC) -c -o "testlibuv.o" \
		$(CFLAGS) "$(BASE)/mk/testlibuv.c" \
		>> config.log 2>> config.log
	@rm testlibuv.o

testopenssl:
	@$(CC) -c -o "testopenssl.o" \
		$(CFLAGS) "$(BASE)/mk/testopenssl.c"\
		>> config.log 2>> config.log
	@rm testopenssl.o

# Library files targets
# =====================
libchirp.a: $(BUILD)/libchirp.a  ## Make libchirp.a
libchirp_test.a: $(BUILD)/libchirp_test.a  ## Make libchirp_test.a
libchirp.so: $(BUILD)/libchirp.so  ## Make libchirp.so

$(BUILD)/libchirp.a: $(LIB_OBJECTS)

$(BUILD)/libchirp_test.a: $(TEST_OBJECTS)

$(BUILD)/libchirp.so: $(LIB_OBJECTS)

# Check target
# ============
check: all  ## Check basic functionality
	LD_LIBRARY_PATH="$(BUILD)" $(BUILD)/src/chirp_etest
	$(BUILD)/src/quickcheck_etest
	$(BUILD)/src/serializer_etest
	$(BUILD)/src/message_etest --always-encrypt --message-count 3
	$(BUILD)/src/message_etest --message-count 3

# Format target
# =============
format:
	CLANG_FORMAT=True $(MAKE) clean doc_files tutorial_files tutorial_code  ## Format code

# Doc target
# ==========
ifeq ($(DOC),True)
doc: doc_files tutorial_files tutorial_code ## Generate documentation
	$(V_E) DOC
	$(V_M)mkdir -p $(BUILD)/doc
	$(V_M)mkdir -p $(BUILD)/doc/html
	$(V_M)rm -f $(BUILD)/doc/include
	$(V_M)rm -f $(BUILD)/doc/src
	$(V_M)ln -s $(BUILD)/include $(BUILD)/doc/include
	$(V_M)ln -s $(BUILD)/src $(BUILD)/doc/src
	$(V_M)cp $(BASE)/doc/*.py $(BUILD)/doc
	$(V_M)cp $(BASE)/doc/*.rst $(BUILD)/doc
ifeq ($(DEV),True)
	$(V_M)BASE=$(BASE) sphinx-build -b html $(BUILD)/doc $(BUILD)/doc/html 2>&1 \
		| grep -v intersphinx \
		| grep -v "ighlighting skipped" \
		| tee doc-gen.log > /dev/null
	@! grep -E "WARNING|ERROR" doc-gen.log
else # DEV
	$(V_M)rm -f $(BUILD)/doc/development.rst
	$(V_M)BASE=$(BASE) sphinx-build -b html $(BUILD)/doc $(BUILD)/doc/html
endif
else # DOC
doc:
	@echo Please reconfigure with ./configure --doc.; false
endif

# Tutorial code - extract docs
# ============================
tutorial_code: $(BUILD)/doc/tutorial/agent.c.rst $(BUILD)/doc/tutorial/monitor.c.rst $(BUILD)/doc/tutorial/collector.c.rst $(BUILD)/doc/tutorial/common.h.rst

# Copy tutorial doc files
# =======================
tutorial_files: $(wildcard $(BASE)/doc/tutorial/*.rst)
	$(V_E) CP tutorial
	$(V_M)mkdir -p $(BUILD)/doc
	$(V_M)rm -rf $(BUILD)/doc/tutorial/
	$(V_M)cp -r $(BASE)/doc/tutorial $(BUILD)/doc/

# Install target
# ==============
ifeq ($(DOC),True)
install: all doc
else
install: all  ## Install chirp
endif
	mkdir -p $(DEST)$(PREFIX)/lib
	cp -f $(BUILD)/libchirp.a $(DEST)$(PREFIX)/lib
	cp -f $(BUILD)/libchirp.so $(DEST)$(PREFIX)/lib/libchirp.so.$(VERSION)
	mkdir -p $(DEST)$(PREFIX)/include
	cp -f $(BASE)/include/libchirp.h $(DEST)$(PREFIX)/include/libchirp.h
	rm -rf $(DEST)$(PREFIX)/include/libchirp/
	cp -rf $(BASE)/include/libchirp/ $(DEST)$(PREFIX)/include/libchirp/
	cd $(DEST)$(PREFIX)/lib && ln -sf libchirp.so.$(VERSION) libchirp.so
	cd $(DEST)$(PREFIX)/lib && ln -sf libchirp.so.$(VERSION) libchirp.so.$(MAJOR)
ifeq ($(DOC),True)
	rm -rf $(DEST)$(PREFIX)/share/doc/chirp
	mkdir -p $(DEST)$(PREFIX)/share/doc/chirp
	cp -R $(BUILD)/doc/html/* $(DEST)$(PREFIX)/share/doc/chirp/
endif

# Utility targets
# ===============
uninstall:  ## Uninstall chirp
	rm -f $(DEST)$(PREFIX)/lib/libchirp.a
	rm -f $(DEST)$(PREFIX)/lib/libchirp.so.$(VERSION)
	rm -f $(DEST)$(PREFIX)/lib/libchirp.so
	rm -f $(DEST)$(PREFIX)/lib/libchirp.so.$(MAJOR)
	rm -f $(DEST)$(PREFIX)/include/libchirp.h
	rm -rf $(DEST)$(PREFIX)/include/libchirp/
	rm -rf $(DEST)$(PREFIX)/share/doc/chirp


# To check if the $(BUILD) variable is correct we added a token to Makefile in
# the build dir.
# If you need to change this, you probably are doing something wrong.
# Everything should be built in $(BUILD).
clean:  # Clean chirp
	@cd "$(BUILD)" && grep -q cf65e84fdbb7644a0c7725ebe6259490 Makefile
	$(V_E) Clean
	$(V_M)cd "$(BUILD)" && rm -rf abi_dumps/
	$(V_M)cd "$(BUILD)" && rm -rf compat_reports/
	$(V_M)cd "$(BUILD)" && rm -rf .hypothesis/
	$(V_M)cd "$(BUILD)" && rm -rf src/
	$(V_M)cd "$(BUILD)" && rm -rf include/
	$(V_M)cd "$(BUILD)" && rm -rf doc/
	$(V_M)cd "$(BUILD)" && rm -rf logs/
	$(V_M)cd "$(BUILD)" && rm -rf dist/
	$(V_M)cd "$(BUILD)" && rm -f *.a *.so
	$(V_M)cd "$(BUILD)" && rm -f unifdef
	$(V_M)cd "$(BUILD)" && \
		mv libchirp-config.h libchirp-config_h && \
		rm -f *.c *.h && \
		mv libchirp-config_h libchirp-config.h
	$(V_M)cd "$(BASE)" && rm -rf .cache
	$(V_M)cd "$(BASE)" && rm -rf __pycache__
	$(V_M)cd "$(BASE)" && rm -rf src/.cache
	$(V_M)cd "$(BASE)" && rm -rf src/__pycache__
