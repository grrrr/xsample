# Makefile for Pure data pd-lib-builder

NAME=xsample
cflags=-DFLEXT_INLINE -DFLEXT_ATTRIBUTES=1

# source files
$(NAME).class.sources = $(wildcard source/*.cpp)

# help files
datafiles = $(wildcard pd/*-help.pd)

# include Makefile.pdlibbuilder from submodule directory 'pd-lib-builder'
PDLIBBUILDER_DIR=./pd-lib-builder/
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder
