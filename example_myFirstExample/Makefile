# Attempt to load a config.make file.
# If none is found, project defaults to a standard OF linux/msys2 build.
ifneq ($(wildcard config.make),)
	include config.make
endif

ifndef OF_ROOT
	OF_ROOT=$(realpath ../../..)
endif

include $(OF_ROOT)/libs/openFrameworksCompiled/project/makefileCommon/compile.project.mk
