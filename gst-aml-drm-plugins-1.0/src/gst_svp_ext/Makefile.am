# Note: plugindir is set in configure

##############################################################################
# TODO: change libgstplugin.la to something else, e.g. libmysomething.la     #
##############################################################################
lib_LTLIBRARIES = libgstamlsvpext.la
plugin_LTLIBRARIES = libgstamlsvppayload.la

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

# sources used to compile this plug-in
libgstamlsvpext_la_SOURCES = gst_svp_meta.cpp gst_svp_logging.cpp \
							 gst_svp_scopedlock.cpp amlogic_svp_impl.cpp \
							 gst_svp_performance.cpp

# compiler and linker flags used to compile this plugin, set in configure.ac
# libgstamlsvpext
libgstamlsvpext_la_CXXFLAGS = $(GST_CFLAGS) \
							  -I$(top_srcdir)/src/secmem \
							  -I$(top_srcdir)/include \
							  -DGST_USE_UNSTABLE_API \
							  -Wno-attributes -Wall -g -fpermissive -std=c++1y -fPIC
libgstamlsvpext_la_LIBADD = $(GST_LIBS) \
							$(top_builddir)/src/secmem/libgstsecmemallocator.la
libgstamlsvpext_la_LDFLAGS = $(GST_LDFLAGS)
libgstamlsvpext_la_LIBTOOLFLAGS = --tag=disable-static

if ESSOS
libgstamlsvpext_la_CXXFLAGS += -DESSOS_RM
libgstamlsvpext_la_LIBADD += -lessosrmgr
endif

libgstamlsvpext_la_includedir = $(includedir)/gstreamer-1.0
libgstamlsvpext_la_include_HEADERS = gst_svp_meta.h sec_security_datatype.h

#libgstamlsvppayload
libgstamlsvppayload_la_SOURCES =  plugins/gst_svp_payload.cpp
libgstamlsvppayload_la_CXXFLAGS = $(GST_CFLAGS)\
								  -I$(top_srcdir)/src/secmem \
								  -I$(top_srcdir)/include \
								  -DGST_USE_UNSTABLE_API \
								  -Wno-attributes -Wall -g -fpermissive -std=c++1y -fPIC

libgstamlsvppayload_la_LIBADD = $(GST_LIBS) \
								$(top_builddir)/src/secmem/libgstsecmemallocator.la \
								libgstamlsvpext.la

libgstamlsvppayload_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) \
								 -Wl,-rpath=../../,-rpath=./ -L./

libgstamlsvppayload_la_LIBTOOLFLAGS = --tag=disable-static

include_HEADERS = gst_svp_meta.h GstPerf.h gst_svp_performance.h sec_security_datatype.h
