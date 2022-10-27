/*
 * gst_ge2d_flip.c
 *
 *  Created on: 2020年11月3日
 *      Author: tao
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gst/allocators/gstdrmallocator.h>
#include <gst/gstdrmbufferpool.h>
#include "gst_ge2d_flip.h"

enum
{
    PROP_0,
    PROP_METHOD,
    PROP_SECURE,
    PROP_OUT_WIDTH,
    PROP_OUT_HEIGHT
};
#define ALIGN_PAD(x, y) (((x) + ((y)-1)) & (~((y)-1)))

GST_DEBUG_CATEGORY_STATIC(gst_ge2d_flip_debug);
#define GST_CAT_DEFAULT gst_ge2d_flip_debug

#define VIDEO_CAPS "{ NV12, NV21 }"

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE("sink",
                                                                   GST_PAD_SINK,
                                                                   GST_PAD_ALWAYS,
                                                                   GST_STATIC_CAPS(
                                                                       GST_VIDEO_CAPS_MAKE(VIDEO_CAPS)));

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE("src",
                                                                  GST_PAD_SRC,
                                                                  GST_PAD_SOMETIMES,
                                                                  GST_STATIC_CAPS(
                                                                      GST_VIDEO_CAPS_MAKE(VIDEO_CAPS)));

#define gst_ge2d_flip_parent_class parent_class
G_DEFINE_TYPE(GstGe2dFlip, gst_ge2d_flip, GST_TYPE_BASE_TRANSFORM);

#define GST_TYPE_GE2D_FLIP_METHOD (gst_ge2d_flip_method_get_type())

static const GEnumValue ge2d_flip_methods[] = {
  {GST_VIDEO_ORIENTATION_IDENTITY, "Identity (no rotation)", "none"},
  {GST_VIDEO_ORIENTATION_90R, "Rotate clockwise 90 degrees", "clockwise"},
  {GST_VIDEO_ORIENTATION_180, "Rotate 180 degrees", "rotate-180"},
  {GST_VIDEO_ORIENTATION_90L, "Rotate counter-clockwise 90 degrees",
      "counterclockwise"},
  {GST_VIDEO_ORIENTATION_HORIZ, "Flip horizontally", "horizontal-flip"},
  {GST_VIDEO_ORIENTATION_VERT, "Flip vertically", "vertical-flip"},
  {0, NULL, NULL},
};

static GType
gst_ge2d_flip_method_get_type(void)
{
    static GType ge2d_flip_method_type = 0;

    if (!ge2d_flip_method_type)
    {
        ge2d_flip_method_type = g_enum_register_static("GstGe2dFlipMethod",
                                                       ge2d_flip_methods);
    }
    return ge2d_flip_method_type;
}

static void
gst_ge2d_flip_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GstGe2dFlip *plugin = GST_GE2D_FLIP(object);

    switch (prop_id)
    {
    case PROP_METHOD:
        GST_OBJECT_LOCK(plugin);
        plugin->method = g_value_get_enum(value);
        GST_OBJECT_UNLOCK(plugin);
        break;
    case PROP_SECURE:
        GST_OBJECT_LOCK(plugin);
        plugin->secure = g_value_get_boolean(value);
        GST_OBJECT_UNLOCK(plugin);
        break;
    case PROP_OUT_WIDTH:
        GST_OBJECT_LOCK(plugin);
        plugin->width_set = g_value_get_uint(value);
        GST_OBJECT_UNLOCK(plugin);
        break;
    case PROP_OUT_HEIGHT:
        GST_OBJECT_LOCK(plugin);
        plugin->height_set = g_value_get_uint(value);
        GST_OBJECT_UNLOCK(plugin);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gst_ge2d_flip_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GstGe2dFlip *plugin = GST_GE2D_FLIP(object);

    switch (prop_id)
    {
    case PROP_METHOD:
        GST_OBJECT_LOCK(plugin);
        g_value_set_enum(value, plugin->method);
        GST_OBJECT_UNLOCK(plugin);
        break;
    case PROP_SECURE:
        GST_OBJECT_LOCK(plugin);
        g_value_set_boolean(value, plugin->secure);
        GST_OBJECT_UNLOCK(plugin);
        break;
    case PROP_OUT_WIDTH:
        GST_OBJECT_LOCK(plugin);
        g_value_set_uint(value, plugin->width_set);
        GST_OBJECT_UNLOCK(plugin);
        break;
    case PROP_OUT_HEIGHT:
        GST_OBJECT_LOCK(plugin);
        g_value_set_uint(value, plugin->height_set);
        GST_OBJECT_UNLOCK(plugin);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gboolean
gst_ge2d_flip_start(GstBaseTransform *trans)
{
    GstGe2dFlip *plugin = GST_GE2D_FLIP(trans);

    int ret = 0;
    plugin->pge2dinfo = &(plugin->amlge2d.ge2dinfo);

    memset(&plugin->amlge2d, 0, sizeof(aml_ge2d_t));

    /* ge2d init */
    ret = aml_ge2d_init(&plugin->amlge2d);
    if (ret < 0)
    {
        GST_ERROR("ge2d init failed");
        return FALSE;
    }
    return TRUE;
}

static gboolean
gst_ge2d_flip_stop(GstBaseTransform *trans)
{
    GstGe2dFlip *plugin = GST_GE2D_FLIP(trans);

    //ge2d release exit
    aml_ge2d_exit(&plugin->amlge2d);
    return TRUE;
}

static gboolean
gst_ge2d_flip_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps)
{
    GstGe2dFlip *plugin = GST_GE2D_FLIP(trans);
    if (!gst_video_info_from_caps(&plugin->in_info, incaps))
        return FALSE;
    if (!gst_video_info_from_caps(&plugin->out_info, outcaps))
        return FALSE;
    return TRUE;
}

static gboolean
gst_ge2d_flip_propose_allocation(GstBaseTransform *trans, GstQuery *decide_query,
                                 GstQuery *query)
{
    GstGe2dFlip *plugin = GST_GE2D_FLIP(trans);
    GstCaps *caps;
    GstBufferPool *pool = NULL;
    gboolean need_pool;

    gst_query_parse_allocation(query, &caps, &need_pool);

    if (need_pool)
    {
        pool = gst_drm_bufferpool_new(plugin->secure, GST_DRM_BUFFERPOOL_TYPE_VIDEO_PLANE);
    }

    gst_query_add_allocation_pool(query, pool, 0, 2, 0);
    if (pool)
        g_object_unref(pool);

    gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, NULL);
    return TRUE;
}

static gboolean
gst_ge2d_flip_decide_allocation(GstBaseTransform *trans, GstQuery *query)
{
    GstGe2dFlip *plugin = GST_GE2D_FLIP(trans);
    GstCaps *caps;
    GstBufferPool *pool = NULL;
    GstStructure *config;
    guint size, min, max;

    if (gst_query_get_n_allocation_pools(query) > 0)
    {
        gst_query_parse_nth_allocation_pool(query, 0, &pool, &size, &min, &max);
        if (pool)
            g_object_unref(pool);
    }
    gst_query_parse_allocation(query, &caps, NULL);

    pool = gst_drm_bufferpool_new(plugin->secure, GST_DRM_BUFFERPOOL_TYPE_VIDEO_PLANE);
    if (pool)
    {
        config = gst_buffer_pool_get_config(pool);
        gst_buffer_pool_config_set_params(config, caps, size, min, max);
        gst_buffer_pool_config_add_option(config, GST_BUFFER_POOL_OPTION_VIDEO_META);
        gst_buffer_pool_config_add_option(config, GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
        gst_buffer_pool_set_config(pool, config);
        gst_query_set_nth_allocation_pool(query, 0, pool, size, min, max);
        gst_object_unref(pool);
    }
    if (!gst_query_find_allocation_meta(query, GST_VIDEO_META_API_TYPE, NULL))
    {
        gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, NULL);
    };

    return TRUE;
}

static GstCaps *
gst_ge2d_flip_transform_caps(GstBaseTransform *trans, GstPadDirection direction,
                             GstCaps *caps, GstCaps *filter)
{
    GstGe2dFlip *plugin = GST_GE2D_FLIP(trans);
    GstCaps *ret = NULL;
    gint width, height, i;

    ret = gst_caps_copy(caps);

    GST_DEBUG_OBJECT(plugin, "transform_caps direction:%d caps:%" GST_PTR_FORMAT, direction, caps);

    for (i = 0; i < gst_caps_get_size(ret); i++)
    {
        GstStructure *structure = gst_caps_get_structure(ret, i);
        gint par_n, par_d, height_set, width_set;
        if (gst_structure_get_int(structure, "width", &width) &&
            gst_structure_get_int(structure, "height", &height))
        {
            if (direction == GST_PAD_SINK)
            {
                plugin->width_src = width;
                plugin->height_src = height;
                if (plugin->width_set == 0)
                {
                    width_set = width;
                } else {
                    width_set = plugin->width_set;
                }
                if (plugin->height_set == 0)
                {
                    height_set = height;
                } else {
                    height_set = plugin->height_set;
                }
            } else {
                switch (plugin->method)
                {
                case GST_VIDEO_ORIENTATION_90R:
                case GST_VIDEO_ORIENTATION_90L:
                    width_set = plugin->height_src;
                    height_set = plugin->width_src;
                    break;
                case GST_VIDEO_ORIENTATION_IDENTITY:
                case GST_VIDEO_ORIENTATION_180:
                case GST_VIDEO_ORIENTATION_HORIZ:
                case GST_VIDEO_ORIENTATION_VERT:
                    width_set = plugin->width_src;
                    height_set = plugin->height_src;
                    break;
                default:
                g_assert_not_reached();
                }
            }

            switch (plugin->method)
            {
            case GST_VIDEO_ORIENTATION_90R:
            case GST_VIDEO_ORIENTATION_90L:
                gst_structure_set(structure, "width", G_TYPE_INT, height_set,
                                  "height", G_TYPE_INT, width_set, NULL);
                if (gst_structure_get_fraction(structure, "pixel-aspect-ratio",
                                               &par_n, &par_d))
                {
                    if (par_n != 1 || par_d != 1)
                    {
                        GValue val = {
                            0,
                        };

                        g_value_init(&val, GST_TYPE_FRACTION);
                        gst_value_set_fraction(&val, par_d, par_n);
                        gst_structure_set_value(structure, "pixel-aspect-ratio", &val);
                        g_value_unset(&val);
                    }
                }
                break;
            case GST_VIDEO_ORIENTATION_IDENTITY:
                gst_structure_set(structure, "width", G_TYPE_INT, width_set,
                                  "height", G_TYPE_INT, height_set, NULL);
                gst_base_transform_set_passthrough(trans, TRUE);
                break;
            case GST_VIDEO_ORIENTATION_180:
            case GST_VIDEO_ORIENTATION_HORIZ:
            case GST_VIDEO_ORIENTATION_VERT:
                gst_structure_set(structure, "width", G_TYPE_INT, width_set,
                                  "height", G_TYPE_INT, height_set, NULL);

                break;
            default:
                g_assert_not_reached();
            }
        }
    }
    GST_DEBUG_OBJECT(plugin, "transformed %" GST_PTR_FORMAT " to %" GST_PTR_FORMAT, caps, ret);

    if (filter)
    {
        GstCaps *intersection;

        GST_DEBUG_OBJECT(plugin, "Using filter caps %" GST_PTR_FORMAT, filter);
        intersection =
            gst_caps_intersect_full(filter, ret, GST_CAPS_INTERSECT_FIRST);
        gst_caps_unref(ret);
        ret = intersection;
        GST_DEBUG_OBJECT(plugin, "Intersection %" GST_PTR_FORMAT, ret);
    }

    return ret;
}

static pixel_format_t pluginFormat4ge2d(GstVideoFormat pluginvideoFormat)
{
    pixel_format_t ge2dfomat;
    switch (pluginvideoFormat)
    {
    case GST_VIDEO_FORMAT_NV21:
    case GST_VIDEO_FORMAT_NV12:
        ge2dfomat = PIXEL_FORMAT_YCbCr_420_SP_NV12;
        break;
    default:
        ge2dfomat = -1;
        break;
    }
    return ge2dfomat;
}

static GE2D_ROTATION pluginOriMethod2ge2d(GstVideoOrientationMethod pluginRotation)
{
    GE2D_ROTATION ge2dratation;
    switch (pluginRotation)
    {
    case GST_VIDEO_ORIENTATION_IDENTITY:
        ge2dratation = GE2D_ROTATION_0;
        break;
    case GST_VIDEO_ORIENTATION_90R:
        ge2dratation = GE2D_ROTATION_90;
        break;
    case GST_VIDEO_ORIENTATION_180:
        ge2dratation = GE2D_ROTATION_180;
        break;
    case GST_VIDEO_ORIENTATION_90L:
        ge2dratation = GE2D_ROTATION_270;
        break;
    default:
        ge2dratation = GE2D_ROTATION_0;
        break;
    }
    return ge2dratation;
}

static GstFlowReturn
gst_ge2d_flip_transform(GstBaseTransform *trans, GstBuffer *inbuf, GstBuffer *outbuf)
{
    GstGe2dFlip *plugin = GST_GE2D_FLIP(trans);
    GstFlowReturn ret = GST_FLOW_ERROR;
    GstMemory *mem;
    guint inbuf_n, outbuf_n;
    int i;
    int plane = GST_VIDEO_INFO_N_PLANES(&plugin->in_info);
    GstVideoMeta *meta_data = NULL;

    aml_ge2d_info_t *pge2dinfo = plugin->pge2dinfo;

    inbuf_n = gst_buffer_n_memory(inbuf);
    outbuf_n = gst_buffer_n_memory(outbuf);

    g_return_val_if_fail(inbuf_n == plane, ret);
    g_return_val_if_fail(outbuf_n == plane, ret);

    for (i = 0; i < plane; i++)
    {
        mem = gst_buffer_peek_memory(inbuf, i);
        g_return_val_if_fail(gst_is_drm_memory(mem), ret);
        pge2dinfo->src_info[0].shared_fd[i] = gst_fd_memory_get_fd(mem); /* input memory fd array, related to xx.plane_number */

        mem = gst_buffer_peek_memory(outbuf, i);
        g_return_val_if_fail(gst_is_drm_memory(mem), ret);
        pge2dinfo->dst_info.shared_fd[i] = gst_fd_memory_get_fd(mem);
    }

    meta_data = gst_buffer_get_video_meta(inbuf);
//    pge2dinfo->mem_sec = plugin->secure ? 1 : 0;
    pge2dinfo->src_info[0].memtype = GE2D_CANVAS_ALLOC;
    pge2dinfo->src_info[0].mem_alloc_type = AML_GE2D_MEM_DMABUF;
    pge2dinfo->src_info[0].plane_number = plane;                             /* When allocating memory, it is a continuous block or separate multiple blocks */
    pge2dinfo->src_info[0].canvas_w = meta_data->stride[0];                  /* input width */
    pge2dinfo->src_info[0].canvas_h = meta_data->height;                     /* input height */
    pge2dinfo->src_info[0].format = pluginFormat4ge2d(GST_VIDEO_INFO_FORMAT(&plugin->in_info));
    pge2dinfo->src_info[0].rect.x = 0;                                       /* input process area x */
    pge2dinfo->src_info[0].rect.y = 0;                                       /* input process area y */
    pge2dinfo->src_info[0].rect.w = GST_VIDEO_INFO_WIDTH(&plugin->in_info);  /* input process area w */
    pge2dinfo->src_info[0].rect.h = GST_VIDEO_INFO_HEIGHT(&plugin->in_info); /* input process area h */
    pge2dinfo->src_info[0].plane_alpha = 0xFF;                               /* global plane alpha*/

    meta_data = gst_buffer_get_video_meta(outbuf);
    pge2dinfo->dst_info.memtype = GE2D_CANVAS_ALLOC;
    pge2dinfo->dst_info.mem_alloc_type = AML_GE2D_MEM_DMABUF;
    pge2dinfo->dst_info.plane_number = plane;                                /* When allocating memory, it is a continuous block or separate multiple blocks */
    pge2dinfo->dst_info.canvas_w = meta_data->stride[0];                     /* output width */
    pge2dinfo->dst_info.canvas_h = meta_data->height;                        /* output height */
    pge2dinfo->dst_info.format = pluginFormat4ge2d(GST_VIDEO_INFO_FORMAT(&plugin->out_info));
    pge2dinfo->dst_info.rect.x = 0;                                          /* output process area x */
    pge2dinfo->dst_info.rect.y = 0;                                          /* output process area y */
    pge2dinfo->dst_info.rect.w = GST_VIDEO_INFO_WIDTH(&plugin->out_info);    /* output process area w */
    pge2dinfo->dst_info.rect.h = GST_VIDEO_INFO_HEIGHT(&plugin->out_info);   /* output process area h */
    pge2dinfo->dst_info.plane_alpha = 0xFF;                                  /* global plane alpha*/

    pge2dinfo->dst_info.rotation = pluginOriMethod2ge2d(plugin->method);
    pge2dinfo->ge2d_op = AML_GE2D_STRETCHBLIT;

    if (pge2dinfo->src_info[0].format == -1 || pge2dinfo->dst_info.format == -1) {
        GST_ERROR("ge2d not support format src_info[0].format %d, dst_info.format %d",pge2dinfo->src_info[0].format, pge2dinfo->dst_info.format);
        goto beach;
    }
    //ge2d begin rotation
    GST_LOG_OBJECT(plugin, "pge2dinfo: in_rect_w %d in_rect_h %d canvas_w %d canvas_h %d out_rect_w %d out_rect_h %d canvas_w %d canvas_h %d" , \
                            pge2dinfo->src_info[0].rect.w, pge2dinfo->src_info[0].rect.h, \
                            pge2dinfo->src_info[0].canvas_w,pge2dinfo->src_info[0].canvas_h,\
                            pge2dinfo->dst_info.rect.w, pge2dinfo->dst_info.rect.h,\
                            pge2dinfo->dst_info.canvas_w,pge2dinfo->dst_info.canvas_h);
    ret = aml_ge2d_process(pge2dinfo);

    if (ret < 0) {
        GST_ERROR("ge2d process failed");
        goto beach;
    }

    ret = GST_FLOW_OK;
beach:
    return ret;
}

static void
gst_ge2d_flip_class_init(GstGe2dFlipClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    GstElementClass *element_class = (GstElementClass *)klass;
    GstBaseTransformClass *base_class = GST_BASE_TRANSFORM_CLASS(klass);

    GST_DEBUG_CATEGORY_INIT(gst_ge2d_flip_debug, "ge2d_flip", 0, "GE2D Flip");

    gobject_class->set_property = gst_ge2d_flip_set_property;
    gobject_class->get_property = gst_ge2d_flip_get_property;

    base_class->start = GST_DEBUG_FUNCPTR(gst_ge2d_flip_start);
    base_class->stop = GST_DEBUG_FUNCPTR(gst_ge2d_flip_stop);
    base_class->set_caps = GST_DEBUG_FUNCPTR(gst_ge2d_flip_set_caps);
    base_class->propose_allocation = GST_DEBUG_FUNCPTR(gst_ge2d_flip_propose_allocation);
    base_class->decide_allocation = GST_DEBUG_FUNCPTR(gst_ge2d_flip_decide_allocation);
    base_class->transform_caps = GST_DEBUG_FUNCPTR(gst_ge2d_flip_transform_caps);
    base_class->transform = GST_DEBUG_FUNCPTR(gst_ge2d_flip_transform);
    base_class->passthrough_on_same_caps = FALSE;
    base_class->transform_ip_on_passthrough = FALSE;

    gst_element_class_add_pad_template(element_class,
                                       gst_static_pad_template_get(&sinktemplate));
    gst_element_class_add_pad_template(element_class,
                                       gst_static_pad_template_get(&srctemplate));

    g_object_class_install_property(gobject_class, PROP_METHOD,
                                    g_param_spec_enum("method", "method",
                                                      "method (deprecated, use video-direction instead)",
                                                      GST_TYPE_GE2D_FLIP_METHOD, GST_VIDEO_ORIENTATION_IDENTITY,
                                                      GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                          G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_SECURE,
                                    g_param_spec_boolean("secure", "Use Secure",
                                                         "Use Secure DRM based memory for allocation",
                                                         FALSE, G_PARAM_WRITABLE));
    g_object_class_install_property (gobject_class, PROP_OUT_WIDTH,
                                    g_param_spec_uint ("out-width", "out-width",
                                                       "out width set can reduce the ge2d bandwidth pressure, 0 is not set ",
                                                       0, G_MAXUINT, 0,
                                                       G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
                                                       G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (gobject_class, PROP_OUT_HEIGHT,
                                    g_param_spec_uint ("out-height", "out-height",
                                                       "out height set can reduce the ge2d bandwidth pressure, 0 is not set ",
                                                       0, G_MAXUINT, 0,
                                                       G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
                                                       G_PARAM_STATIC_STRINGS));

    gst_element_class_set_details_simple(element_class,
                                         "Amlogic GE2D Plugin",
                                         "Filter/Effect/Video",
                                         "GE2D Plugin",
                                         "mm@amlogic.com");
}

static void
gst_ge2d_flip_init(GstGe2dFlip *plugin)
{
    GstBaseTransform *base = GST_BASE_TRANSFORM(plugin);
    gst_base_transform_set_passthrough(base, FALSE);
    gst_base_transform_set_in_place(base, FALSE);

    plugin->secure = FALSE;
}
