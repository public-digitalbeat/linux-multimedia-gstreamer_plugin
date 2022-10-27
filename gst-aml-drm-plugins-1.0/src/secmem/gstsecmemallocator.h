/*
 * gstsecmemallocator.h
 *
 *  Created on: Feb 8, 2020
 *      Author: tao
 */


#ifndef __GST_SECMEMALLOCATOR_H__
#define __GST_SECMEMALLOCATOR_H__

#include <stdint.h>
#include <gst/gst.h>
#include <gst/allocators/allocators.h>
#include <secmem_types.h>

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_SECMEM_ALLOCATOR \
  (gst_secmem_allocator_get_type())
#define GST_SECMEM_ALLOCATOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SECMEM_ALLOCATOR,GstSecmemAllocator))
#define GST_SECMEM_ALLOCATOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SECMEM_ALLOCATOR,GstSecmemAllocatorClass))
#define GST_IS_SECMEM_ALLOCATOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SECMEM_ALLOCATOR))
#define GST_IS_SECMEM_ALLOCATOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SECMEM_ALLOCATOR))

typedef struct _GstSecmemAllocator      GstSecmemAllocator;
typedef struct _GstSecmemAllocatorClass GstSecmemAllocatorClass;
typedef uint32_t secmem_handle_t;
typedef uint32_t secmem_paddr_t;

#define GST_ALLOCATOR_SECMEM "secmem"
#define GST_CAPS_FEATURE_MEMORY_SECMEM_MEMORY "memory:SecMem"

struct _GstSecmemAllocator
{
    GstDmaBufAllocator      parent;
    void                   *sess;
    gboolean                is_4k;
    gboolean                is_vp9;
    gboolean                is_av1;
    GCond                   cond;
    GMutex                  mutex;
};

struct _GstSecmemAllocatorClass
{
    GstDmaBufAllocatorClass parent_class;
};

/*Sec decoder definition */
enum {
    SECMEM_DECODER_DEFAULT                       = 0,
    SECMEM_DECODER_VP9,
    SECMEM_DECODER_AV1,
    SECMEM_DECODER_AUDIO,
    SECMEM_MAX_CODEC_NUM,
};

GType           gst_secmem_allocator_get_type (void);
GstAllocator *  gst_secmem_allocator_new (gboolean is_4k, uint8_t decoder_format);
gboolean        gst_is_secmem_memory (GstMemory *mem);
gboolean        gst_secmem_fill(GstMemory *mem, uint32_t offset, uint8_t *buffer, uint32_t length);
gboolean        gst_secmem_store_csd(GstMemory *mem, uint8_t *buffer, uint32_t length);
gboolean        gst_secmem_prepend_csd(GstMemory *mem);
gboolean        gst_secmem_parse_avcc(GstMemory *mem, uint8_t *buffer, uint32_t length);
gboolean        gst_secmem_parse_avc2nalu(GstMemory *mem, uint32_t *flag);
gboolean        gst_secmem_parse_hvcc(GstMemory *mem, uint8_t *buffer, uint32_t length);
gboolean        gst_secmem_parse_hvc2nalu(GstMemory *mem, uint32_t *flag);
gboolean        gst_secmem_parse_vp9(GstMemory *mem);
gboolean        gst_secmem_parse_av1(GstMemory *mem);
gint            gst_secmem_check_free_buf_size(GstAllocator * allocator);
gint            gst_secmem_get_free_buf_num(GstMemory *mem);
gint            gst_secmem_get_free_buf_size(GstMemory *mem);
secmem_handle_t gst_secmem_memory_get_handle (GstMemory *mem);
secmem_paddr_t  gst_secmem_memory_get_paddr (GstMemory *mem);
secmem_handle_t gst_buffer_get_secmem_handle(GstBuffer *buffer);
secmem_paddr_t  gst_buffer_get_secmem_paddr(GstBuffer *buffer);
gboolean        gst_buffer_copy_to_secmem(GstBuffer *dst, GstBuffer *src);
gboolean        gst_buffer_sideband_secmem(GstBuffer *dst);


G_END_DECLS

#endif /* __GST_SECMEMALLOCATOR_H__ */
