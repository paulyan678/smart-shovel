PYTHON ?= python3
PIO ?= pio
CLANG_FORMAT ?= clang-format
CPPCHECK ?= cppcheck

CPP_SOURCE_DIRS := src include lib/smart_shovel_core/include lib/smart_shovel_core/src test/test_core
SECRET_TOKEN_PATTERN := (AKIA|ASIA)[0-9A-Z]{16}|gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,}|xox[baprs]-[A-Za-z0-9-]{20,}|-----BEGIN (RSA|OPENSSH|EC|DSA|PGP) PRIVATE KEY-----
SECRET_ASSIGNMENT_PATTERN := (ssid|pass(word)?|wifi_(ssid|password)|api_?key|secret|token)[[:space:]]*(\[[^]]*\])?[[:space:]]*=[[:space:]]*"[^"]{4,}"

.DEFAULT_GOAL := help

.PHONY: help setup test-python calibration-check validate-events test-native build-firmware
.PHONY: format format-check static-check secret-check verify clean

help:
	@printf '%s\n' \
		'make setup              Install pinned Python development dependencies' \
		'make test-python        Run calibration-tool unit tests' \
		'make calibration-check  Recompute the preserved calibration fit' \
		'make validate-events EVENTS=path/to/events.csv  Validate an SD export' \
		'make test-native        Build and run host-side Unity tests' \
		'make build-firmware     Compile for the Arduino Nano RP2040 Connect' \
		'make format-check       Check maintained C/C++ formatting' \
		'make static-check       Run practical C++ static analysis' \
		'make secret-check       Scan the tracked current tree for credential patterns' \
		'make verify             Run every non-hardware quality gate'

setup:
	$(PYTHON) -m pip install --disable-pip-version-check -r requirements-dev.txt

test-python:
	$(PYTHON) -m unittest discover -s tools/tests -v

calibration-check:
	$(PYTHON) tools/analyze_calibration.py \
		--input calibration/data/raw/calib1.csv \
		--hac-lags 5

validate-events:
	@test -n "$(EVENTS)" || { echo 'Set EVENTS=path/to/events.csv'; exit 2; }
	$(PYTHON) tools/validate_events.py "$(EVENTS)"

test-native:
	$(PIO) test -e native

build-firmware:
	$(PIO) run -e nanorp2040connect
	$(PYTHON) tools/check_firmware_size.py \
		--image .pio/build/nanorp2040connect/firmware.bin \
		--maximum-bytes 2097152

format:
	@files="$$(find $(CPP_SOURCE_DIRS) -type f \
		\( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.h' \
		-o -name '*.hh' -o -name '*.hpp' -o -name '*.ino' \) -print)"; \
	if [ -z "$$files" ]; then echo 'No maintained C/C++ files found.'; exit 1; fi; \
	$(CLANG_FORMAT) -i $$files

format-check:
	@files="$$(find $(CPP_SOURCE_DIRS) -type f \
		\( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.h' \
		-o -name '*.hh' -o -name '*.hpp' -o -name '*.ino' \) -print)"; \
	if [ -z "$$files" ]; then echo 'No maintained C/C++ files found.'; exit 1; fi; \
	$(CLANG_FORMAT) --dry-run --Werror $$files

static-check:
	$(CPPCHECK) --language=c++ --std=c++17 \
		--enable=warning,performance,portability \
		--error-exitcode=1 --inline-suppr --suppress=missingIncludeSystem \
		--template=gcc -Ilib/smart_shovel_core/include $(CPP_SOURCE_DIRS)

secret-check:
	@if git ls-files | grep -Eq '(^|/)secrets\.hpp$$'; then \
		echo 'Tracked secrets.hpp file detected; keep local credentials untracked.'; \
		exit 1; \
	fi
	@if git grep -nEI '$(SECRET_TOKEN_PATTERN)' -- .; then \
		echo 'Potential credential token or private key detected.'; \
		exit 1; \
	fi
	@if git grep -nEI '$(SECRET_ASSIGNMENT_PATTERN)' -- . \
		':(exclude,glob)**/secrets.example.hpp'; then \
		echo 'Potential hard-coded credential assignment detected.'; \
		exit 1; \
	fi
	@echo 'Secret-pattern scan passed.'

verify: test-python calibration-check secret-check format-check static-check test-native build-firmware

clean:
	$(PIO) run --target clean
	rm -rf .pio
