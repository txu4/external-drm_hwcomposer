/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "hwc-platform-IA"

#include <drm/drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cutils/log.h>

#include "platformia.h"

namespace android {

#define ALIGN(val, align) (((val) + (align)-1) & ~((align)-1))

#ifdef USE_IA_PLANNER
// static
Importer *Importer::CreateInstance(DrmResources *drm) {
  IAImporter *importer = new IAImporter(drm);
  if (!importer)
    return NULL;

  int ret = importer->Init();
  if (ret) {
    ALOGE("Failed to initialize the IA importer %d", ret);
    delete importer;
    return NULL;
  }
  return importer;
}
#endif

IAImporter::IAImporter(DrmResources *drm) : drm_(drm) {
}

IAImporter::~IAImporter() {
}

int IAImporter::Init() {
  int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                          (const hw_module_t **)&gralloc_);
  if (ret) {
    ALOGE("Failed to open gralloc module");
    return ret;
  }
  return 0;
}

uint32_t IAImporter::ConvertHalFormatToDrm(uint32_t hal_format) {
  switch (hal_format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
      return DRM_FORMAT_ABGR8888;
    case HAL_PIXEL_FORMAT_RGBX_8888:
      return DRM_FORMAT_XBGR8888;
    case HAL_PIXEL_FORMAT_RGB_888:
      return DRM_FORMAT_BGR888;
    case HAL_PIXEL_FORMAT_BGRA_8888:
      return DRM_FORMAT_ARGB8888;
    case HAL_PIXEL_FORMAT_RGB_565:
      return DRM_FORMAT_RGB565;
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
      return DRM_FORMAT_YUV420;
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
      return DRM_FORMAT_YUYV;
    case HAL_PIXEL_FORMAT_YCbCr_422_SP:
      return DRM_FORMAT_NV16;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
      return DRM_FORMAT_NV21;
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
      return DRM_FORMAT_XBGR8888;
    default:
      ALOGI("Unknown HAL Format 0x%x", hal_format);
      return 0;
  }
}

uint32_t IAImporter::GetFormatForFrameBuffer(uint32_t fourcc_format,
                                             uint32_t plane_type) {
  if (plane_type != DRM_PLANE_TYPE_PRIMARY)
    return fourcc_format;

  // We only support 24 bit colordepth for primary planes on
  // pre SKL Hardware. Ideally, we query format support from
  // plane to determine this.
  switch (fourcc_format) {
    case DRM_FORMAT_ABGR8888:
      return DRM_FORMAT_XBGR8888;
    case DRM_FORMAT_ARGB8888:
      return DRM_FORMAT_XRGB8888;
    default:
      break;
  }

  return fourcc_format;
}

EGLImageKHR IAImporter::ImportImage(EGLDisplay egl_display,
                                    DrmHwcBuffer *buffer,
                                    buffer_handle_t handle) {
  gralloc_drm_handle_t *gr_handle = gralloc_drm_handle(handle);
  EGLImageKHR image = EGL_NO_IMAGE_KHR;
  // Note: If eglCreateImageKHR is successful for a EGL_LINUX_DMA_BUF_EXT
  // target, the EGL will take a reference to the dma_buf.
  if (buffer->operator->()->format == DRM_FORMAT_YUV420) {
    const EGLint attr_list_yv12[] = {
        EGL_WIDTH,
        static_cast<EGLint>(buffer->operator->()->width),
        EGL_HEIGHT,
        static_cast<EGLint>(buffer->operator->()->height),
        EGL_LINUX_DRM_FOURCC_EXT,
        DRM_FORMAT_YUV420,
        EGL_DMA_BUF_PLANE0_FD_EXT,
        gr_handle->prime_fd,
        EGL_DMA_BUF_PLANE0_PITCH_EXT,
        static_cast<EGLint>(buffer->operator->()->pitches[0]),
        EGL_DMA_BUF_PLANE0_OFFSET_EXT,
        static_cast<EGLint>(buffer->operator->()->offsets[0]),
        EGL_DMA_BUF_PLANE1_FD_EXT,
        gr_handle->prime_fd,
        EGL_DMA_BUF_PLANE1_PITCH_EXT,
        static_cast<EGLint>(buffer->operator->()->pitches[1]),
        EGL_DMA_BUF_PLANE1_OFFSET_EXT,
        static_cast<EGLint>(buffer->operator->()->offsets[1]),
        EGL_DMA_BUF_PLANE2_FD_EXT,
        gr_handle->prime_fd,
        EGL_DMA_BUF_PLANE2_PITCH_EXT,
        static_cast<EGLint>(buffer->operator->()->pitches[2]),
        EGL_DMA_BUF_PLANE2_OFFSET_EXT,
        static_cast<EGLint>(buffer->operator->()->offsets[2]),
        EGL_NONE,
        0};
    image = eglCreateImageKHR(
        egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
        static_cast<EGLClientBuffer>(nullptr), attr_list_yv12);
  } else {
    const EGLint attr_list[] = {
        EGL_WIDTH,
        static_cast<EGLint>(buffer->operator->()->width),
        EGL_HEIGHT,
        static_cast<EGLint>(buffer->operator->()->height),
        EGL_LINUX_DRM_FOURCC_EXT,
        static_cast<EGLint>(buffer->operator->()->format),
        EGL_DMA_BUF_PLANE0_FD_EXT,
        gr_handle->prime_fd,
        EGL_DMA_BUF_PLANE0_PITCH_EXT,
        static_cast<EGLint>(buffer->operator->()->pitches[0]),
        EGL_DMA_BUF_PLANE0_OFFSET_EXT,
        0,
        EGL_NONE,
        0};
    image =
        eglCreateImageKHR(egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                          static_cast<EGLClientBuffer>(nullptr), attr_list);
  }

  return image;
}

int IAImporter::ImportBuffer(buffer_handle_t handle, hwc_drm_bo_t *bo) {
  gralloc_drm_handle_t *gr_handle = gralloc_drm_handle(handle);
  if (!gr_handle)
    return -EINVAL;

  uint32_t gem_handle;
  int ret = drmPrimeFDToHandle(drm_->fd(), gr_handle->prime_fd, &gem_handle);
  if (ret) {
    ALOGE("Failed to import prime fd %d ret=%d", gr_handle->prime_fd, ret);
    return ret;
  }

  memset(bo, 0, sizeof(hwc_drm_bo_t));
  bo->width = gr_handle->width;
  bo->height = gr_handle->height;
  bo->format = ConvertHalFormatToDrm(gr_handle->format);
  bo->pitches[0] = gr_handle->stride;
  bo->gem_handles[0] = gem_handle;
  bo->offsets[0] = 0;

  if (bo->format == DRM_FORMAT_YUV420) {
    bo->pitches[1] = ALIGN(bo->pitches[0] / 2, 16);
    bo->pitches[2] = ALIGN(bo->pitches[0] / 2, 16);
    bo->offsets[2] = bo->offsets[0] + bo->pitches[0] * bo->height;
    bo->offsets[1] = bo->offsets[2] + bo->pitches[2] * bo->height / 2;
  }

  return 0;
}

int IAImporter::CreateFrameBuffer(hwc_drm_bo_t *bo, uint32_t plane_type) {
  uint32_t format = GetFormatForFrameBuffer(bo->format, plane_type);
  int ret =
      drmModeAddFB2(drm_->fd(), bo->width, bo->height, format, bo->gem_handles,
                    bo->pitches, bo->offsets, &bo->fb_id, 0);

  if (ret) {
    ALOGE("drmModeAddFB2 error (%dx%d, %c%c%c%c, handle %d pitch %d) (%s)",
          bo->width, bo->height, format, format >> 8,
          format >> 16, format >> 24, bo->gem_handles[0], bo->pitches[0],
          strerror(-ret));
  }

  return ret;
}

int IAImporter::ReleaseBuffer(hwc_drm_bo_t *bo) {
  if (bo->fb_id)
    if (drmModeRmFB(drm_->fd(), bo->fb_id))
      ALOGE("Failed to rm fb");

  struct drm_gem_close gem_close;
  memset(&gem_close, 0, sizeof(gem_close));
  int num_gem_handles = sizeof(bo->gem_handles) / sizeof(bo->gem_handles[0]);
  for (int i = 0; i < num_gem_handles; i++) {
    if (!bo->gem_handles[i])
      continue;

    gem_close.handle = bo->gem_handles[i];
    int ret = drmIoctl(drm_->fd(), DRM_IOCTL_GEM_CLOSE, &gem_close);
    if (ret)
      ALOGE("Failed to close gem handle %d %d", i, ret);
    else
      bo->gem_handles[i] = 0;
  }
  return 0;
}

int PlanStagePrimary::ProvisionPlanes(
    std::vector<DrmCompositionPlane> *composition,
    std::map<size_t, DrmHwcLayer *> &layers, DrmCrtc *crtc,
    std::vector<DrmPlane *> *planes) {
  // Ensure we always have a valid primary plane. On some platforms vblank is
  // tied to primary and whole pipe can get disabled in case primary plane is
  // disabled.
  DrmPlane *primary_plane = NULL;
  for (auto i = planes->begin(); i != planes->end(); ++i) {
    if ((*i)->type() == DRM_PLANE_TYPE_PRIMARY) {
      primary_plane = *i;
      planes->erase(i);
      break;
    }
  }

  // If we dont have a free primary plane, than its being used as the precomp
  // plane.
  if (!primary_plane)
    return 0;

  auto precomp = GetPrecompIter(composition);
  composition->emplace(precomp, DrmCompositionPlane::Type::kLayer,
                       primary_plane, crtc, layers.begin()->first);
  layers.erase(layers.begin());

  return 0;
}

#ifdef USE_IA_PLANNER
std::unique_ptr<Planner> Planner::CreateInstance(DrmResources *) {
  std::unique_ptr<Planner> planner(new Planner);
  planner->AddStage<PlanStagePrimary>();
  planner->AddStage<PlanStageGreedy>();
  return planner;
}
#endif
}
