/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ANDROID_DRM_CONNECTOR_H_
#define ANDROID_DRM_CONNECTOR_H_

#include "drmencoder.h"
#include "drmmode.h"
#include "drmproperty.h"
#include "rockchip/drmtype.h"

#include <stdint.h>
#include <xf86drmMode.h>
#include <vector>

namespace android {

class DrmDevice;

class DrmConnector {
 public:
  DrmConnector(DrmDevice *drm, drmModeConnectorPtr c,
               DrmEncoder *current_encoder,
               std::vector<DrmEncoder *> &possible_encoders);
  DrmConnector(const DrmProperty &) = delete;
  DrmConnector &operator=(const DrmProperty &) = delete;

  int Init();

  uint32_t id() const;
  uint32_t type() { return type_; }
  uint32_t type_id() const { return type_id_; };

  int display() const;
  void set_display(int display);
  int priority() const;
  void set_priority(uint32_t priority);
  uint32_t possible_displays() const;
  void set_possible_displays(uint32_t possible_displays);

  bool internal() const;
  bool external() const;
  bool writeback() const;
  bool valid_type() const;

  int UpdateModes();
  void ResetModesReady(){ bModeReady_ = false;};
  bool ModesReady(){ return bModeReady_;};

  const std::vector<DrmMode> &modes() const {
    return modes_;
  }
  const std::vector<DrmMode> &raw_modes() const {
    return raw_modes_;
  }
  const DrmMode &best_mode() const;
  const DrmMode &active_mode() const;
  const DrmMode &current_mode() const;
  void set_best_mode(const DrmMode &mode);
  void set_active_mode(const DrmMode &mode);
  void set_current_mode(const DrmMode &mode);
  void SetDpmsMode(uint32_t dpms_mode);

  const DrmProperty &dpms_property() const;
  const DrmProperty &crtc_id_property() const;
  const DrmProperty &writeback_pixel_formats() const;
  const DrmProperty &writeback_fb_id() const;
  const DrmProperty &writeback_out_fence() const;

  const std::vector<DrmEncoder *> &possible_encoders() const {
    return possible_encoders_;
  }
  DrmEncoder *encoder() const;
  void set_encoder(DrmEncoder *encoder);

  drmModeConnection state() const;
  drmModeConnection raw_state() const;
  void force_disconnect(bool force);

  uint32_t mm_width() const;
  uint32_t mm_height() const;

  uint32_t get_preferred_mode_id() const {
    return preferred_mode_id_;
  }
  // RK Support
  bool isSupportSt2084() { return bSupportSt2084_; }
  bool isSupportHLG() { return bSupportHLG_; }
  bool is_hdmi_support_hdr() const;
  int switch_hdmi_hdr_mode(android_dataspace_t colorspace);

  const DrmProperty &brightness_id_property() const;
  const DrmProperty &contrast_id_property() const;
  const DrmProperty &saturation_id_property() const;
  const DrmProperty &hue_id_property() const;
  const DrmProperty &hdr_metadata_property() const;
  const DrmProperty &hdr_panel_property() const;
  const DrmProperty &hdmi_output_colorimetry_property() const;
  const DrmProperty &hdmi_output_format_property() const;
  const DrmProperty &hdmi_output_depth_property() const;

  const std::vector<DrmHdr> &get_hdr_support_list() const { return drmHdr_; }
  struct hdr_static_metadata* get_hdr_metadata_ptr(){ return &hdr_metadata_; };

 private:
  DrmDevice *drm_;

  uint32_t id_;
  DrmEncoder *encoder_;
  int display_;

  uint32_t type_;
  uint32_t type_id_;
  uint32_t priority_;
  drmModeConnection state_;
  bool force_disconnect_;

  uint32_t mm_width_;
  uint32_t mm_height_;

  DrmMode active_mode_;
  DrmMode current_mode_;
  DrmMode best_mode_;
  std::vector<DrmMode> modes_;
  std::vector<DrmMode> raw_modes_;
  std::vector<DrmHdr> drmHdr_;

  DrmProperty dpms_property_;
  DrmProperty crtc_id_property_;
  DrmProperty writeback_pixel_formats_;
  DrmProperty writeback_fb_id_;
  DrmProperty writeback_out_fence_;

  //RK support
  DrmProperty brightness_id_property_;
  DrmProperty contrast_id_property_;
  DrmProperty saturation_id_property_;
  DrmProperty hue_id_property_;
  DrmProperty hdr_metadata_property_;
  DrmProperty hdr_panel_property_;
  DrmProperty hdmi_output_colorimetry_;
  DrmProperty hdmi_output_format_;
  DrmProperty hdmi_output_depth_;
  std::vector<DrmEncoder *> possible_encoders_;
  drmModeConnectorPtr connector_;

  uint32_t preferred_mode_id_;
  uint32_t possible_displays_;

  bool bModeReady_;
  bool bSupportSt2084_;
  bool bSupportHLG_;
  struct hdr_static_metadata hdr_metadata_;
  int colorimetry_;
  struct hdr_output_metadata last_hdr_metadata_;
};
}  // namespace android

#endif  // ANDROID_DRM_PLANE_H_
