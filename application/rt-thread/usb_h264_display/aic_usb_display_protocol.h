/*
 * Copyright (c) 2026, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AIC_USB_DISPLAY_PROTOCOL_H
#define AIC_USB_DISPLAY_PROTOCOL_H

#include <stdint.h>

/* Keep the ArtInChip USB identity; the Windows INF binds to this VID/PID. */
#define AIC_USB_DISPLAY_VID                  0x33C3U
#define AIC_USB_DISPLAY_PID                  0x0E05U
#define AIC_USB_DISPLAY_BULK_IN_EP           0x81U
#define AIC_USB_DISPLAY_BULK_OUT_EP          0x02U

/* xfz1986 udisp v2 wire protocol. */
#define AIC_USB_DISPLAY_TYPE_RGB565          0U
#define AIC_USB_DISPLAY_TYPE_RGB888          1U
#define AIC_USB_DISPLAY_TYPE_YUV420          2U
#define AIC_USB_DISPLAY_TYPE_JPEG            3U
#define AIC_USB_DISPLAY_TYPE_H264            4U
#define AIC_USB_DISPLAY_TYPE_ZLP_WORKAROUND  0xffU

#define AIC_USB_DISPLAY_FRAME_ID_MASK        0x3ffU
#define AIC_USB_DISPLAY_PAYLOAD_SHIFT        10U
#define AIC_USB_DISPLAY_MAX_JPEG_SIZE        (1024U * 1024U)
#define AIC_USB_DISPLAY_MAX_H264_AU_SIZE     (1024U * 1024U)

#if defined(_MSC_VER)
#pragma pack(push, 1)
#define AIC_USB_DISPLAY_PACKED
#else
#define AIC_USB_DISPLAY_PACKED __attribute__((packed))
#endif

/*
 * The final 32-bit word is encoded by the Windows bitfields as:
 * bits 0..9 frame_id, bits 10..31 payload_total. Keeping it as a plain
 * integer avoids relying on compiler-specific bitfield layout on the target.
 */
struct AIC_USB_DISPLAY_PACKED aic_usb_display_frame_header {
    uint16_t crc16;
    uint8_t type;
    uint8_t command;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint32_t frame_info;
};

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

static inline uint16_t aic_usb_display_frame_id(
    const struct aic_usb_display_frame_header *header)
{
    return (uint16_t)(header->frame_info & AIC_USB_DISPLAY_FRAME_ID_MASK);
}

static inline uint32_t aic_usb_display_payload_size(
    const struct aic_usb_display_frame_header *header)
{
    return header->frame_info >> AIC_USB_DISPLAY_PAYLOAD_SHIFT;
}

#endif /* AIC_USB_DISPLAY_PROTOCOL_H */
