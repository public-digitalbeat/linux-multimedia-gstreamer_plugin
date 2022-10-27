# Note: plugindir is set in configure

##############################################################################
# TODO: change libgstplugin.la to something else, e.g. libmysomething.la     #
##############################################################################
lib_LTLIBRARIES = libgstsecmemallocator.la

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

# sources used to compile this plug-in
libgstsecmemallocator_la_SOURCES = gstsecmemallocator.c

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstsecmemallocator_la_CFLAGS = $(GST_CFLAGS) -I$(top_srcdir)/include
libgstsecmemallocator_la_LIBADD = $(GST_LIBS) -lsecmem
libgstsecmemallocator_la_LDFLAGS = $(GST_LDFLAGS)
libgstsecmemallocator_la_LIBTOOLFLAGS = --tag=disable-static

libgstsecmemallocator_includedir = $(includedir)/gstreamer-1.0/gst/allocators
libgstsecmemallocator_include_HEADERS = gstsecmemallocator.h