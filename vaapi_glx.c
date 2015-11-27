#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>
#include <GL/glut.h>
#include <va/va.h>
#include <va/va_x11.h>
#include <va/va_glx.h>
#include "h264.h"

#define CHECK_VASTATUS(va_status,func)                                  \
if (va_status != VA_STATUS_SUCCESS) {                                   \
    fprintf(stderr,"%s:%s (%d) failed va_status=%x,exit\n", __func__, func, __LINE__,va_status); \
    exit(1);                                                            \
}

#define CLIP_WIDTH  H264_CLIP_WIDTH
#define CLIP_HEIGHT H264_CLIP_HEIGHT

#define WIN_WIDTH  (CLIP_WIDTH<<1)
#define WIN_HEIGHT (CLIP_HEIGHT<<1)

extern glmake(Display* display, int width, int height);
extern void glswap();
extern void glrelease();

void* glsurface;
int texture_id;

Display *x11_display;
Window win;
GC context;

VASurfaceID surface_id;
VAEntrypoint entrypoints[5];
int num_entrypoints,vld_entrypoint;
VAConfigAttrib attrib;
VAConfigID config_id;

VAContextID context_id;
VABufferID pic_param_buf,iqmatrix_buf,slice_param_buf,slice_data_buf;
int major_ver, minor_ver;
VADisplay	va_dpy;
VAStatus va_status;

int kk,entrycnt=0;

static void *open_display(void) {
    return XOpenDisplay(NULL);
}

static void close_display(void *win_display) {
    XCloseDisplay(win_display);
}

void* render(void* param) {

  printf("Copy va surface into texture.\n");
  glEnable(GL_TEXTURE_2D);
  glGenTextures(1,&texture_id);
  printf("texture_id = %d\n",texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D,0, GL_RGBA, CLIP_WIDTH, CLIP_HEIGHT, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

  va_status=vaCreateSurfaceGLX(va_dpy, GL_TEXTURE_2D, texture_id, &glsurface);
  CHECK_VASTATUS(va_status, "vaCreateSurfaceGLX");

  /* Measure the duration of copy*/
  /////////////////////////////////
  struct timeval start_count;
  struct timeval end_count;
  gettimeofday(&start_count, 0);
  va_status=vaCopySurfaceGLX(va_dpy,glsurface,surface_id,0);
  CHECK_VASTATUS(va_status, "vaCopySurfaceGLX");
  gettimeofday(&end_count, 0);

  double starttime_in_micro_sec = (start_count.tv_sec * 1000000) + start_count.tv_usec;
  double endtime_in_micro_sec = (end_count.tv_sec * 1000000) + end_count.tv_usec;
  double duration_in_milli_sec = (endtime_in_micro_sec - starttime_in_micro_sec) * 0.001;

  printf ("\n\n");
  printf ("****************************************************\n");
  printf ("The duration of vaCopySurfaceGLX is %d milliseconds \n", (int)duration_in_milli_sec);
  printf ("****************************************************\n");
  /////////////////////////////////

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, texture_id);

  glBegin(GL_QUADS);
  glTexCoord2f(0, 0); glVertex2f(-1.0f, -1.0f);
  glTexCoord2f(0, 1); glVertex2f(1.0f, -1.0f);
  glTexCoord2f(1, 1); glVertex2f(1.0f,  1.0f);
  glTexCoord2f(1, 0); glVertex2f(-1.0f, 1.0f);
  glEnd();

  glBindTexture(GL_TEXTURE_2D, 0);
  glDisable(GL_TEXTURE_2D);
  glswap();
}

int main(int argc,char **argv)
{
   x11_display = XOpenDisplay(NULL);
   if (!x11_display) {
       fprintf(stderr, "error: can't connect to X server!\n");
       return -1;
   }

   win = glmake(x11_display, WIN_WIDTH, WIN_HEIGHT);

   va_dpy = vaGetDisplayGLX(x11_display);

   va_status = vaInitialize(va_dpy, &major_ver, &minor_ver);
   assert(va_status == VA_STATUS_SUCCESS);
   va_status = vaQueryConfigEntrypoints(va_dpy, VAProfileH264High, entrypoints,
                                        &num_entrypoints);
   CHECK_VASTATUS(va_status, "vaQueryConfigEntrypoints");

   for	(vld_entrypoint = 0; vld_entrypoint < num_entrypoints; vld_entrypoint++) {
       if (entrypoints[vld_entrypoint] == VAEntrypointVLD)
           break;
   }

   for	(kk = 0; kk < num_entrypoints; kk++) {
       if (entrypoints[kk] == VAEntrypointVLD)
           entrycnt++;
   }
   printf("entry count of VAEntrypointVLD for VAProfileH264High is %d\n",entrycnt);

   if (vld_entrypoint == num_entrypoints) {
       /* not find VLD entry point */
       assert(0);
   }

   /* Assuming finding VLD, find out the format for the render target */
   attrib.type = VAConfigAttribRTFormat;
   vaGetConfigAttributes(va_dpy, VAProfileH264High, VAEntrypointVLD,
                         &attrib, 1);
   {
      printf("Support below color format for VAProfileH264High, VAEntrypointVLD:\n");
      if ((attrib.value & VA_RT_FORMAT_YUV420) != 0) printf("  VA_RT_FORMAT_YUV420\n");
      if ((attrib.value & VA_RT_FORMAT_YUV422) != 0) printf("  VA_RT_FORMAT_YUV422\n");
      if ((attrib.value & VA_RT_FORMAT_YUV444) != 0) printf("  VA_RT_FORMAT_YUV444\n");
      if ((attrib.value & VA_RT_FORMAT_YUV411) != 0) printf("  VA_RT_FORMAT_YUV411\n");
      if ((attrib.value & VA_RT_FORMAT_YUV400) != 0) printf("  VA_RT_FORMAT_YUV400\n");
      if ((attrib.value & VA_RT_FORMAT_RGB16) != 0) printf("  VA_RT_FORMAT_RGB16\n");
      if ((attrib.value & VA_RT_FORMAT_RGB32) != 0) printf("  VA_RT_FORMAT_RGB32\n");
      if ((attrib.value & VA_RT_FORMAT_RGBP) != 0) printf("  VA_RT_FORMAT_RGBP\n");
      if ((attrib.value & VA_RT_FORMAT_PROTECTED) != 0) printf("  VA_RT_FORMAT_PROTECTED\n");
   }
   if ((attrib.value & VA_RT_FORMAT_YUV420) == 0) {
       /* not find desired YUV420 RT format */
       assert(0);
   }

   va_status = vaCreateConfig(va_dpy, VAProfileH264High, VAEntrypointVLD,
                             &attrib, 1,&config_id);
   CHECK_VASTATUS(va_status, "vaQueryConfigEntrypoints");

   va_status = vaCreateSurfaces(
       va_dpy,
       VA_RT_FORMAT_YUV420, CLIP_WIDTH, CLIP_HEIGHT,
       &surface_id, 1,
       NULL, 0
   );
   CHECK_VASTATUS(va_status, "vaCreateSurfaces");

   /* Create a context for this decode pipe */
   va_status = vaCreateContext(va_dpy, config_id,
                              CLIP_WIDTH,
                              ((CLIP_HEIGHT+15)/16)*16,
                              VA_PROGRESSIVE,
                              &surface_id,
                              1,
                              &context_id);
   CHECK_VASTATUS(va_status, "vaCreateContext");

   h264_pic_param.frame_num = 0;
   h264_pic_param.CurrPic.picture_id = surface_id;
   h264_pic_param.CurrPic.TopFieldOrderCnt = 0;
   h264_pic_param.CurrPic.BottomFieldOrderCnt = 0;
   for(kk=0;kk<16;kk++){
       h264_pic_param.ReferenceFrames[kk].picture_id          = 0xffffffff;
       h264_pic_param.ReferenceFrames[kk].flags               = VA_PICTURE_H264_INVALID;
       h264_pic_param.ReferenceFrames[kk].TopFieldOrderCnt    = 0;
       h264_pic_param.ReferenceFrames[kk].BottomFieldOrderCnt = 0;
   }

   va_status = vaCreateBuffer(va_dpy, context_id,
                             VAPictureParameterBufferType,
                             sizeof(VAPictureParameterBufferH264),
                             1, &h264_pic_param,
                             &pic_param_buf);
   CHECK_VASTATUS(va_status, "vaCreateBuffer");

   va_status = vaCreateBuffer(va_dpy, context_id,
                             VAIQMatrixBufferType,
                             sizeof(VAIQMatrixBufferH264),
                             1, &h264_iq_matrix,
                             &iqmatrix_buf );
   CHECK_VASTATUS(va_status, "vaCreateBuffer");

   for (kk = 0; kk < 32; kk++) {
       h264_slice_param.RefPicList0[kk].picture_id          = 0xffffffff;
       h264_slice_param.RefPicList0[kk].flags               = VA_PICTURE_H264_INVALID;
       h264_slice_param.RefPicList0[kk].TopFieldOrderCnt    = 0;
       h264_slice_param.RefPicList0[kk].BottomFieldOrderCnt = 0;
       h264_slice_param.RefPicList1[kk].picture_id          = 0xffffffff;
       h264_slice_param.RefPicList1[kk].flags               = VA_PICTURE_H264_INVALID;
       h264_slice_param.RefPicList1[kk].TopFieldOrderCnt    = 0;
       h264_slice_param.RefPicList1[kk].BottomFieldOrderCnt = 0;

   }
   h264_slice_param.slice_data_size   = H264_CLIP_SLICE_SIZE;
   h264_slice_param.slice_data_offset = 0;
   h264_slice_param.slice_data_flag   = VA_SLICE_DATA_FLAG_ALL;

   va_status = vaCreateBuffer(va_dpy, context_id,
                             VASliceParameterBufferType,
                             sizeof(VASliceParameterBufferH264),
                             1,
                             &h264_slice_param, &slice_param_buf);
   CHECK_VASTATUS(va_status, "vaCreateBuffer");

   va_status = vaCreateBuffer(va_dpy, context_id,
                             VASliceDataBufferType,
                             H264_CLIP_SLICE_SIZE,
                             1,
                             h264_clip+H264_CLIP_SLICE_OFFSET,
                             &slice_data_buf);
   CHECK_VASTATUS(va_status, "vaCreateBuffer");

   va_status = vaBeginPicture(va_dpy, context_id, surface_id);
   CHECK_VASTATUS(va_status, "vaBeginPicture");

   va_status = vaRenderPicture(va_dpy,context_id, &pic_param_buf, 1);
   CHECK_VASTATUS(va_status, "vaRenderPicture");

   va_status = vaRenderPicture(va_dpy,context_id, &iqmatrix_buf, 1);
   CHECK_VASTATUS(va_status, "vaRenderPicture");

   va_status = vaRenderPicture(va_dpy,context_id, &slice_param_buf, 1);
   CHECK_VASTATUS(va_status, "vaRenderPicture");

   va_status = vaRenderPicture(va_dpy,context_id, &slice_data_buf, 1);
   CHECK_VASTATUS(va_status, "vaRenderPicture");

   va_status = vaEndPicture(va_dpy,context_id);
   CHECK_VASTATUS(va_status, "vaEndPicture");

   va_status = vaSyncSurface(va_dpy, surface_id);
   CHECK_VASTATUS(va_status, "vaSyncSurface");

   render(NULL);

   printf("press any key to exit\n");
   char is_get_char = getchar();

   if(win) {
     XUnmapWindow(x11_display, win);
     XDestroyWindow(x11_display, win);
     win = NULL;
   }

    //vaDestroySurfaces(va_dpy,&surface_id,1);
    //vaDestroyConfig(va_dpy,config_id);
    //vaDestroyContext(va_dpy,context_id);
    //vaTerminate(va_dpy);
    //close_display(x11_display);

   return 0;
}




