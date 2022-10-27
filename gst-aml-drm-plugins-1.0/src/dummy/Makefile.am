# Note: plugindir is set in configure

##############################################################################
# TODO: change libgstplugin.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libgstdummydrm.la

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

# sources used to compile this plug-in
libgstdummydrm_la_SOURCES = gstdummydrm.c

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstdummydrm_la_CFLAGS = $(GST_CFLAGS) \
	-I$(top_srcdir)/src/secmem \
	-I$(top_srcdir)/include
libgstdummydrm_la_LIBADD = $(GST_LIBS) \
	$(top_builddir)/src/secmem/libgstsecmemallocator.la
	
libgstdummydrm_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstdummydrm_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstdummydrm.h 
