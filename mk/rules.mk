.PHONY += doc

BN = $(basename $(@))
CLFORMAT_EXPECT=version 4.
CLFORMAT_VERSION=$(shell clang-format -version 2> /dev/null)

# Make .o form .c files
# =====================
$(BUILD)/%.o: $(BASE)/%.c
	@mkdir -p "$(dir $@)"
ifeq ($(MACRO_DEBUG),True)
	$(V_E) MDCC $<
	$(V_M)$(CC) $(CFLAGS) -E -P $< | clang-format > $(BN).f.c
	$(V_M)mv $(BN).f.c $(BN).c
	$(V_M)$(CC) -c -o $@ $(BN).c $(NWCFLAGS) \
			2> $@.log || \
		(cat $@.log; false)
else
	$(V_E) CC $<
	$(V_M)$(CC) -c -o $@ $< $(CFLAGS)
endif

# Make doc (.rst) from source files
# =================================
$(BUILD)/%.rst: $(BASE)/%
	@mkdir -p "$(dir $@)"
	$(V_E) TWSP $<
	$(V_M)$(BASE)/mk/twsp $<
ifneq (,$(findstring $(CLFORMAT_EXPECT),$(CLFORMAT_VERSION)))
	$(V_E) FRMT $<
	$(V_M)clang-format $< > $@.cf
ifeq ($(CLANG_FORMAT),True)
	$(V_M)mv $@.cf $<
else
	$(V_M)diff $< $@.cf > /dev/null || \
		(echo $<:1:1: Please clang-format the file; false)
endif
else
ifeq ($(IS_ALPINE_CI),True)
	@echo Wrong clang-format version. Please check CLFORMAT_EXPECT.
	@false
endif
endif
	$(V_E) RST $<
	$(V_M)$(BASE)/mk/c2rst $< $@

# Make lib (.a) files
# ===================
$(BUILD)/%.a:
	$(V_E) AR $@
	$(V_M)ar $(ARFLAGS) $@ $+ > /dev/null 2> /dev/null
ifeq ($(STRIP),True)
	$(V_E) STRIP $@
	$(V_M)$(STRPCMD) $@
endif

# Make shared objects (.so) files
# ===============================
$(BUILD)/%.so:
	$(V_E) LD $@
	$(V_M)$(CC) -shared -o $@ $+ $(LDFLAGS)
ifeq ($(STRIP),True)
	$(V_E) STRIP $@
	$(V_M)$(STRPCMD) $@
endif

# Make test binares (*_etest)
# ===========================
$(BUILD)/%_etest: $(BUILD)/%_etest.o libchirp_test.a libchirp.a
ifeq ($(VERBOSE),True)
	@if [ "$@" = "$(BUILD)/src/chirp_etest" ]; then \
		LIBCHIRP="-L$(BUILD) -lchirp"; \
	else \
		LIBCHIRP="$(BUILD)/libchirp.a"; \
	fi; \
	echo $(CC) -o $@ $< $(BUILD)/libchirp_test.a $$LIBCHIRP $(LDFLAGS); \
	$(CC) -o $@ $< $(BUILD)/libchirp_test.a $$LIBCHIRP $(LDFLAGS)
ifeq ($(STRIP),True)
	$(STRPCMD) $@
endif
else
	@echo LD $@
	@if [ "$@" = "$(BUILD)/src/chirp_etest" ]; then \
		LIBCHIRP="-L$(BUILD) -lchirp"; \
	else \
		LIBCHIRP="$(BUILD)/libchirp.a"; \
	fi; \
	$(CC) -o $@ $< $(BUILD)/libchirp_test.a $$LIBCHIRP $(LDFLAGS)
ifeq ($(STRIP),True)
	@echo STRIP $@
	@$(STRPCMD) $@
endif
endif
