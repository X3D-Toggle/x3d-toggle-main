#!/bin/sh
## `Makefile`
## Distribution Model: FHS-compliant, Lean and Stateless Build.
## Standardizes compilation and strict headless file deployment.

.SHELLFLAGS = -ec

CC             		 = clang
CFLAGS        		?= -Wall -O2 -Wextra -D_GNU_SOURCE -fno-builtin -nostdlib -fno-stack-protector
USER_CFLAGS          = -DBPF_NO_PRESERVE_ACCESS_INDEX
LDFLAGS        		 = 
INCLUDES             = -Iinclude -Ibuild -Isrc/daemon -Isrc/cli -Isrc/daemon/polling -Isrc/daemon/bpf -DUSR_LIBS=\"$(USR_LIBS)\" -DVAR_LOGS=\"$(VAR_LOGS)\" -DVAR_AUDITS=\"$(VAR_AUDITS)\" -DVAR_DUMPS=\"$(VAR_DUMPS)\" -DDIR_BIN=\"$(DIR_BIN)\" -DDIR_RUN=\"$(DIR_RUN)\" 
LIBS          		 = -lbpf
CLI           		 = -DCLI_BUILD
BACKEND         	 = -DBACKEND

export USR       	?= /usr
export ETC			?= /etc
export VAR			?= /var
export LIB			?= /lib
export BIN			?= bin
export SRC			?= src
export RUN			?= /run

export USR_MAN1     ?= $(USR_MAN)/man1
export DIR_BIN		?= $(ETC)/x3d-toggle.d
export DIR_RUN		?= $(RUN)/x3d-toggle

export USR_BIN      ?= $(USR)/bin
export USR_SHARE    ?= $(USR)/share
export USR_LIB      ?= $(USR)/lib

export VAR_LIB      ?= $(VAR)/lib
export VAR_LOG      ?= $(VAR)/log/x3d-toggle

export LIB_UDEV     ?= $(USR_LIB)/udev
export LIB_SYSTEMD  ?= $(USR_LIB)/systemd
export LIB_POLKIT   ?= $(USR_LIB)/polkit-1
export SYS_SYSUSERS ?= $(USR_LIB)/sysusers.d
export SYS_TMPFILES ?= $(USR_LIB)/tmpfiles.d
export USR_LIBS     ?= $(USR_LIB)/x3d-toggle

export USR_PIXMAPS  ?= $(USR_SHARE)/pixmaps
export USR_APPS     ?= $(USR_SHARE)/applications
export USR_MAN		?= $(USR_SHARE)/man
export USR_ASSETS   ?= $(USR_SHARE)/x3d-toggle

export VAR_LIBS     ?= $(VAR_LIB)/x3d-toggle
export VAR_LOGS     ?= $(VAR_LOG)/logs
export VAR_AUDITS   ?= $(VAR_LOG)/audits
export VAR_DUMPS    ?= $(VAR_LOG)/coredumps

export SYS_UDEV     ?= $(LIB_UDEV)/rules.d
export SYS_SYSTEMD  ?= $(LIB_SYSTEMD)/system
export SYS_POLKIT   ?= $(LIB_POLKIT)/rules.d

export SRC_DAEMON   ?= $(SRC)/daemon
export SRC_CLI      ?= $(SRC)/cli
export SRC_IPC      ?= $(SRC)
export SRC_POLLING  ?= $(SRC_DAEMON)/polling
export SRC_BPF      ?= $(SRC_DAEMON)/bpf

NAME_CLI         = x3d-toggle
NAME_DAEMON      = x3d-daemon
NAME_WRAPPER     = x3d-run

DIR_BUILD        = build
TARGET_CLI       = $(BIN)/$(NAME_CLI)
TARGET_DAEMON    = $(BIN)/$(NAME_DAEMON)
TARGET_WRAPPER   = $(BIN)/$(NAME_WRAPPER)
TARGET_CCD       = $(DIR_BUILD)/ccd.c $(DIR_BUILD)/ccd.h
TARGET_UI        = $(DIR_BUILD)/xui.c $(DIR_BUILD)/xui.h
TARGET_FRAMEWORK = $(DIR_BUILD)/.framework_synced
TARGET_CONFIG    = $(DIR_BUILD)/config.h

SRCS_CORE       = $(DIR_BUILD)/xui.c \
                  $(DIR_BUILD)/ccd.c \
                  $(SRC)/libc.c \
                  $(SRC)/error.c \
                  $(SRC)/games.c \
                  $(SRC)/worker.c \
                  $(SRC)/systemd.c \
                  $(SRC_DAEMON)/config.c \
                  $(SRC_DAEMON)/modes.c \
                  $(SRC_DAEMON)/cppc.c \
                  $(SRC_DAEMON)/diag.c \
                  $(SRC)/affinity.c \
                  $(SRC)/stress.c \
                  $(SRC)/scheduler.c \
                  $(SRC_DAEMON)/daemon.c

SRCS_CLI        = $(SRC)/toggle.c \
                  $(SRC_CLI)/cli.c \
                  $(SRC_CLI)/misc.c \
                  $(SRCS_CORE)

SRCS_GUI        = $(SRC_CLI)/dialog.c \
                  $(SRC)/status.c

SRCS_DAEMON     = $(SRC)/socket.c \
                  $(SRC_POLLING)/polling.c \
                  $(SRC_BPF)/bpf-user.c \
                  $(SRC)/status.c \
                  $(SRCS_CORE)

DEST_POLLING    = $(DESTDIR)$(USR_LIBS)/polling
DEST_EBPF       = $(DESTDIR)$(USR_LIBS)/ebpf
DEST_IPC        = $(DESTDIR)$(USR_LIBS)/ipc
DEST_BIN        = $(DESTDIR)$(USR_BIN)
DEST_LIBS       = $(DESTDIR)$(USR_LIBS)
DEST_MAN        = $(DESTDIR)$(USR_MAN)
DEST_ETC        = $(DESTDIR)$(DIR_BIN)
DEST_VAR        = $(DESTDIR)$(VAR)
DEST_SYS        = $(DESTDIR)$(SYS)
DEST_SCRIPTS    = $(DESTDIR)$(USR_SHARE)/scripts
DEST_TOOLS      = $(DESTDIR)$(USR_LIBS)/scripts/tools
DEST_FRAMEWORK  = $(DESTDIR)$(USR_LIBS)/scripts/framework
DEST_LOGS       = $(DESTDIR)$(VAR_LOGS)
DEST_AUDITS     = $(DESTDIR)$(VAR_AUDITS)
DEST_DUMPS      = $(DESTDIR)$(VAR_DUMPS)
DEST_RUN        = $(DESTDIR)$(RUN)

DEST_SYSTEMD    = $(DESTDIR)$(SYS_SYSTEMD)
DEST_UDEV       = $(DESTDIR)$(SYS_UDEV)
DEST_POLKIT     = $(DESTDIR)$(SYS_POLKIT)
DEST_SYSUSERS   = $(DESTDIR)$(SYS_SYSUSERS)
DEST_TMPFILES   = $(DESTDIR)$(SYS_TMPFILES)
DEST_APPS       = $(DESTDIR)$(USR_APPS)
DEST_PIXMAPS    = $(DESTDIR)$(USR_PIXMAPS)

SRCS_WRAPPER    = $(SRC)/run.c $(SRC)/status.c $(SRCS_CORE)

.PHONY: all clean uninstall purge prep build install framework setup

all: build

$(TARGET_FRAMEWORK): framework
framework:
	X3D_FRAMEWORK=1 X3D_EXEC=1 sh ./scripts/framework/framework.sh --gen-all
	touch $(TARGET_FRAMEWORK)

	@echo ""
	@echo "==========================================================="
	@if [ -z "$(DESTDIR)" ]; then \
		echo "Local install detected. Run 'sudo make setup' to configure."; \
	else \
		echo "Packager install detected. User must configure via setup"; \
		echo "script post-install."; \
	fi
	@echo "==========================================================="
	@echo ""

$(TARGET_UI) $(TARGET_CCD) $(TARGET_CONFIG): $(TARGET_FRAMEWORK)
build: $(TARGET_CLI) $(TARGET_DAEMON) $(TARGET_WRAPPER)
	true

$(TARGET_CLI): $(SRCS_CLI) $(SRCS_GUI) $(TARGET_UI) $(TARGET_CCD) $(TARGET_FRAMEWORK)
	$(CC) $(CFLAGS) $(USER_CFLAGS) $(CLI) $(INCLUDES) $(SRCS_CLI) $(SRCS_GUI) -o $(TARGET_CLI) $(LIBS)

$(TARGET_DAEMON): $(SRCS_DAEMON) $(TARGET_UI) $(TARGET_CCD) $(TARGET_FRAMEWORK)
	$(CC) $(CFLAGS) $(USER_CFLAGS) $(BACKEND) $(INCLUDES) $(SRCS_DAEMON) -o $(TARGET_DAEMON) $(LIBS)

$(TARGET_WRAPPER): $(SRCS_WRAPPER) $(TARGET_UI) $(TARGET_CCD) $(TARGET_FRAMEWORK)
	$(CC) $(CFLAGS) $(USER_CFLAGS) $(BACKEND) $(CLI) $(INCLUDES) $(SRCS_WRAPPER) -o $(TARGET_WRAPPER) $(LIBS)

prep:
	mkdir -p bin $(DIR_BUILD)

install: build
	install -dm755 $(DEST_BIN)
	install -m755 bin/x3d-toggle $(DEST_BIN)/x3d-toggle
	install -m755 bin/x3d-daemon $(DEST_BIN)/x3d-daemon
	install -m755 bin/x3d-run $(DEST_BIN)/x3d-run
	ln -sf x3d-toggle $(DEST_BIN)/x3d

	install -dm755 $(DEST_LIBS)
	install -m644 $(DIR_BUILD)/bpf.o $(DEST_LIBS)/bpf.o
	install -m644 $(DIR_BUILD)/vmlinux.h $(DEST_LIBS)/vmlinux.h
	install -m644 $(DIR_BUILD)/xui.h $(DEST_LIBS)/xui.h
	install -m644 $(DIR_BUILD)/ccd.h $(DEST_LIBS)/ccd.h

	install -dm755 $(DEST_POLLING)
	install -m644 $(SRC_POLLING)/polling.c $(DEST_POLLING)/polling.c
	install -m644 $(SRC_POLLING)/polling.h $(DEST_POLLING)/polling.h

	install -dm755 $(DEST_IPC)
	install -m644 $(SRC_IPC)/socket.c $(DEST_IPC)/socket.c
	install -m644 $(SRC_IPC)/worker.c $(DEST_IPC)/worker.c
	install -m644 include/ipc.h $(DEST_IPC)/ipc.h

	install -dm755 $(DEST_ETC)
	install -m644 config/settings.conf $(DEST_ETC)/settings.conf
	install -m644 config/games.conf $(DEST_ETC)/games.conf

	install -dm775 $(DEST_LOGS)
	install -dm775 $(DEST_AUDITS)
	install -dm775 $(DEST_DUMPS)

	install -dm755 $(DEST_SYSTEMD)
	install -m644 packaging/x3d-toggle.service $(DEST_SYSTEMD)/x3d-toggle.service

	install -dm755 $(DEST_SYSUSERS)
	install -m644 packaging/sysusers.conf $(DEST_SYSUSERS)/x3d_toggle-sysusers.conf

	install -dm755 $(DEST_TMPFILES)
	install -m644 packaging/tmpfiles.conf $(DEST_TMPFILES)/x3d_toggle-tmpfiles.conf

	install -dm755 $(DEST_UDEV)
	install -m644 packaging/sysfs.rules $(DEST_UDEV)/99-x3d_toggle-sysfs.rules

	install -dm755 $(DEST_POLKIT)
	install -m644 packaging/50-x3d_toggle-service.rules $(DEST_POLKIT)/50-x3d_toggle-service.rules

	install -dm755 $(DEST_TOOLS)
	install -dm755 $(DEST_FRAMEWORK)
	install -dm755 $(DEST_TOOLS)
	install -m755 scripts/tools/*.sh $(DEST_TOOLS)/
	install -m755 scripts/framework/*.sh $(DEST_FRAMEWORK)/

	install -dm755 $(DEST_APPS)
	install -m644 packaging/x3d-toggle.desktop $(DEST_APPS)/x3d-toggle.desktop

	install -dm755 $(DEST_PIXMAPS)
	install -m644 assets/x3d-toggle.svg $(DEST_PIXMAPS)/x3d-toggle.svg

	-chown :x3d-toggle $(DEST_LOGS)
	-chown :x3d-toggle $(DEST_AUDITS)
	chmod 775 $(DEST_LOGS)
	chmod 775 $(DEST_AUDITS)

	-udevadm control --reload-rules && udevadm trigger
	-systemctl daemon-reload

setup:
	@if [ "$$(id -u)" -ne 0 ]; then echo "    ❌ Error: Run with sudo make setup"; exit 1; fi
	./setup.sh

clean:
	rm -rf bin/* build/* bin/.[!.]* build/.[!.]*

uninstall:
	-systemctl stop x3d-toggle.service
	-killall -9 x3d-daemon

	rm -f $(DEST_RUN)/x3d-toggle.ipc
	rm -f $(DEST_RUN)/x3d-toggle.pid
	rm -f $(DEST_RUN)/x3d-toggle.sock
	rm -f $(DEST_RUN)/x3d-toggle.lock
	rm -f $(DEST_RUN)/x3d-toggle.worker
	rm -f $(DEST_RUN)/x3d-toggle.log

	rm -f $(DEST_BIN)/x3d-toggle
	rm -f $(DEST_BIN)/x3d-daemon
	rm -f $(DEST_BIN)/x3d-run
	rm -f $(DEST_BIN)/x3d
	rm -f $(DEST_APPS)/x3d-toggle.desktop
	rm -f $(DEST_PIXMAPS)/x3d-toggle.svg
	rm -f $(DEST_SYSUSERS)/x3d_toggle-sysusers.conf
	rm -f $(DEST_TMPFILES)/x3d_toggle-tmpfiles.conf
	rm -f $(DEST_SYSTEMD)/x3d-toggle.service
	rm -f $(DEST_UDEV)/99-x3d_toggle-sysfs.rules
	rm -f $(DEST_POLKIT)/50-x3d_toggle-service.rules	
	rm -f $(DEST_UDEV)/99-x3d-toggle.rules
	rm -f $(DEST_POLKIT)/x3d-toggle.rules

	rm -rf $(DEST_LOGS)
	rm -rf $(DEST_AUDITS)
	rm -rf $(DEST_DUMPS)
	rm -rf $(DEST_LIBS)
	rm -rf $(DEST_ETC)
	rm -rf $(DEST_VAR)/lib/x3d-toggle

	rm -rf bin/* build/* bin/.[!.]* build/.[!.]*

	-udevadm control --reload-rules && udevadm trigger
	-systemctl daemon-reload

purge: clean uninstall

## end of MAKEFILE