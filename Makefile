# FmLibPlug — cmake/ctest shortcuts (make help)
#
#   make build      → artefacts under build/FmLibPlug_artefacts/$(CONFIG)/
#   make install    → AU/VST3/CLAP/LV2 → ~/Library/Audio/Plug-Ins/... (macOS)
#   make plugins    → build && install
#
# Standalone stays in the build tree.

BUILD_DIR   ?= build
JOBS        ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
CMAKE       ?= cmake
BUILD_TYPE  ?= Debug
# Prefer the configured CMake build type when present (avoids Debug build + Release install mismatch).
CONFIG      ?= $(shell if [ -f "$(BUILD_DIR)/CMakeCache.txt" ]; then \
		sed -n 's/^CMAKE_BUILD_TYPE:STRING=//p' "$(BUILD_DIR)/CMakeCache.txt" | head -1; \
	fi)
ifeq ($(strip $(CONFIG)),)
CONFIG := $(BUILD_TYPE)
endif

# Force format binaries to be deleted before linking so incremental builds cannot
# leave Standalone/AU/… stale while only SharedCode/VST3 refreshed. Set FORCE_RELINK=0 to skip.
FORCE_RELINK ?= 1

AU_DIR   ?= $(HOME)/Library/Audio/Plug-Ins/Components
VST3_DIR ?= $(HOME)/Library/Audio/Plug-Ins/VST3
CLAP_DIR ?= $(HOME)/Library/Audio/Plug-Ins/CLAP
LV2_DIR  ?= $(HOME)/Library/Audio/Plug-Ins/LV2

ARTEFACT_ROOT := $(BUILD_DIR)/FmLibPlug_artefacts/$(CONFIG)

.PHONY: help configure configure-release ensure-configure build rebuild \
	standalone vst3 au auv3 lv2 clap shared plugins \
	force-relink-clean verify-plugins verify-one \
	install \
	touch-artefact-bundles show-config write-build-id \
	check check-smoke check-all check-hw check-hw-list \
	fixtures clean print-artefacts write-stamp

help:
	@echo "FmLibPlug make targets (BUILD_DIR=$(BUILD_DIR), CONFIG=$(CONFIG))"
	@echo ""
	@echo "  Primary"
	@echo "    make build               Build Standalone+VST3+AU+LV2+CLAP (+ tests)  [no install]"
	@echo "    make install             Copy AU/VST3/CLAP/LV2 into ~/Library/Audio/Plug-Ins/...  (macOS)"
	@echo "    make plugins             make build && make install"
	@echo ""
	@echo "  Configure"
	@echo "    make configure / configure-release"
	@echo "    make standalone|vst3|au|lv2|clap"
	@echo "    make shared              Shared code .a only (not host-loadable)"
	@echo "    make verify-plugins / print-artefacts / show-config"
	@echo ""
	@echo "  Tests: make check | check-smoke | check-all | check-hw | check-hw-list | fixtures"
	@echo "  Other: make clean | rebuild"
	@echo ""
	@echo "Build artefacts:  $(ARTEFACT_ROOT)/"
	@echo "Install targets (macOS):"
	@echo "  AU    $(AU_DIR)/FmLibPlug.component"
	@echo "  VST3  $(VST3_DIR)/FmLibPlug.vst3"
	@echo "  CLAP  $(CLAP_DIR)/FmLibPlug.clap"
	@echo "  LV2   $(LV2_DIR)/FmLibPlug.lv2"
	@echo "Standalone stays in the build tree (not installed)."

show-config:
	@echo "BUILD_DIR=$(BUILD_DIR)"
	@echo "BUILD_TYPE=$(BUILD_TYPE)"
	@echo "CONFIG=$(CONFIG) (from CMakeCache or BUILD_TYPE)"
	@echo "ARTEFACT_ROOT=$(ARTEFACT_ROOT)"
	@echo "FORCE_RELINK=$(FORCE_RELINK)"
	@if [ -f "$(BUILD_DIR)/CMakeCache.txt" ]; then \
		echo "CMakeCache CMAKE_BUILD_TYPE=$$(sed -n 's/^CMAKE_BUILD_TYPE:STRING=//p' "$(BUILD_DIR)/CMakeCache.txt" | head -1)"; \
	fi

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DFMLIBPLUG_BUILD_TESTS=ON \
		-DFMLIBPLUG_BUILD_CLAP=ON

configure-release:
	$(MAKE) configure BUILD_TYPE=Release CONFIG=Release

ensure-configure:
	@if [ ! -f "$(BUILD_DIR)/CMakeCache.txt" ]; then \
		echo "No $(BUILD_DIR)/CMakeCache.txt — configuring ($(BUILD_TYPE))..."; \
		$(CMAKE) -S . -B $(BUILD_DIR) \
			-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
			-DFMLIBPLUG_BUILD_TESTS=ON \
			-DFMLIBPLUG_BUILD_CLAP=ON; \
	else \
		$(CMAKE) -S . -B $(BUILD_DIR) \
			-DFMLIBPLUG_BUILD_TESTS=ON \
			-DFMLIBPLUG_BUILD_CLAP=ON >/dev/null; \
	fi

force-relink-clean:
	@if [ "$(FORCE_RELINK)" = "1" ]; then \
		echo "Removing format binaries to force relink against SharedCode..."; \
		rm -f "$(ARTEFACT_ROOT)/Standalone/FmLibPlug.app/Contents/MacOS/FmLibPlug"; \
		rm -f "$(ARTEFACT_ROOT)/VST3/FmLibPlug.vst3/Contents/MacOS/FmLibPlug"; \
		rm -f "$(ARTEFACT_ROOT)/AU/FmLibPlug.component/Contents/MacOS/FmLibPlug"; \
		rm -f "$(ARTEFACT_ROOT)/LV2/FmLibPlug.lv2/libFmLibPlug.so"; \
		rm -f "$(ARTEFACT_ROOT)/CLAP/FmLibPlug.clap/Contents/MacOS/FmLibPlug"; \
	fi

write-stamp:
	@mkdir -p "$(ARTEFACT_ROOT)"; \
	stamp="$(ARTEFACT_ROOT)/BUILD_STAMP.txt"; \
	{ \
	  echo "version=1.0.0"; \
	  echo "config=$(CONFIG)"; \
	  echo "built_utc=$$(date -u +%Y-%m-%dT%H:%M:%SZ)"; \
	  echo "git=$$(git -C . rev-parse --short HEAD 2>/dev/null || echo unknown)"; \
	  echo "shared_mtime=$$(stat -f '%Sm' -t '%Y-%m-%d %H:%M:%S' "$(ARTEFACT_ROOT)/libFmLibPlug_SharedCode.a" 2>/dev/null || echo none)"; \
	} > "$$stamp"; \
	echo "Wrote $$stamp"; \
	cat "$$stamp"

# Build only — artefacts under ARTEFACT_ROOT, nothing copied to Plug-Ins.
build: ensure-configure write-build-id
	@$(MAKE) show-config
	@echo "==> Building SharedCode"
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlug -j$(JOBS)
	@$(MAKE) force-relink-clean CONFIG="$(CONFIG)"
	@echo "==> Linking all formats (FmLibPlug_Plugins)"
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlug_Plugins -j$(JOBS)
	@echo "==> Building remaining targets (tests/tools)"
	$(CMAKE) --build $(BUILD_DIR) -j$(JOBS)
	@$(MAKE) verify-plugins CONFIG="$(CONFIG)"
	@$(MAKE) touch-artefact-bundles CONFIG="$(CONFIG)"
	@$(MAKE) write-stamp CONFIG="$(CONFIG)"
	@$(MAKE) print-artefacts CONFIG="$(CONFIG)"
	@echo ""
	@echo "Build complete (not installed)."
	@echo "Artefacts: $(ARTEFACT_ROOT)/"
	@echo "UI build id is shown next to the version label — confirm it after loading in the DAW."
	@echo "Next:  make install    # or: make plugins  (= build + install)"

write-build-id:
	@mkdir -p "$(BUILD_DIR)/generated"; \
	stamp=$$(date -u +%Y%m%dT%H%M%SZ); \
	git=$$(git -C . rev-parse --short HEAD 2>/dev/null || echo nogit); \
	id="$$git-$$stamp"; \
	printf '%s\n' \
		'#include "BuildId.h"' \
		'namespace fmlib {' \
		'const char* buildId() { return "'$$id'"; }' \
		'}' > "$(BUILD_DIR)/generated/BuildId.cpp"; \
	echo "Build id: $$id"

# Convenience: build then install for hosts.
plugins: build install

rebuild: clean configure build

shared: ensure-configure
	@echo "NOTE: shared code only — not host-loadable. Use: make build  or  make plugins"
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlug -j$(JOBS)

standalone: ensure-configure force-relink-clean
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlug -j$(JOBS)
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlug_Standalone -j$(JOBS)
	@$(MAKE) verify-one BIN="$(ARTEFACT_ROOT)/Standalone/FmLibPlug.app/Contents/MacOS/FmLibPlug"

vst3: ensure-configure force-relink-clean
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlug -j$(JOBS)
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlug_VST3 -j$(JOBS)
	@$(MAKE) verify-one BIN="$(ARTEFACT_ROOT)/VST3/FmLibPlug.vst3/Contents/MacOS/FmLibPlug"

au: ensure-configure force-relink-clean
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlug -j$(JOBS)
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlug_AU -j$(JOBS)
	@$(MAKE) verify-one BIN="$(ARTEFACT_ROOT)/AU/FmLibPlug.component/Contents/MacOS/FmLibPlug"

auv3:
	@echo "AUv3 is opt-in and unavailable on many CLI toolchains."
	@echo "Use: make au"
	@exit 1

lv2: ensure-configure force-relink-clean
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlug -j$(JOBS)
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlug_LV2 -j$(JOBS)
	@$(MAKE) verify-one BIN="$(ARTEFACT_ROOT)/LV2/FmLibPlug.lv2/libFmLibPlug.so"

clap: ensure-configure force-relink-clean
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlug -j$(JOBS)
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlug_CLAP -j$(JOBS)
	@$(MAKE) verify-one BIN="$(ARTEFACT_ROOT)/CLAP/FmLibPlug.clap/Contents/MacOS/FmLibPlug"

touch-artefact-bundles:
	@root="$(ARTEFACT_ROOT)"; \
	for p in \
		"$$root/Standalone/FmLibPlug.app" \
		"$$root/VST3/FmLibPlug.vst3" \
		"$$root/AU/FmLibPlug.component" \
		"$$root/LV2/FmLibPlug.lv2" \
		"$$root/CLAP/FmLibPlug.clap"; do \
		if [ -e "$$p" ]; then touch -c "$$p" 2>/dev/null || touch "$$p"; fi; \
	done

verify-one:
	@shared="$(ARTEFACT_ROOT)/libFmLibPlug_SharedCode.a"; \
	bin="$(BIN)"; \
	if [ ! -e "$$bin" ] || [ ! -s "$$bin" ]; then echo "MISSING/EMPTY: $$bin"; exit 1; fi; \
	if [ -e "$$shared" ]; then \
	  st=$$(stat -f '%m' "$$shared"); bt=$$(stat -f '%m' "$$bin"); \
	  if [ "$$bt" -lt "$$st" ]; then \
	    echo "STALE: $$bin is older than SharedCode.a — rerun make build"; \
	    exit 1; \
	  fi; \
	fi; \
	echo "OK: $$bin"

verify-plugins:
	@root="$(ARTEFACT_ROOT)"; \
	shared="$$root/libFmLibPlug_SharedCode.a"; \
	err=0; \
	echo "Verifying plugins under $$root"; \
	if [ ! -d "$$root" ]; then \
		echo "ERROR: artefact root missing: $$root"; \
		echo "Hint: BUILD_TYPE/CONFIG mismatch? e.g. make build BUILD_TYPE=Release"; \
		exit 1; \
	fi; \
	if [ ! -e "$$shared" ]; then echo "MISSING SharedCode: $$shared"; exit 1; fi; \
	st=$$(stat -f '%m' "$$shared"); \
	echo "SharedCode.a mtime: $$(stat -f '%Sm' -t '%Y-%m-%d %H:%M:%S' "$$shared")"; \
	check() { \
		if [ ! -e "$$1" ]; then echo "MISSING: $$1"; err=1; \
		elif [ ! -s "$$1" ]; then echo "EMPTY:   $$1"; err=1; \
		else \
		  bt=$$(stat -f '%m' "$$1"); \
		  if [ "$$bt" -lt "$$st" ]; then \
		    echo "STALE:   $$1 (older than SharedCode.a)"; err=1; \
		  else \
		    echo "OK:      $$1 ($$(stat -f '%Sm %z bytes' -t '%H:%M:%S' "$$1"))"; \
		  fi; \
		fi; \
	}; \
	check "$$root/Standalone/FmLibPlug.app/Contents/MacOS/FmLibPlug"; \
	check "$$root/VST3/FmLibPlug.vst3/Contents/MacOS/FmLibPlug"; \
	check "$$root/AU/FmLibPlug.component/Contents/MacOS/FmLibPlug"; \
	check "$$root/LV2/FmLibPlug.lv2/libFmLibPlug.so"; \
	check "$$root/CLAP/FmLibPlug.clap/Contents/MacOS/FmLibPlug"; \
	exit $$err

print-artefacts:
	@echo "Artefact root: $(ARTEFACT_ROOT)"; \
	if [ -f "$(ARTEFACT_ROOT)/BUILD_STAMP.txt" ]; then echo "---- BUILD_STAMP ----"; cat "$(ARTEFACT_ROOT)/BUILD_STAMP.txt"; echo "--------------------"; fi; \
	du -sh \
		"$(ARTEFACT_ROOT)/Standalone/FmLibPlug.app" \
		"$(ARTEFACT_ROOT)/VST3/FmLibPlug.vst3" \
		"$(ARTEFACT_ROOT)/AU/FmLibPlug.component" \
		"$(ARTEFACT_ROOT)/LV2/FmLibPlug.lv2" \
		"$(ARTEFACT_ROOT)/CLAP/FmLibPlug.clap" \
		2>/dev/null || true

# Install host formats only (not Standalone).
install: verify-plugins
	@echo ""
	@echo "==== INSTALL ===="
	@echo "Source:  $(ARTEFACT_ROOT)/"
	@echo "Destinations:"
	@echo "  AU    -> $(AU_DIR)/FmLibPlug.component"
	@echo "  VST3  -> $(VST3_DIR)/FmLibPlug.vst3"
	@echo "  CLAP  -> $(CLAP_DIR)/FmLibPlug.clap"
	@echo "  LV2   -> $(LV2_DIR)/FmLibPlug.lv2"
	@echo "==============="
	@mkdir -p "$(AU_DIR)" "$(VST3_DIR)" "$(CLAP_DIR)" "$(LV2_DIR)"
	@rm -rf "$(AU_DIR)/FmLibPlug.component" \
		"$(VST3_DIR)/FmLibPlug.vst3" \
		"$(CLAP_DIR)/FmLibPlug.clap" \
		"$(LV2_DIR)/FmLibPlug.lv2"
	@if command -v ditto >/dev/null 2>&1; then \
		ditto "$(ARTEFACT_ROOT)/AU/FmLibPlug.component" "$(AU_DIR)/FmLibPlug.component"; \
		ditto "$(ARTEFACT_ROOT)/VST3/FmLibPlug.vst3" "$(VST3_DIR)/FmLibPlug.vst3"; \
		ditto "$(ARTEFACT_ROOT)/CLAP/FmLibPlug.clap" "$(CLAP_DIR)/FmLibPlug.clap"; \
		ditto "$(ARTEFACT_ROOT)/LV2/FmLibPlug.lv2" "$(LV2_DIR)/FmLibPlug.lv2"; \
	else \
		cp -R "$(ARTEFACT_ROOT)/AU/FmLibPlug.component" "$(AU_DIR)/"; \
		cp -R "$(ARTEFACT_ROOT)/VST3/FmLibPlug.vst3" "$(VST3_DIR)/"; \
		cp -R "$(ARTEFACT_ROOT)/CLAP/FmLibPlug.clap" "$(CLAP_DIR)/"; \
		cp -R "$(ARTEFACT_ROOT)/LV2/FmLibPlug.lv2" "$(LV2_DIR)/"; \
	fi
	@cp "$(ARTEFACT_ROOT)/BUILD_STAMP.txt" "$(VST3_DIR)/FmLibPlug.vst3/BUILD_STAMP.txt" 2>/dev/null || true
	@cp "$(ARTEFACT_ROOT)/BUILD_STAMP.txt" "$(AU_DIR)/FmLibPlug.component/BUILD_STAMP.txt" 2>/dev/null || true
	@echo "---- checksum verification ----"; \
	fail=0; \
	verify_copy() { \
		src="$$1"; dst="$$2"; label="$$3"; \
		if [ ! -e "$$src" ] || [ ! -e "$$dst" ]; then echo "MISSING $$label"; fail=1; return; fi; \
		ss=$$(shasum -a 256 "$$src" | awk '{print $$1}'); \
		ds=$$(shasum -a 256 "$$dst" | awk '{print $$1}'); \
		dm=$$(stat -f '%Sm' -t '%Y-%m-%d %H:%M:%S' "$$dst"); \
		if [ "$$ss" = "$$ds" ]; then \
			echo "OK  $$label  $$dm"; \
			echo "    $$dst"; \
		else \
			echo "MISMATCH $$label"; fail=1; \
		fi; \
	}; \
	verify_copy "$(ARTEFACT_ROOT)/AU/FmLibPlug.component/Contents/MacOS/FmLibPlug" \
		"$(AU_DIR)/FmLibPlug.component/Contents/MacOS/FmLibPlug" "AU"; \
	verify_copy "$(ARTEFACT_ROOT)/VST3/FmLibPlug.vst3/Contents/MacOS/FmLibPlug" \
		"$(VST3_DIR)/FmLibPlug.vst3/Contents/MacOS/FmLibPlug" "VST3"; \
	verify_copy "$(ARTEFACT_ROOT)/CLAP/FmLibPlug.clap/Contents/MacOS/FmLibPlug" \
		"$(CLAP_DIR)/FmLibPlug.clap/Contents/MacOS/FmLibPlug" "CLAP"; \
	verify_copy "$(ARTEFACT_ROOT)/LV2/FmLibPlug.lv2/libFmLibPlug.so" \
		"$(LV2_DIR)/FmLibPlug.lv2/libFmLibPlug.so" "LV2"; \
	if [ "$$fail" != "0" ]; then exit 1; fi
	@echo "Install complete. Quit the DAW fully (not just rescan) — hosts keep loaded plugins in memory."
	@echo "Confirm the UI version label matches the Build id printed by make build."
	@# macOS AU registration can cache the old component; nudge it.
	@rm -f "$(HOME)/Library/Caches/com.apple.audiounits.cache" 2>/dev/null || true
	@rm -rf "$(HOME)/Library/Caches/AudioUnitCache" 2>/dev/null || true
	@killall -9 AudioComponentRegistrar 2>/dev/null || true
	@# Ad-hoc sign installed bundles so Gatekeeper/hosts accept the replacement.
	@codesign --force --deep -s - "$(AU_DIR)/FmLibPlug.component" 2>/dev/null || true
	@codesign --force --deep -s - "$(VST3_DIR)/FmLibPlug.vst3" 2>/dev/null || true
	@codesign --force --deep -s - "$(CLAP_DIR)/FmLibPlug.clap" 2>/dev/null || true
	@# Bump CFBundleVersion so hosts notice a change even if they key off Info.plist.
	@buildver=$$(date -u +%Y%m%d%H%M%S); \
	for plist in \
		"$(AU_DIR)/FmLibPlug.component/Contents/Info.plist" \
		"$(VST3_DIR)/FmLibPlug.vst3/Contents/Info.plist" \
		"$(CLAP_DIR)/FmLibPlug.clap/Contents/Info.plist"; do \
		if [ -f "$$plist" ]; then \
			/usr/libexec/PlistBuddy -c "Set :CFBundleVersion $$buildver" "$$plist" 2>/dev/null \
				|| /usr/libexec/PlistBuddy -c "Add :CFBundleVersion string $$buildver" "$$plist" 2>/dev/null \
				|| true; \
		fi; \
	done
	@echo "Standalone (not installed): $(ARTEFACT_ROOT)/Standalone/FmLibPlug.app"
	@echo "  open \"$(ARTEFACT_ROOT)/Standalone/FmLibPlug.app\""

check: ensure-configure
	$(CMAKE) --build $(BUILD_DIR) --target check -j$(JOBS)

check-smoke: ensure-configure
	$(CMAKE) --build $(BUILD_DIR) --target check-smoke -j$(JOBS)

check-all: ensure-configure
	$(CMAKE) --build $(BUILD_DIR) --target check-all -j$(JOBS)

check-hw: ensure-configure
	$(CMAKE) --build $(BUILD_DIR) --target check-hw -j$(JOBS)

check-hw-list: ensure-configure
	$(CMAKE) --build $(BUILD_DIR) --target check-hw-list -j$(JOBS)

fixtures: ensure-configure
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlugFixtures -j$(JOBS)

clean:
	rm -rf $(BUILD_DIR)
