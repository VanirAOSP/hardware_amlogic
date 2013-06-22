#ifndef AML_CAMERA_HARDWARE_INCLUDE_
#define AML_CAMERA_HARDWARE_INCLUDE_

void convert_rgb16_to_nv21(uint8_t *rgb, uint8_t *yuv, int width, int height);
void convert_rgb24_to_rgb16(uint8_t *rgb888, uint8_t *rgb565, int width, int height);
void yuyv422_to_rgb16(unsigned char *from, unsigned char *to, int width,int height);
void yuyv422_to_rgb16(unsigned char *from, unsigned char *to, int size);
void yuyv422_to_rgb24(unsigned char *buf, unsigned char *rgb, int width, int height);
void yuyv422_to_nv21(unsigned char *bufsrc, unsigned char *bufdest, int width, int height);
void yv12_adjust_memcpy(unsigned char *dst, unsigned char *src, int width, int height);
void yuyv_to_yv12(unsigned char *src, unsigned char *dst, int width, int height);
#endif /* AML_CAMERA_HARDWARE_INCLUDE_*/
