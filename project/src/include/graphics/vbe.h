#pragma once
#include <stdint.h>

#define VBE_MODE 0x4118

typedef struct {
    uint16_t mode_attributes;
    uint8_t  window_a_attr;
    uint8_t  window_b_attr;
    uint16_t window_granularity;
    uint16_t window_size;
    uint16_t window_a_segment;
    uint16_t window_b_segment;
    uint32_t window_function;
    uint16_t bytes_per_scanline;
    uint16_t width;
    uint16_t height;
    uint8_t  char_width;
    uint8_t  char_height;
    uint8_t  planes;
    uint8_t  bpp;
    uint8_t  banks;
    uint8_t  memory_model;
    uint8_t  bank_size;
    uint8_t  image_pages;
    uint8_t  reserved1;
    uint8_t  red_mask_size;
    uint8_t  red_field_pos;
    uint8_t  green_mask_size;
    uint8_t  green_field_pos;
    uint8_t  blue_mask_size;
    uint8_t  blue_field_pos;
    uint8_t  reserved_mask_size;
    uint8_t  reserved_field_pos;
    uint8_t  direct_color_info;
    uint32_t framebuffer;
    uint32_t off_screen_mem;
    uint16_t off_screen_mem_size;
    uint8_t  reserved[206];
} __attribute__((packed)) vbe_mode_info_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    uint8_t bpp;
    uint32_t* framebuffer;
} vbe_info_t;

extern vbe_mode_info_t vbe_mode_info_data;

void vbe_init();
uint16_t vbe_get_width();
uint16_t vbe_get_height();
uint16_t vbe_get_pitch();
uint8_t vbe_get_bpp();
uint8_t* vbe_get_framebuffer();
vbe_info_t get_vbe_struct();