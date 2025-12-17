#
# Makefile
#
# $Id$
#
# Makefile for Poco Application
#

#POCO_BASE =../../../../../../poco-1.6.0
POCO_BASE =../../../poco-1.6.0
PROJECT_BASE = ./
POCO_BUILE = ./
include poco_app.make

ASFDIR  = ..
ASFINCS = -I$(ASFDIR)/include
ASFLIBS = -L$(ASFDIR)/lib/Linux/x86_64/ -lasfd

#all:
#	echo $(POCO_BASE)
#	echo $(ASFLIBS)


INCLUDE += $(ASFINCS)
LIBRARY += $(ASFLIBS)

objects = tbrmif

target         = tbrmif
target_version = 1
#target_libs    = test 
target_libs    = PocoJSON PocoFoundation 

include $(POCO_BASE)/build/rules/exec

