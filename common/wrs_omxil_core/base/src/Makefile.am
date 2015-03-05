libomxil_base_source_cpp = \
       	cmodule.cpp \
	componentbase.cpp \
	portbase.cpp \
	portvideo.cpp \
	$(NULL)

libomxil_base_source_h = \
        ../inc/cmodule.h \
	../inc/portbase.h \
	../inc/portvideo.h \
	../inc/componentbase.h \
	$(NULL)

libomxil_base_source_priv_h = \
	$(NULL)
	
libomxil_base_ldflags = \
	-version-info 0:5:0 \
	$(NULL)

lib_LTLIBRARIES			= libomxil_base.la
libomxil_baseincludedir		= ${includedir}/omx
libomxil_baseinclude_HEADERS	= $(libomxil_base_source_h)
noinst_HEADERS			= $(libomxil_base_source_h_priv)
libomxil_base_la_SOURCES	= $(libomxil_base_source_cpp)
libomxil_base_la_LDFLAGS	= $(libomxil_base_ldflags)
libomxil_base_la_CPPFLAGS       = -I../inc -I$(top_srcdir)/utils/inc -I$(top_srcdir)/ilcore/inc/khronos/openmax/include

DISTCLEANFILES = Makefile.in
