# FmLibPlug — cmake/ctest shortcuts (make help)
#
#   make build      → artefacts under build/FmLibPlug_artefacts/$(CONFIG)/
#   make install    → VST3/CLAP/LV2 (+ AU on macOS) into OS default folders
#   make plugins    → build && install
#
# Standalone stays in the build tree. See docs/PLATFORM.md for Windows/Linux.

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

SCRIPTS := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/scripts)
UNAME_S := $(shell uname -s 2>/dev/null)
ifeq ($(UNAME_S),Darwin)
  FMLIB_OS := macos
  AU_DIR   ?= $(HOME)/Library/Audio/Plug-Ins/Components
  VST3_DIR ?= $(HOME)/Library/Audio/Plug-Ins/VST3
  CLAP_DIR ?= $(HOME)/Library/Audio/Plug-Ins/CLAP
  LV2_DIR  ?= $(HOME)/Library/Audio/Plug-Ins/LV2
  INSTALL_AU ?= 1
else ifeq ($(UNAME_S),Linux)
  FMLIB_OS := linux
  AU_DIR   ?=
  VST3_DIR ?= $(HOME)/.vst3
  CLAP_DIR ?= $(HOME)/.clap
  LV2_DIR  ?= $(HOME)/.lv2
  INSTALL_AU ?= 0
else ifneq (,$(filter MINGW% MSYS% CYGWIN%,$(UNAME_S)))
  # Git Bash / MSYS — mirror PowerShell defaults (shell handles spaces in Program Files).
  FMLIB_OS := windows
  AU_DIR   ?=
  VST3_DIR ?= $(shell if [ -n "$$COMMONPROGRAMFILES" ]; then printf '%s\n' "$$COMMONPROGRAMFILES/VST3"; elif [ -n "$$PROGRAMFILES" ]; then printf '%s\n' "$$PROGRAMFILES/Common Files/VST3"; else printf '%s\n' "$$HOME/.vst3"; fi)
  CLAP_DIR ?= $(shell if [ -n "$$COMMONPROGRAMFILES" ]; then printf '%s\n' "$$COMMONPROGRAMFILES/CLAP"; elif [ -n "$$PROGRAMFILES" ]; then printf '%s\n' "$$PROGRAMFILES/Common Files/CLAP"; else printf '%s\n' "$$HOME/.clap"; fi)
  LV2_DIR  ?= $(shell if [ -n "$$APPDATA" ]; then printf '%s\n' "$$APPDATA/LV2"; else printf '%s\n' "$$HOME/AppData/Roaming/LV2"; fi)
  INSTALL_AU ?= 0
else
  FMLIB_OS := other
  AU_DIR   ?=
  VST3_DIR ?= $(HOME)/.vst3
  CLAP_DIR ?= $(HOME)/.clap
  LV2_DIR  ?= $(HOME)/.lv2
  INSTALL_AU ?= 0
endif

ARTEFACT_ROOT := $(BUILD_DIR)/FmLibPlug_artefacts/$(CONFIG)

.PHONY: help configure configure-release ensure-configure build rebuild \
	standalone vst3 au auv3 lv2 clap shared plugins \
	force-relink-clean verify-plugins verify-one \
	install \
	touch-artefact-bundles show-config write-build-id \
	check check-smoke check-all check-hw check-hw-list \
	fixtures clean print-artefacts write-stamp

help:
	@echo "FmLibPlug make targets (OS=$(FMLIB_OS), BUILD_DIR=$(BUILD_DIR), CONFIG=$(CONFIG))"
	@echo ""
	@echo "  Primary"
	@echo "    make build               Build Standalone+VST3+LV2+CLAP (+ AU on macOS) [no install]"
	@echo "    make install             Copy plugins into OS default folders (see docs/PLATFORM.md)"
	@echo "    make plugins             make build && make install"
	@echo ""
	@echo "  Configure"
	@echo "    make configure / configure-release"
	@echo "    make standalone|vst3|au|lv2|clap"
	@echo "    make shared              Shared code library only (not host-loadable)"
	@echo "    make verify-plugins / print-artefacts / show-config"
	@echo ""
	@echo "  Tests: make check | check-smoke | check-all | check-hw | check-hw-list | fixtures"
	@echo "  Other: make clean | rebuild"
	@echo ""
	@echo "Build artefacts:  $(ARTEFACT_ROOT)/"
	@echo "Install targets ($(FMLIB_OS)):"
	@if [ "$(INSTALL_AU)" = "1" ]; then echo "  AU    $(AU_DIR)/FmLibPlug.component"; fi
	@echo "  VST3  $(VST3_DIR)/FmLibPlug.vst3"
	@echo "  CLAP  $(CLAP_DIR)/FmLibPlug.clap"
	@echo "  LV2   $(LV2_DIR)/FmLibPlug.lv2"
	@echo "Standalone stays in the build tree (not installed)."
	@echo "Windows without Make: scripts/install-plugins.ps1 (see docs/PLATFORM.md)."

show-config:
	@echo "FMLIB_OS=$(FMLIB_OS)"
	@echo "BUILD_DIR=$(BUILD_DIR)"
	@echo "BUILD_TYPE=$(BUILD_TYPE)"
	@echo "CONFIG=$(CONFIG) (from CMakeCache or BUILD_TYPE)"
	@echo "ARTEFACT_ROOT=$(ARTEFACT_ROOT)"
	@echo "FORCE_RELINK=$(FORCE_RELINK)"
	@echo "INSTALL_AU=$(INSTALL_AU)"
	@echo "VST3_DIR=$(VST3_DIR)"
	@echo "CLAP_DIR=$(CLAP_DIR)"
	@echo "LV2_DIR=$(LV2_DIR)"
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
		echo "Removing format products to force relink against SharedCode..."; \
		rm -rf "$(ARTEFACT_ROOT)/Standalone/FmLibPlug.app" \
			"$(ARTEFACT_ROOT)/Standalone/FmLibPlug" \
			"$(ARTEFACT_ROOT)/Standalone/FmLibPlug.exe" \
			"$(ARTEFACT_ROOT)/VST3/FmLibPlug.vst3" \
			"$(ARTEFACT_ROOT)/AU/FmLibPlug.component" \
			"$(ARTEFACT_ROOT)/LV2/FmLibPlug.lv2" \
			"$(ARTEFACT_ROOT)/CLAP/FmLibPlug.clap"; \
	fi

write-stamp:
	@mkdir -p "$(ARTEFACT_ROOT)"; \
	. "$(SCRIPTS)/fmlib-paths.sh"; \
	stamp="$(ARTEFACT_ROOT)/BUILD_STAMP.txt"; \
	shared=$$(fmlib_shared_lib "$(ARTEFACT_ROOT)" || true); \
	{ \
	  echo "version=1.2.0"; \
	  echo "config=$(CONFIG)"; \
	  echo "os=$(FMLIB_OS)"; \
	  echo "built_utc=$$(date -u +%Y-%m-%dT%H:%M:%SZ)"; \
	  echo "git=$$(git -C . rev-parse --short HEAD 2>/dev/null || echo unknown)"; \
	  echo "shared_mtime=$$(fmlib_mtime_human "$${shared:-}")"; \
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
	@$(MAKE) verify-one KIND=Standalone

vst3: ensure-configure force-relink-clean
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlug -j$(JOBS)
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlug_VST3 -j$(JOBS)
	@$(MAKE) verify-one KIND=VST3

au: ensure-configure force-relink-clean
	@if [ "$(INSTALL_AU)" != "1" ]; then echo "AU is Apple-only."; exit 1; fi
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlug -j$(JOBS)
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlug_AU -j$(JOBS)
	@$(MAKE) verify-one KIND=AU

auv3:
	@echo "AUv3 is opt-in and unavailable on many CLI toolchains."
	@echo "Use: make au  (macOS only)"
	@exit 1

lv2: ensure-configure force-relink-clean
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlug -j$(JOBS)
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlug_LV2 -j$(JOBS)
	@$(MAKE) verify-one KIND=LV2

clap: ensure-configure force-relink-clean
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlug -j$(JOBS)
	$(CMAKE) --build $(BUILD_DIR) --target FmLibPlug_CLAP -j$(JOBS)
	@$(MAKE) verify-one KIND=CLAP

touch-artefact-bundles:
	@root="$(ARTEFACT_ROOT)"; \
	for p in \
		"$$root/Standalone/FmLibPlug.app" \
		"$$root/Standalone/FmLibPlug" \
		"$$root/Standalone/FmLibPlug.exe" \
		"$$root/VST3/FmLibPlug.vst3" \
		"$$root/AU/FmLibPlug.component" \
		"$$root/LV2/FmLibPlug.lv2" \
		"$$root/CLAP/FmLibPlug.clap"; do \
		if [ -e "$$p" ]; then touch -c "$$p" 2>/dev/null || touch "$$p"; fi; \
	done

verify-one:
	@. "$(SCRIPTS)/fmlib-paths.sh"; \
	root="$(ARTEFACT_ROOT)"; \
	case "$(KIND)" in \
		Standalone) \
			prod="$$root/Standalone/FmLibPlug.app"; \
			[ -e "$$prod" ] || prod="$$root/Standalone"; \
			;; \
		VST3) prod="$$root/VST3/FmLibPlug.vst3" ;; \
		AU) prod="$$root/AU/FmLibPlug.component" ;; \
		LV2) prod="$$root/LV2/FmLibPlug.lv2" ;; \
		CLAP) prod="$$root/CLAP/FmLibPlug.clap" ;; \
		*) echo "verify-one: set KIND=Standalone|VST3|AU|LV2|CLAP"; exit 1 ;; \
	esac; \
	bin=$$(fmlib_find_binary "$$prod") || { echo "MISSING/EMPTY under $$prod"; exit 1; }; \
	shared=$$(fmlib_shared_lib "$$root") || true; \
	if [ -n "$$shared" ] && [ -e "$$shared" ]; then \
	  st=$$(fmlib_mtime "$$shared"); bt=$$(fmlib_mtime "$$bin"); \
	  if [ "$$bt" -lt "$$st" ]; then \
	    echo "STALE: $$bin is older than SharedCode — rerun make build"; \
	    exit 1; \
	  fi; \
	fi; \
	echo "OK: $$bin"

verify-plugins:
	@. "$(SCRIPTS)/fmlib-paths.sh"; \
	root="$(ARTEFACT_ROOT)"; \
	err=0; \
	echo "Verifying plugins under $$root (os=$(FMLIB_OS))"; \
	if [ ! -d "$$root" ]; then \
		echo "ERROR: artefact root missing: $$root"; \
		echo "Hint: BUILD_TYPE/CONFIG mismatch? e.g. make build BUILD_TYPE=Release"; \
		exit 1; \
	fi; \
	shared=$$(fmlib_shared_lib "$$root") || { echo "MISSING SharedCode under $$root"; exit 1; }; \
	st=$$(fmlib_mtime "$$shared"); \
	echo "SharedCode mtime: $$(fmlib_mtime_human "$$shared")"; \
	check_kind() { \
		label="$$1"; prod="$$2"; required="$$3"; \
		bin=$$(fmlib_find_binary "$$prod" 2>/dev/null || true); \
		if [ -z "$$bin" ]; then \
			if [ "$$required" = "1" ]; then echo "MISSING: $$label ($$prod)"; err=1; \
			else echo "SKIP:    $$label (not built)"; fi; \
			return; \
		fi; \
		bt=$$(fmlib_mtime "$$bin"); \
		if [ "$$bt" -lt "$$st" ]; then \
			echo "STALE:   $$bin (older than SharedCode)"; err=1; \
		else \
			echo "OK:      $$bin"; \
		fi; \
	}; \
	sa="$$root/Standalone/FmLibPlug.app"; \
	[ -e "$$sa" ] || sa="$$root/Standalone"; \
	check_kind Standalone "$$sa" 1; \
	check_kind VST3 "$$root/VST3/FmLibPlug.vst3" 1; \
	check_kind AU "$$root/AU/FmLibPlug.component" "$(INSTALL_AU)"; \
	check_kind LV2 "$$root/LV2/FmLibPlug.lv2" 1; \
	check_kind CLAP "$$root/CLAP/FmLibPlug.clap" 1; \
	exit $$err

print-artefacts:
	@echo "Artefact root: $(ARTEFACT_ROOT)"; \
	if [ -f "$(ARTEFACT_ROOT)/BUILD_STAMP.txt" ]; then echo "---- BUILD_STAMP ----"; cat "$(ARTEFACT_ROOT)/BUILD_STAMP.txt"; echo "--------------------"; fi; \
	du -sh \
		"$(ARTEFACT_ROOT)/Standalone/FmLibPlug.app" \
		"$(ARTEFACT_ROOT)/Standalone/FmLibPlug" \
		"$(ARTEFACT_ROOT)/Standalone/FmLibPlug.exe" \
		"$(ARTEFACT_ROOT)/VST3/FmLibPlug.vst3" \
		"$(ARTEFACT_ROOT)/AU/FmLibPlug.component" \
		"$(ARTEFACT_ROOT)/LV2/FmLibPlug.lv2" \
		"$(ARTEFACT_ROOT)/CLAP/FmLibPlug.clap" \
		2>/dev/null || true

# Install host formats only (not Standalone). Portable across macOS/Linux (+ Git Bash).
install: verify-plugins
	@AU_DIR="$(AU_DIR)" VST3_DIR="$(VST3_DIR)" CLAP_DIR="$(CLAP_DIR)" LV2_DIR="$(LV2_DIR)" INSTALL_AU="$(INSTALL_AU)" \
		"$(SCRIPTS)/install-plugins.sh" "$(ARTEFACT_ROOT)"

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
