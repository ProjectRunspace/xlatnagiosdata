CXXFLAGS = -Wall -Wextra -pedantic -std=c++20 -c -fno-exceptions -fno-rtti
DEBUG ?= 0
ifeq ($(DEBUG), 1)
	CXXFLAGS +=-g3 -DDEBUG
else
	CXXFLAGS +=-g0 -DNDEBUG -O3
endif

CXX = g++ $(CXXFLAGS)
LDFLAGS = -Wall
LD = g++ $(LDFLAGS)

PACKAGE := xlatnagiosdata
MAKEFILE_DIRECTORY :=$(patsubst %/,%,$(dir $(realpath $(lastword $(MAKEFILE_LIST)))))
MAKEFILE_DIRECTORY_SHORTNAME := $(notdir $(MAKEFILE_DIRECTORY))
DAEMON_EXECUTABLE := $(PACKAGE)d
DAEMON_CONFIG_FILE := $(DAEMON_EXECUTABLE).toml
DAEMON_SERVICE_FILE := $(DAEMON_EXECUTABLE).service
SOURCE_DAEMON_DIR := ./daemon
SOURCE_DAEMON_CONFIG_DIR := $(SOURCE_DAEMON_DIR)/config
SOURCE_DAEMON_SOURCE_DIR := $(SOURCE_DAEMON_DIR)/source
BUILD_DIR := ./build

SOURCE_EXT = cpp
OBJECT_EXT = o
SOURCES := $(wildcard $(SOURCE_DAEMON_SOURCE_DIR)/*.cpp)
OBJECTS := $(patsubst $(SOURCE_DAEMON_SOURCE_DIR)/%,$(BUILD_DIR)/%,$(SOURCES:.$(SOURCE_EXT)=.$(OBJECT_EXT)))
LIBRARIES = -lcurl

INSTALL_CONFIG_DIR = /etc/$(PACKAGE)/
INSTALL_EXECUTABLE_DIR = /usr/local/bin/
INSTALL_SERVICE_DIR = /etc/systemd/system/
BUILDSTAT = stat $(OUT) 2>/dev/null | grep Modify
INSTALLEDSTAT = stat $(INSTALLDIR)$(OUT) 2>dev/null /grep Modify
TARBALL = $(PACKAGE).tar.gz

help:
	@echo make options for $(PACKAGE):
	@echo
	@echo "make all:                 creates build directory if it does not exist, builds"
	@echo "                          daemon executable. ignores unchanged source files"
	@echo
	@echo "make clean:               deletes build directory and daemon executable"
	@echo
	@echo "make clean-intermediate:  deletes build directory, leaves daemon executable"
	@echo
	@echo "make build_directories:   creates the build directory"
	@echo
	@echo "sudo make install:        installs the daemon -- use make all first -- and"
	@echo "                          configuration files. starts daemon"
	@echo
	@echo "sudo make reinstall:      uninstalls and reinstalls, ignores log and configuration"
	@echo
	@echo "sudo make purge:          uninstalls daemon, removes configuration and log files"
	@echo
	@echo "make rebuild:             deletes any previous builds, creates the build"
	@echo "                          directory, builds the daemon"
	@echo
	@echo "sudo make uninstall:      stops and removes daemon, deletes executable, ignores"
	@echo "                          log and configuration files"
	@echo
	@echo "make tar:                 creates a tarball of the project"

all: build_directories $(DAEMON_EXECUTABLE)

$(DAEMON_EXECUTABLE): $(OBJECTS)
	$(LD) -o $@ $^ $(LIBRARIES)

$(BUILD_DIR)/%.$(OBJECT_EXT): $(SOURCE_DAEMON_SOURCE_DIR)/%.$(SOURCE_EXT)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean: clean-intermediate
	rm -f $(DAEMON_EXECUTABLE)

clean-intermediate:
	rm -rf $(BUILD_DIR)

build_directories:
	mkdir -p $(BUILD_DIR)

install:
	@if [ ! -d "$(INSTALL_CONFIG_DIR)" ];\
	then\
		echo mkdir -p $(INSTALL_CONFIG_DIR);\
		mkdir -p $(INSTALL_CONFIG_DIR);\
	fi

	@if [ ! -d "$(INSTALL_CONFIG_DIR)$(DAEMON_CONFIG_FILE)" ];\
	then\
		echo cp -p $(SOURCE_DAEMON_CONFIG_DIR)/$(DAEMON_CONFIG_FILE) $(INSTALL_CONFIG_DIR);\
		cp -p $(SOURCE_DAEMON_CONFIG_DIR)/$(DAEMON_CONFIG_FILE) $(INSTALL_CONFIG_DIR);\
	fi

	cp -p $(SOURCE_DAEMON_CONFIG_DIR)/$(DAEMON_SERVICE_FILE) $(INSTALL_SERVICE_DIR)
	-cp -p $(DAEMON_EXECUTABLE) $(INSTALL_EXECUTABLE_DIR)
	systemctl daemon-reload
	systemctl enable $(DAEMON_EXECUTABLE)
	systemctl start $(DAEMON_EXECUTABLE)
	journalctl -u $(DAEMON_EXECUTABLE)
	@echo
	@echo Check journalctl output for successful start.
	@echo Modify main nagios.cfg file:
	@echo "   process_performance_data=1"
	@echo "   host_perfdata_file_template=\$$TIMET\$$\\\\t\$$HOSTNAME\$$\\\\t\$$HOSTCHECKCOMMAND\$$\\\\t\$$HOSTPERFDATA\$$"
	@echo "   service_perfdata_file_template=\$$TIMET\$$\\\\t\$$HOSTNAME\$$\\\\t\$$SERVICEDESC\$$\\\\t\$$SERVICEPERFDATA\$$"
	@echo "   host_perfdata_file_processing_command=process-host-perfdata-$(PACKAGE)"
	@echo "   service_perfdata_file_processing_command=process-service-perfdata-$(PACKAGE)"
	@echo
	@echo Create rules in a .cfg file in /usr/local/nagios/etc/objects:
	@echo "  define command {"
	@echo "    command_name process-host-perfdata-$(PACKAGE)"
	@echo "    command_line mv /usr/local/nagios/var/host-perfdata /var/spool/$(PACKAGE)/\$$TIMET\$$.perfdata.host"
	@echo "  }"
	@echo
	@echo "  define command {"
	@echo "    command_name process-service-perfdata-$(PACKAGE)"
	@echo "    command_line mv /usr/local/nagios/var/service-perfdata /var/spool/$(PACKAGE)/\$$TIMET\$$.perfdata.service"
	@echo "  }"
	@echo
	@echo To generate data, mark hosts and services as "process_perf_data 1"
	@echo Check and restart the nagios service to apply changes.

purge: uninstall
	-rm -rf $(INSTALL_CONFIG_DIR)
	-rm -rf /var/log/$(PACKAGE)

rebuild: clean build_directories $(DAEMON_EXECUTABLE)

reinstall: uninstall install

uninstall:
	-systemctl stop $(DAEMON_EXECUTABLE)
	-systemctl disable $(DAEMON_EXECUTABLE)
	-rm $(INSTALL_SERVICE_DIR)$(DAEMON_SERVICE_FILE)
	-systemctl daemon-reload
	-rm $(INSTALL_EXECUTABLE_DIR)$(DAEMON_EXECUTABLE)

tar:
	@echo $(MAKEFILE_DIRECTORY)
	@echo $(MAKEFILE_DIRECTORY_SHORTNAME)

	rm -f $(TARBALL)
	tar zcvf $(TARBALL) -C $(MAKEFILE_DIRECTORY)/.. $(MAKEFILE_DIRECTORY_SHORTNAME)/daemon $(MAKEFILE_DIRECTORY_SHORTNAME)/LICENSE $(MAKEFILE_DIRECTORY_SHORTNAME)/makefile $(MAKEFILE_DIRECTORY_SHORTNAME)/README.md

.PHONY: all build_directories clean clean-intermediate install rebuild uninstall tar
