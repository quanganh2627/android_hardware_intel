libomxil_utils_source_cpp = \
	list.c \
	queue.c \
	module.c \
	thread.cpp \
	workqueue.cpp \
	$(NULL)

libomxil_utils_source_h = \
	../inc/list.h \
        ../inc/module.h \
	../inc/queue.h \
	../inc/sysdeps.h \
	../inc/workqueue.h \
	../inc/thread.h \
	$(NULL)

libomxil_utils_source_priv_h = \
	$(NULL)
	
libomxil_utils_ldflags = \
	-version-info 0:5:0 \
	$(NULL)

lib_LTLIBRARIES			= libomxil_utils.la
libomxil_utilsincludedir	= ${includedir}/omx
libomxil_utilsinclude_HEADERS	= $(libomxil_utils_source_h)
noinst_HEADERS			= $(libomxil_utils_source_h_priv)
libomxil_utils_la_SOURCES	= $(libomxil_utils_source_cpp)
libomxil_utils_la_LDFLAGS	= $(libomxil_utils_ldflags)
libomxil_utils_la_CPPFLAGS      = -I../inc

DISTCLEANFILES = Makefile.in
