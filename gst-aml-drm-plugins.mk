#############################################################
#
# gat-aml-drm-plugins1
#
#############################################################
GST_AML_DRM_PLUGINS1_VERSION = 1.0
GST_AML_DRM_PLUGINS1_SITE = $(TOPDIR)/../multimedia/gst-aml-drm-plugins1/gst-aml-drm-plugins-1.0
GST_AML_DRM_PLUGINS1_SITE_METHOD = local

GST_AML_DRM_PLUGINS1_INSTALL_STAGING = YES
GST_AML_DRM_PLUGINS1_AUTORECONF = YES
GST_AML_DRM_PLUGINS1_DEPENDENCIES = gstreamer1 host-pkgconf

ifneq ($(BR2_PACKAGE_LIBSECMEM),y)
GST_AML_DRM_PLUGINS1_DEPENDENCIES  += libsecmem-bin
else
GST_AML_DRM_PLUGINS1_DEPENDENCIES  += libsecmem
endif

$(eval $(autotools-package))

