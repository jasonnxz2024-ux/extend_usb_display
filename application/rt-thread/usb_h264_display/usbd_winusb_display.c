/*
 * Copyright (c) 2026, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "aic_core.h"
#include "aic_log.h"
#include "aic_usb_display_protocol.h"
#include "artinchip_fb.h"
#include "mpp_decoder.h"
#include "mpp_fb.h"
#include "usbd_core.h"

#define WCID_VENDOR_CODE                 0x17
#define USB_DISPLAY_MAX_POWER            100
#define USB_DISPLAY_LANGID               1033
#define USB_DISPLAY_CONFIG_SIZE          (9 + 9 + 7 + 7)
#define USB_DISPLAY_INTERFACE_COUNT      1
#define USB_DISPLAY_RX_SIZE              (60U * 1024U)
#define USB_DISPLAY_FRAME_BUFFER_COUNT   2U
#define USB_DISPLAY_WIDTH                1024U
#define USB_DISPLAY_HEIGHT               600U
#define USB_DISPLAY_DECODER_STACK_SIZE   (8U * 1024U)
#define USB_DISPLAY_DECODER_PRIORITY     20U

#ifdef LPKG_CHERRYUSB_WINUSB_DISPLAY_H264
#define USB_DISPLAY_ENCODE_TYPE          AIC_USB_DISPLAY_TYPE_H264
#define USB_DISPLAY_MAX_PAYLOAD_SIZE     AIC_USB_DISPLAY_MAX_H264_AU_SIZE
#define USB_DISPLAY_CODEC_TYPE           MPP_CODEC_VIDEO_DECODER_H264
#define USB_DISPLAY_CODEC_NAME           "H264"
#define USB_DISPLAY_THREAD_NAME          "usb_h264"
#else
#define USB_DISPLAY_ENCODE_TYPE          AIC_USB_DISPLAY_TYPE_JPEG
#define USB_DISPLAY_MAX_PAYLOAD_SIZE     AIC_USB_DISPLAY_MAX_JPEG_SIZE
#define USB_DISPLAY_CODEC_TYPE           MPP_CODEC_VIDEO_DECODER_MJPEG
#define USB_DISPLAY_CODEC_NAME           "JPEG"
#define USB_DISPLAY_THREAD_NAME          "usb_jpeg"
#endif

#ifdef CONFIG_USB_HS
#define USB_DISPLAY_EP_MPS               512
#else
#define USB_DISPLAY_EP_MPS               64
#endif

struct usb_display_frame {
    struct aic_usb_display_frame_header header;
    uint8_t *data;
    uint32_t length;
};

struct usb_display_receiver {
    uint8_t header_data[sizeof(struct aic_usb_display_frame_header)];
    uint32_t header_length;
    uint32_t payload_length;
    uint32_t payload_received;
    struct usb_display_frame *frame;
};

static const uint8_t wcid_string_descriptor_msos[18] = {
    0x12, USB_DESCRIPTOR_TYPE_STRING,
    'M', 0, 'S', 0, 'F', 0, 'T', 0, '1', 0, '0', 0, '0', 0,
    WCID_VENDOR_CODE, 0,
};

static const uint8_t winusb_wcid_descriptor[40] = {
    0x28, 0, 0, 0, 0, 1, 4, 0, 1,
    0, 0, 0, 0, 0, 0, 0,
    0, 1,
    'W', 'I', 'N', 'U', 'S', 'B', 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
};

static const uint8_t winusb_wcid_properties[142] = {
    0x8e, 0, 0, 0, 0, 1, 5, 0, 1, 0,
    0x84, 0, 0, 0, 1, 0, 0, 0, 0x28, 0,
    'D', 0, 'e', 0, 'v', 0, 'i', 0, 'c', 0,
    'e', 0, 'I', 0, 'n', 0, 't', 0, 'e', 0,
    'r', 0, 'f', 0, 'a', 0, 'c', 0, 'e', 0,
    'G', 0, 'U', 0, 'I', 0, 'D', 0, 0, 0,
    0x4e, 0, 0, 0,
    '{', 0, '7', 0, 'A', 0, '8', 0, '6', 0,
    'D', 0, '8', 0, '0', 0, 'C', 0, '-', 0,
    '3', 0, 'D', 0, 'A', 0, '0', 0, '-', 0,
    '4', 0, 'A', 0, 'E', 0, '1', 0, '-', 0,
    '8', 0, '2', 0, 'E', 0, '8', 0, '-', 0,
    '7', 0, '7', 0, '4', 0, '4', 0, 'E', 0,
    '5', 0, '8', 0, 'A', 0, 'F', 0, 'B', 0,
    '2', 0, 'F', 0, '}', 0, 0, 0,
};

static const uint8_t *winusb_wcid_properties_table[] = {
    winusb_wcid_properties,
};

static struct usb_msosv1_descriptor msosv1_descriptor = {
    .string = wcid_string_descriptor_msos,
    .vendor_code = WCID_VENDOR_CODE,
    .compat_id = winusb_wcid_descriptor,
    .comp_id_property = winusb_wcid_properties_table,
};

static const uint8_t usb_display_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0, 0, 0,
                               AIC_USB_DISPLAY_VID,
                               AIC_USB_DISPLAY_PID, 0x0101, 0x01),
    USB_CONFIG_DESCRIPTOR_INIT(USB_DISPLAY_CONFIG_SIZE,
                               USB_DISPLAY_INTERFACE_COUNT, 0x01,
                               USB_CONFIG_BUS_POWERED,
                               USB_DISPLAY_MAX_POWER),
    USB_INTERFACE_DESCRIPTOR_INIT(0, 0, 2, 0xff, 0x00, 0x00, 0x04),
    USB_ENDPOINT_DESCRIPTOR_INIT(AIC_USB_DISPLAY_BULK_IN_EP, 0x02,
                                 USB_DISPLAY_EP_MPS, 0),
    USB_ENDPOINT_DESCRIPTOR_INIT(AIC_USB_DISPLAY_BULK_OUT_EP, 0x02,
                                 USB_DISPLAY_EP_MPS, 0),
    USB_LANGID_INIT(USB_DISPLAY_LANGID),
    0x14, USB_DESCRIPTOR_TYPE_STRING,
    'A', 0, 'r', 0, 't', 0, 'I', 0, 'n', 0, 'C', 0, 'h', 0, 'i', 0, 'p', 0,
    /* xfz1986 reads the display mode and encoder from the product string. */
#ifdef LPKG_CHERRYUSB_WINUSB_DISPLAY_H264
    0x50, USB_DESCRIPTOR_TYPE_STRING,
    'a', 0, 'i', 0, 'c', 0, 'u', 0, 'd', 0, 'i', 0, 's', 0, 'p', 0,
    '0', 0, '_', 0, 'R', 0, '1', 0, '0', 0, '2', 0, '4', 0, 'x', 0,
    '6', 0, '0', 0, '0', 0, '_', 0, 'E', 0, 'h', 0, '2', 0, '6', 0,
    '4', 0, '4', 0, '_', 0, 'F', 0, 'p', 0, 's', 0, '3', 0, '0', 0,
    '_', 0, 'B', 0, 'l', 0, '1', 0, '0', 0, '2', 0, '4', 0,
#else
    0x4e, USB_DESCRIPTOR_TYPE_STRING,
    'a', 0, 'i', 0, 'c', 0, 'u', 0, 'd', 0, 'i', 0, 's', 0, 'p', 0,
    '0', 0, '_', 0, 'R', 0, '1', 0, '0', 0, '2', 0, '4', 0, 'x', 0,
    '6', 0, '0', 0, '0', 0, '_', 0, 'E', 0, 'j', 0, 'p', 0, 'g', 0,
    '6', 0, '_', 0, 'F', 0, 'p', 0, 's', 0, '3', 0, '0', 0, '_', 0,
    'B', 0, 'l', 0, '1', 0, '0', 0, '2', 0, '4', 0,
#endif
    0x16, USB_DESCRIPTOR_TYPE_STRING,
    '0', 0, '0', 0, '0', 0, '0', 0, '0', 0, '0', 0, '0', 0, '0', 0,
    '0', 0, '1', 0,
    0x18, USB_DESCRIPTOR_TYPE_STRING,
    'W', 0, 'i', 0, 'n', 0, 'U', 0, 'S', 0, 'B', 0, ' ', 0, 'D', 0,
    'i', 0, 's', 0, 'p', 0,
#ifdef CONFIG_USB_HS
    0x0a, USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER,
    0x00, 0x02, 0, 0, 0, 0x40, 0, 0,
#endif
    0,
};

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX
static uint8_t usb_display_rx[USB_DISPLAY_RX_SIZE];

static struct usb_display_frame usb_display_frames[USB_DISPLAY_FRAME_BUFFER_COUNT];
static struct usb_display_receiver usb_display_receiver;
static aicos_queue_t usb_display_free_frames;
static aicos_queue_t usb_display_ready_frames;

static void usb_display_reset_receiver(void)
{
    memset(&usb_display_receiver, 0, sizeof(usb_display_receiver));
}

static void usb_display_return_frame(struct usb_display_frame *frame)
{
    frame->length = 0;
    if (!aicos_queue_send(usb_display_free_frames, &frame))
        pr_err("USB display: free frame queue is full\n");
}

static bool usb_display_header_valid(
    const struct aic_usb_display_frame_header *header,
    uint32_t payload_length)
{
    return header->type == USB_DISPLAY_ENCODE_TYPE &&
           header->x == 0 && header->y == 0 &&
           header->width == USB_DISPLAY_WIDTH &&
           header->height == USB_DISPLAY_HEIGHT &&
           payload_length > 0 &&
           payload_length <= USB_DISPLAY_MAX_PAYLOAD_SIZE;
}

static void usb_display_begin_frame(void)
{
    struct aic_usb_display_frame_header header;
    struct usb_display_frame *frame = NULL;
    uint32_t payload_length;

    memcpy(&header, usb_display_receiver.header_data, sizeof(header));
    payload_length = aic_usb_display_payload_size(&header);
    usb_display_receiver.payload_length = payload_length;
    usb_display_receiver.payload_received = 0;

    if (header.type == AIC_USB_DISPLAY_TYPE_ZLP_WORKAROUND &&
        payload_length == 0) {
        usb_display_reset_receiver();
        return;
    }

    if (!usb_display_header_valid(&header, payload_length)) {
        pr_warn("USB display: reject frame %u type %u %ux%u+%u+%u len %u\n",
                aic_usb_display_frame_id(&header), header.type,
                header.width, header.height, header.x, header.y,
                payload_length);
        return;
    }

    if (!aicos_queue_receive(usb_display_free_frames, &frame, 0)) {
        pr_warn("USB display: drop frame %u, decoder busy\n",
                aic_usb_display_frame_id(&header));
        return;
    }

    frame->header = header;
    frame->length = 0;
    usb_display_receiver.frame = frame;
}

static void usb_display_finish_frame(void)
{
    struct usb_display_frame *frame = usb_display_receiver.frame;

    if (frame) {
        if (!aicos_queue_send(usb_display_ready_frames, &frame))
            usb_display_return_frame(frame);
    }

    usb_display_reset_receiver();
}

static void usb_display_receive(const uint8_t *data, uint32_t length)
{
    while (length) {
        uint32_t copy_length;

        if (usb_display_receiver.header_length <
            sizeof(struct aic_usb_display_frame_header)) {
            copy_length = sizeof(struct aic_usb_display_frame_header) -
                          usb_display_receiver.header_length;
            if (copy_length > length)
                copy_length = length;

            memcpy(usb_display_receiver.header_data +
                   usb_display_receiver.header_length, data, copy_length);
            usb_display_receiver.header_length += copy_length;
            data += copy_length;
            length -= copy_length;

            if (usb_display_receiver.header_length ==
                sizeof(struct aic_usb_display_frame_header)) {
                usb_display_begin_frame();
                if (!usb_display_receiver.header_length)
                    continue;
            }
        }

        if (!length)
            break;

        copy_length = usb_display_receiver.payload_length -
                      usb_display_receiver.payload_received;
        if (copy_length > length)
            copy_length = length;

        if (usb_display_receiver.frame) {
            memcpy(usb_display_receiver.frame->data +
                   usb_display_receiver.payload_received,
                   data, copy_length);
            usb_display_receiver.frame->length += copy_length;
        }

        usb_display_receiver.payload_received += copy_length;
        data += copy_length;
        length -= copy_length;

        if (usb_display_receiver.payload_received ==
            usb_display_receiver.payload_length)
            usb_display_finish_frame();
    }
}

static int usb_display_show_frame(struct mpp_fb *fb,
                                  struct mpp_frame *frame,
                                  uint32_t width, uint32_t height)
{
    struct aicfb_layer_data layer = {0};

    layer.layer_id = AICFB_LAYER_TYPE_VIDEO;
    layer.enable = 1;
    layer.scale_size.width = width;
    layer.scale_size.height = height;
    layer.buf = frame->buf;
    layer.buf.buf_type = MPP_PHY_ADDR;

    if (mpp_fb_ioctl(fb, AICFB_UPDATE_LAYER_CONFIG, &layer) < 0)
        return -1;

    mpp_fb_ioctl(fb, AICFB_WAIT_FOR_VSYNC, &layer);
    return 0;
}

static void usb_display_disable_ui_layer(struct mpp_fb *fb)
{
    struct aicfb_layer_data layer = {0};

    layer.layer_id = AICFB_LAYER_TYPE_UI;
    if (mpp_fb_ioctl(fb, AICFB_GET_LAYER_CONFIG, &layer) < 0)
        return;

    layer.enable = 0;
    mpp_fb_ioctl(fb, AICFB_UPDATE_LAYER_CONFIG, &layer);
}

static int usb_display_render_frame(struct mpp_decoder *decoder,
                                    struct mpp_fb *fb,
                                    struct usb_display_frame *usb_frame,
                                    uint32_t *decoded_frames)
{
    struct mpp_frame decoded_frame = {0};
    uint32_t visible_width;
    uint32_t visible_height;
    int ret;

    ret = mpp_decoder_get_frame(decoder, &decoded_frame);
    if (ret == DEC_NO_RENDER_FRAME || ret == DEC_ERR_FM_NOT_CREATE ||
        ret == DEC_NO_EMPTY_FRAME)
        return 0;
    if (ret < 0)
        return ret;

    visible_width = decoded_frame.buf.crop_en ?
                    decoded_frame.buf.crop.width :
                    decoded_frame.buf.size.width;
    visible_height = decoded_frame.buf.crop_en ?
                     decoded_frame.buf.crop.height :
                     decoded_frame.buf.size.height;
    if (visible_width != usb_frame->header.width ||
        visible_height != usb_frame->header.height ||
        usb_display_show_frame(fb, &decoded_frame,
                               visible_width, visible_height) < 0) {
        pr_warn("USB display: decoded frame %u has invalid mode "
                "%ux%u (buffer %ux%u)\n",
                aic_usb_display_frame_id(&usb_frame->header),
                visible_width, visible_height,
                decoded_frame.buf.size.width, decoded_frame.buf.size.height);
    } else {
        (*decoded_frames)++;
        if (*decoded_frames == 1 || !(*decoded_frames % 30U))
            pr_info("USB display: displayed frame %u, %s %u bytes, total %u\n",
                    aic_usb_display_frame_id(&usb_frame->header),
                    USB_DISPLAY_CODEC_NAME, usb_frame->length,
                    *decoded_frames);
    }
    mpp_decoder_put_frame(decoder, &decoded_frame);
    return 1;
}

#ifdef LPKG_CHERRYUSB_WINUSB_DISPLAY_H264
static int usb_display_drain_frames(struct mpp_decoder *decoder,
                                    struct mpp_fb *fb,
                                    struct usb_display_frame *usb_frame,
                                    uint32_t *decoded_frames)
{
    int ret;

    do {
        ret = usb_display_render_frame(decoder, fb, usb_frame,
                                       decoded_frames);
    } while (ret > 0);
    return ret;
}
#endif

static int usb_display_decode_packet(struct mpp_decoder *decoder,
                                     const uint8_t *data, uint32_t length)
{
    struct mpp_packet packet = {0};
    int ret;

    ret = mpp_decoder_get_packet(decoder, &packet, length);
    if (ret != DEC_OK)
        return ret;
    memcpy(packet.data, data, length);
    packet.size = length;
    packet.flag = 0;
    ret = mpp_decoder_put_packet(decoder, &packet);
    if (ret != DEC_OK)
        return ret;
    return mpp_decoder_decode(decoder);
}

#ifdef LPKG_CHERRYUSB_WINUSB_DISPLAY_H264
static int usb_display_find_h264_start_code(const uint8_t *data,
                                            uint32_t length,
                                            uint32_t offset)
{
    uint32_t i;

    for (i = offset; i + 3U < length; i++) {
        if (data[i] == 0 && data[i + 1U] == 0 &&
            (data[i + 2U] == 1 ||
             (data[i + 2U] == 0 && data[i + 3U] == 1)))
            return (int)i;
    }
    return -1;
}

static int usb_display_decode_h264_access_unit(struct mpp_decoder *decoder,
                                               struct mpp_fb *fb,
                                               struct usb_display_frame *frame,
                                               uint32_t *decoded_frames)
{
    int start;

    start = usb_display_find_h264_start_code(frame->data, frame->length, 0);
    if (start < 0)
        return DEC_ERR_NOT_SUPPORT;

    while ((uint32_t)start < frame->length) {
        uint32_t search_offset = (uint32_t)start + 3U;
        int next = usb_display_find_h264_start_code(frame->data,
                                                    frame->length,
                                                    search_offset);
        uint32_t nal_length = next < 0 ?
                              frame->length - (uint32_t)start :
                              (uint32_t)next - (uint32_t)start;
        int ret = usb_display_decode_packet(decoder,
                                            frame->data + start,
                                            nal_length);
        bool retry_decode = ret == DEC_NO_EMPTY_FRAME;

        if (ret != DEC_OK && ret != DEC_NO_EMPTY_FRAME)
            return ret;
        ret = usb_display_drain_frames(decoder, fb, frame, decoded_frames);
        if (ret < 0)
            return ret;
        if (retry_decode) {
            ret = mpp_decoder_decode(decoder);
            if (ret != DEC_OK)
                return ret;
            ret = usb_display_drain_frames(decoder, fb, frame,
                                           decoded_frames);
            if (ret < 0)
                return ret;
        }
        if (next < 0)
            break;
        start = next;
    }
    return 0;
}
#endif

static void usb_display_decoder_thread(void *argument)
{
    struct decode_config config = {0};
    struct mpp_decoder *decoder = NULL;
    struct mpp_fb *fb = NULL;
    uint32_t decoded_frames = 0;

    (void)argument;
    config.pix_fmt = MPP_FMT_NV12;
    config.bitstream_buffer_size = USB_DISPLAY_MAX_PAYLOAD_SIZE * 2U;
    config.packet_count = 8;
    config.extra_frame_num = 1;

    while (1) {
        struct usb_display_frame *usb_frame = NULL;
        int ret;

        if (!aicos_queue_receive(usb_display_ready_frames, &usb_frame,
                                 AICOS_WAIT_FOREVER))
            continue;

        /* All RT-Thread device init hooks have completed by the first frame. */
        if (!decoder) {
            decoder = mpp_decoder_create(USB_DISPLAY_CODEC_TYPE);
            fb = mpp_fb_open();
            if (!decoder || !fb || mpp_decoder_init(decoder, &config) < 0) {
                pr_err("USB display: failed to initialize VE/DE pipeline\n");
                if (decoder) {
                    mpp_decoder_destory(decoder);
                    decoder = NULL;
                }
                if (fb) {
                    mpp_fb_close(fb);
                    fb = NULL;
                }
                usb_display_return_frame(usb_frame);
                aicos_msleep(100);
                continue;
            }

            usb_display_disable_ui_layer(fb);
            pr_info("USB display: xfz1986 %s receiver ready "
                    "(%ux%u, max %u KiB)\n",
                    USB_DISPLAY_CODEC_NAME,
                    USB_DISPLAY_WIDTH, USB_DISPLAY_HEIGHT,
                    USB_DISPLAY_MAX_PAYLOAD_SIZE / 1024U);
        }

#ifdef LPKG_CHERRYUSB_WINUSB_DISPLAY_H264
        if (usb_display_find_h264_start_code(usb_frame->data,
                                             usb_frame->length, 0) < 0) {
            pr_warn("USB display: frame %u is not Annex-B H264 (%u bytes)\n",
                    aic_usb_display_frame_id(&usb_frame->header),
                    usb_frame->length);
            goto put_usb_frame;
        }
        ret = usb_display_decode_h264_access_unit(decoder, fb, usb_frame,
                                                  &decoded_frames);
#else
        if (usb_frame->length < 2 || usb_frame->data[0] != 0xff ||
            usb_frame->data[1] != 0xd8) {
            pr_warn("USB display: frame %u is not a JPEG (%u bytes)\n",
                    aic_usb_display_frame_id(&usb_frame->header),
                    usb_frame->length);
            goto put_usb_frame;
        }
        ret = usb_display_decode_packet(decoder, usb_frame->data,
                                        usb_frame->length);
        if (ret >= 0) {
            ret = usb_display_render_frame(decoder, fb, usb_frame,
                                           &decoded_frames);
            if (ret >= 0)
                ret = 0;
        }
#endif
        if (ret != DEC_OK)
            goto decode_error;
        goto put_usb_frame;

decode_error:
        pr_warn("USB display: %s decode failed for frame %u, ret %d\n",
                USB_DISPLAY_CODEC_NAME,
                aic_usb_display_frame_id(&usb_frame->header), ret);
        mpp_decoder_reset(decoder);

put_usb_frame:
        usb_display_return_frame(usb_frame);
    }
}

static int usb_display_prepare_pipeline(void)
{
    uint32_t i;

    usb_display_free_frames = aicos_queue_create(
        sizeof(struct usb_display_frame *), USB_DISPLAY_FRAME_BUFFER_COUNT);
    usb_display_ready_frames = aicos_queue_create(
        sizeof(struct usb_display_frame *), USB_DISPLAY_FRAME_BUFFER_COUNT);
    if (!usb_display_free_frames || !usb_display_ready_frames)
        return -1;

    for (i = 0; i < USB_DISPLAY_FRAME_BUFFER_COUNT; i++) {
        struct usb_display_frame *frame = &usb_display_frames[i];

        frame->data = aicos_malloc_align(0, USB_DISPLAY_MAX_PAYLOAD_SIZE,
                                         CACHE_LINE_SIZE);
        if (!frame->data)
            return -1;
        usb_display_return_frame(frame);
    }

    if (!aicos_thread_create(USB_DISPLAY_THREAD_NAME,
                             USB_DISPLAY_DECODER_STACK_SIZE,
                             USB_DISPLAY_DECODER_PRIORITY,
                             usb_display_decoder_thread, NULL))
        return -1;

    return 0;
}

static void usb_display_out(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)ep;
    if (nbytes)
        usb_display_receive(usb_display_rx, nbytes);

    usbd_ep_start_read(busid, AIC_USB_DISPLAY_BULK_OUT_EP,
                       usb_display_rx, sizeof(usb_display_rx));
}

static void usb_display_in(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    (void)nbytes;
}

static struct usbd_endpoint usb_display_out_ep = {
    .ep_addr = AIC_USB_DISPLAY_BULK_OUT_EP,
    .ep_cb = usb_display_out,
};

static struct usbd_endpoint usb_display_in_ep = {
    .ep_addr = AIC_USB_DISPLAY_BULK_IN_EP,
    .ep_cb = usb_display_in,
};

static struct usbd_interface usb_display_intf;

static void usb_display_event_handler(uint8_t busid, uint8_t event)
{
    switch (event) {
    case USBD_EVENT_CONFIGURED:
        usb_display_reset_receiver();
        usbd_ep_start_read(busid, AIC_USB_DISPLAY_BULK_OUT_EP,
                           usb_display_rx, sizeof(usb_display_rx));
        break;
    case USBD_EVENT_DISCONNECTED:
    case USBD_EVENT_RESET:
        if (usb_display_receiver.frame)
            usb_display_return_frame(usb_display_receiver.frame);
        usb_display_reset_receiver();
        break;
    default:
        break;
    }
}

static int usb_display_init(uint8_t busid, uintptr_t reg_base)
{
    if (sizeof(struct aic_usb_display_frame_header) != 16U)
        return -1;
    if (usb_display_prepare_pipeline() < 0) {
        pr_err("USB display: failed to allocate %s pipeline\n",
               USB_DISPLAY_CODEC_NAME);
        return -1;
    }

    usbd_desc_register(busid, usb_display_descriptor);
    usbd_msosv1_desc_register(busid, &msosv1_descriptor);
    usbd_add_interface(busid, &usb_display_intf);
    usbd_add_endpoint(busid, &usb_display_out_ep);
    usbd_add_endpoint(busid, &usb_display_in_ep);
    usbd_initialize(busid, reg_base, usb_display_event_handler);
    return 0;
}

#if defined(KERNEL_RTTHREAD)
#include <rtthread.h>

static int usbd_winusb_display_init(void)
{
    return usb_display_init(0, USB_DEV_BASE);
}
INIT_DEVICE_EXPORT(usbd_winusb_display_init);
#endif
