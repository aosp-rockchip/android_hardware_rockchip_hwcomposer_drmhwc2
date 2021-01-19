/*
 * Copyright (C) 2020 Rockchip Electronics Co.Ltd.
 *
 * Modification based on code covered by the Apache License, Version 2.0 (the "License").
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS TO YOU ON AN "AS IS" BASIS
 * AND ANY AND ALL WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH SOFTWARE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF TITLE,
 * NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY, ACCURACY OR FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.
 *
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
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
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#define LOG_TAG "drm-vop"

#include "rockchip/platform/drmvop2.h"
#include "drmdevice.h"

#include <log/log.h>

namespace android {

void PlanStageVop2::Init(){
  bMultiAreaEnable = hwc_get_bool_property("vendor.hwc.multi_area_enable","false");

  bMultiAreaScaleEnable = hwc_get_int_property("vendor.hwc.multi_area_scale_mode","false");

  bSmartScaleEnable = hwc_get_bool_property("vendor.hwc.smart_scale_enable","false");
  ALOGI_IF(LogLevel(DBG_DEBUG),"PlanStageVop2::Init bMultiAreaEnable=%d, bMultiAreaScaleEnable=%d",
            bMultiAreaEnable,bMultiAreaScaleEnable);
}

bool PlanStageVop2::HasLayer(std::vector<DrmHwcLayer*>& layer_vector,DrmHwcLayer *layer){
        for (std::vector<DrmHwcLayer*>::const_iterator iter = layer_vector.begin();
               iter != layer_vector.end(); ++iter) {
            if((*iter)->uId_==layer->uId_)
                return true;
          }

          return false;
}

int PlanStageVop2::IsXIntersect(hwc_rect_t* rec,hwc_rect_t* rec2){
    if(rec2->top == rec->top)
        return 1;
    else if(rec2->top < rec->top)
    {
        if(rec2->bottom > rec->top)
            return 1;
        else
            return 0;
    }
    else
    {
        if(rec->bottom > rec2->top  )
            return 1;
        else
            return 0;
    }
    return 0;
}


bool PlanStageVop2::IsRec1IntersectRec2(hwc_rect_t* rec1, hwc_rect_t* rec2){
    int iMaxLeft,iMaxTop,iMinRight,iMinBottom;
    ALOGD_IF(LogLevel(DBG_DEBUG),"is_not_intersect: rec1[%d,%d,%d,%d],rec2[%d,%d,%d,%d]",rec1->left,rec1->top,
        rec1->right,rec1->bottom,rec2->left,rec2->top,rec2->right,rec2->bottom);

    iMaxLeft = rec1->left > rec2->left ? rec1->left: rec2->left;
    iMaxTop = rec1->top > rec2->top ? rec1->top: rec2->top;
    iMinRight = rec1->right <= rec2->right ? rec1->right: rec2->right;
    iMinBottom = rec1->bottom <= rec2->bottom ? rec1->bottom: rec2->bottom;

    if(iMaxLeft > iMinRight || iMaxTop > iMinBottom)
        return false;
    else
        return true;

    return false;
}

bool PlanStageVop2::IsLayerCombine(DrmHwcLayer * layer_one,DrmHwcLayer * layer_two){
    if(!bMultiAreaEnable)
      return false;

    //multi region only support RGBA888 RGBX8888 RGB888 565 BGRA888 NV12
    if(layer_one->iFormat_ >= HAL_PIXEL_FORMAT_YCrCb_NV12_10
        || layer_two->iFormat_ >= HAL_PIXEL_FORMAT_YCrCb_NV12_10
        || (layer_one->iFormat_ != layer_two->iFormat_)
        || layer_one->alpha!= layer_two->alpha
        || ((layer_one->bScale_ || layer_two->bScale_) && !bMultiAreaScaleEnable)
        || IsRec1IntersectRec2(&layer_one->display_frame,&layer_two->display_frame)
        || IsXIntersect(&layer_one->display_frame,&layer_two->display_frame)
        )
    {
        ALOGD_IF(LogLevel(DBG_DEBUG),"is_layer_combine layer one alpha=%d,is_scale=%d",layer_one->alpha,layer_one->bScale_);
        ALOGD_IF(LogLevel(DBG_DEBUG),"is_layer_combine layer two alpha=%d,is_scale=%d",layer_two->alpha,layer_two->bScale_);
        return false;
    }

    return true;
}

int PlanStageVop2::CombineLayer(LayerMap& layer_map,std::vector<DrmHwcLayer*> &layers,uint32_t iPlaneSize){

    /*Group layer*/
    int zpos = 0;
    size_t i,j;
    uint32_t sort_cnt=0;
    bool is_combine = false;

    layer_map.clear();

    for (i = 0; i < layers.size(); ) {
        if(!layers[i]->bUse_)
            continue;

        sort_cnt=0;
        if(i == 0)
        {
            layer_map[zpos].push_back(layers[0]);
        }

        for(j = i+1; j < layers.size(); j++) {
            DrmHwcLayer *layer_one = layers[j];
            //layer_one.index = j;
            is_combine = false;

            for(size_t k = 0; k <= sort_cnt; k++ ) {
                DrmHwcLayer *layer_two = layers[j-1-k];
                //layer_two.index = j-1-k;
                //juage the layer is contained in layer_vector
                bool bHasLayerOne = HasLayer(layer_map[zpos],layer_one);
                bool bHasLayerTwo = HasLayer(layer_map[zpos],layer_two);

                //If it contain both of layers,then don't need to go down.
                if(bHasLayerOne && bHasLayerTwo)
                    continue;

                if(IsLayerCombine(layer_one,layer_two)) {
                    //append layer into layer_vector of layer_map_.
                    if(!bHasLayerOne && !bHasLayerTwo)
                    {
                        layer_map[zpos].emplace_back(layer_one);
                        layer_map[zpos].emplace_back(layer_two);
                        is_combine = true;
                    }
                    else if(!bHasLayerTwo)
                    {
                        is_combine = true;
                        for(std::vector<DrmHwcLayer*>::const_iterator iter= layer_map[zpos].begin();
                            iter != layer_map[zpos].end();++iter)
                        {
                            if((*iter)->uId_==layer_one->uId_)
                                    continue;

                            if(!IsLayerCombine(*iter,layer_two))
                            {
                                is_combine = false;
                                break;
                            }
                        }

                        if(is_combine)
                            layer_map[zpos].emplace_back(layer_two);
                    }
                    else if(!bHasLayerOne)
                    {
                        is_combine = true;
                        for(std::vector<DrmHwcLayer*>::const_iterator iter= layer_map[zpos].begin();
                            iter != layer_map[zpos].end();++iter)
                        {
                            if((*iter)->uId_==layer_two->uId_)
                                    continue;

                            if(!IsLayerCombine(*iter,layer_one))
                            {
                                is_combine = false;
                                break;
                            }
                        }

                        if(is_combine)
                            layer_map[zpos].emplace_back(layer_one);
                    }
                }

                if(!is_combine)
                {
                    //if it cann't combine two layer,it need start a new group.
                    if(!bHasLayerOne)
                    {
                        zpos++;
                        layer_map[zpos].emplace_back(layer_one);
                    }
                    is_combine = false;
                    break;
                }
             }
             sort_cnt++; //update sort layer count
             if(!is_combine)
             {
                break;
             }
        }

        if(is_combine)  //all remain layer or limit MOST_WIN_ZONES layer is combine well,it need start a new group.
            zpos++;
        if(sort_cnt)
            i+=sort_cnt;    //jump the sort compare layers.
        else
            i++;
    }

  // RK356x sort layer by ypos
  for (LayerMap::iterator iter = layer_map.begin();
       iter != layer_map.end(); ++iter) {
        if(iter->second.size() > 1) {
            for(uint32_t i=0;i < iter->second.size()-1;i++) {
                for(uint32_t j=i+1;j < iter->second.size();j++) {
                     if(iter->second[i]->display_frame.top > iter->second[j]->display_frame.top) {
                        ALOGD_IF(LogLevel(DBG_DEBUG),"swap %d and %d",iter->second[i]->uId_,iter->second[j]->uId_);
                        std::swap(iter->second[i],iter->second[j]);
                     }
                 }
            }
        }
  }


  for (LayerMap::iterator iter = layer_map.begin();
       iter != layer_map.end(); ++iter) {
        ALOGD_IF(LogLevel(DBG_DEBUG),"layer map id=%d,size=%zu",iter->first,iter->second.size());
        for(std::vector<DrmHwcLayer*>::const_iterator iter_layer = iter->second.begin();
            iter_layer != iter->second.end();++iter_layer)
        {
             ALOGD_IF(LogLevel(DBG_DEBUG),"\tlayer id=%u , name=%s",(*iter_layer)->uId_,(*iter_layer)->sLayerName_.c_str());
        }
  }

    if((int)layer_map.size() > iPlaneSize)
    {
        ALOGD_IF(LogLevel(DBG_DEBUG),"map size=%zu should not bigger than plane size=%d", layer_map.size(), iPlaneSize);
        return -1;
    }

    return 0;

}

bool PlanStageVop2::HasGetNoAfbcUsablePlanes(DrmCrtc *crtc, std::vector<PlaneGroup *> &plane_groups) {
    std::vector<DrmPlane *> usable_planes;
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(!(*iter)->bUse)
                //only count the first plane in plane group.
                std::copy_if((*iter)->planes.begin(), (*iter)->planes.begin()+1,
                       std::back_inserter(usable_planes),
                       [=](DrmPlane *plane) {
                       return !plane->is_use() && plane->GetCrtcSupported(*crtc) && !plane->get_afbc(); }
                       );
  }
  return usable_planes.size() > 0;;
}

bool PlanStageVop2::HasGetNoYuvUsablePlanes(DrmCrtc *crtc, std::vector<PlaneGroup *> &plane_groups) {
    std::vector<DrmPlane *> usable_planes;
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(!(*iter)->bUse)
                //only count the first plane in plane group.
                std::copy_if((*iter)->planes.begin(), (*iter)->planes.begin()+1,
                       std::back_inserter(usable_planes),
                       [=](DrmPlane *plane) {
                       return !plane->is_use() && plane->GetCrtcSupported(*crtc) && !plane->get_yuv(); }
                       );
  }
  return usable_planes.size() > 0;;
}

bool PlanStageVop2::HasGetNoScaleUsablePlanes(DrmCrtc *crtc, std::vector<PlaneGroup *> &plane_groups) {
    std::vector<DrmPlane *> usable_planes;
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(!(*iter)->bUse)
                //only count the first plane in plane group.
                std::copy_if((*iter)->planes.begin(), (*iter)->planes.begin()+1,
                       std::back_inserter(usable_planes),
                       [=](DrmPlane *plane) {
                       return !plane->is_use() && plane->GetCrtcSupported(*crtc) && !plane->get_scale(); }
                       );
  }
  return usable_planes.size() > 0;;
}

bool PlanStageVop2::HasGetNoAlphaUsablePlanes(DrmCrtc *crtc, std::vector<PlaneGroup *> &plane_groups) {
    std::vector<DrmPlane *> usable_planes;
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(!(*iter)->bUse)
                //only count the first plane in plane group.
                std::copy_if((*iter)->planes.begin(), (*iter)->planes.begin()+1,
                       std::back_inserter(usable_planes),
                       [=](DrmPlane *plane) {
                       return !plane->is_use() && plane->GetCrtcSupported(*crtc) && !plane->alpha_property().id(); }
                       );
  }
  return usable_planes.size() > 0;
}

bool PlanStageVop2::HasGetNoEotfUsablePlanes(DrmCrtc *crtc, std::vector<PlaneGroup *> &plane_groups) {
    std::vector<DrmPlane *> usable_planes;
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(!(*iter)->bUse)
                //only count the first plane in plane group.
                std::copy_if((*iter)->planes.begin(), (*iter)->planes.begin()+1,
                       std::back_inserter(usable_planes),
                       [=](DrmPlane *plane) {
                       return !plane->is_use() && plane->GetCrtcSupported(*crtc) && !plane->get_hdr2sdr(); }
                       );
  }
  return usable_planes.size() > 0;
}

bool PlanStageVop2::GetCrtcSupported(const DrmCrtc &crtc, uint32_t possible_crtc_mask) {
  return !!((1 << crtc.pipe()) & possible_crtc_mask);
}

bool PlanStageVop2::HasPlanesWithSize(DrmCrtc *crtc, int layer_size, std::vector<PlaneGroup *> &plane_groups) {
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(GetCrtcSupported(*crtc, (*iter)->possible_crtcs) && !(*iter)->bUse &&
                (*iter)->planes.size() == (size_t)layer_size)
                return true;
  }
  return false;
}

int PlanStageVop2::MatchPlane(std::vector<DrmCompositionPlane> *composition_planes,
                   std::vector<PlaneGroup *> &plane_groups,
                   DrmCompositionPlane::Type type, DrmCrtc *crtc,
                   std::pair<int, std::vector<DrmHwcLayer*>> layers, int *zpos, bool match_best=false) {

  uint32_t layer_size = layers.second.size();
  bool b_yuv=false,b_scale=false,b_alpha=false,b_hdr2sdr=false,b_afbc=false;
  std::vector<PlaneGroup *> ::const_iterator iter;
  uint64_t rotation = 0;
  uint64_t alpha = 0xFF;
  uint16_t eotf = TRADITIONAL_GAMMA_SDR;
  bool bMulArea = layer_size > 0 ? true : false;
  DrmDevice *drm = crtc->getDrmDevice();
  DrmConnector *connector = drm->GetConnectorForDisplay(crtc->display());
  bool bHdrSupport = connector->is_hdmi_support_hdr() && iSupportHdrCnt > 0;

  static bool b_cluster0_used = false;
  static bool b_cluster1_used = false;
  static int i_cluster0_used_dst_x_offset = 0;
  static int i_cluster1_used_dst_x_offset = 0;
  static bool b_cluster0_two_win_mode = false;
  static bool b_cluster1_two_win_mode = false;

  //loop plane groups.
  for (iter = plane_groups.begin();
     iter != plane_groups.end(); ++iter) {
     uint32_t combine_layer_count = 0;
     ALOGD_IF(LogLevel(DBG_DEBUG),"line=%d,last zpos=%d,group(%" PRIu64 ") zpos=%d,group bUse=%d,crtc=0x%x,"
                                   "current_possible_crtcs=0x%x,possible_crtcs=0x%x",
                                   __LINE__, *zpos, (*iter)->share_id, (*iter)->zpos, (*iter)->bUse,
                                   (1<<crtc->pipe()), (*iter)->current_possible_crtcs,(*iter)->possible_crtcs);
      //find the match zpos plane group
      if(!(*iter)->bUse && !(*iter)->b_reserved)
      {
          ALOGD_IF(LogLevel(DBG_DEBUG),"line=%d,layer_size=%d,planes size=%zu",__LINE__,layer_size,(*iter)->planes.size());

          //find the match combine layer count with plane size.
          if(layer_size <= (*iter)->planes.size())
          {
              //loop layer
              for(std::vector<DrmHwcLayer*>::const_iterator iter_layer= layers.second.begin();
                  iter_layer != layers.second.end();++iter_layer)
              {
                  //reset is_match to false
                  (*iter_layer)->bMatch_ = false;

                  if(match_best){
                      if(!((*iter)->win_type & (*iter_layer)->iBestPlaneType)){
                          ALOGD_IF(LogLevel(DBG_DEBUG),"line=%d, plane_group win-type = 0x%" PRIx64 " , layer best-type = %x, not match ",
                          __LINE__,(*iter)->win_type, (*iter_layer)->iBestPlaneType);
                          continue;
                      }
                  }

                  //loop plane
                  for(std::vector<DrmPlane*> ::const_iterator iter_plane=(*iter)->planes.begin();
                      !(*iter)->planes.empty() && iter_plane != (*iter)->planes.end(); ++iter_plane)
                  {
                      ALOGD_IF(LogLevel(DBG_DEBUG),"line=%d,crtc=0x%x,plane(%d) is_use=%d,possible_crtc_mask=0x%x",__LINE__,(1<<crtc->pipe()),
                              (*iter_plane)->id(),(*iter_plane)->is_use(),(*iter_plane)->get_possible_crtc_mask());


                      if(!(*iter_plane)->is_use() && (*iter_plane)->GetCrtcSupported(*crtc))
                      {
                          bool bNeed = false;

                          if((*iter_plane)->win_type() & DRM_PLANE_TYPE_CLUSTER0_WIN0){
                                b_cluster0_used = false;
                                b_cluster0_two_win_mode = true;
                                i_cluster0_used_dst_x_offset = 0;
                          }

                          if((*iter_plane)->win_type() & DRM_PLANE_TYPE_CLUSTER1_WIN0){
                                b_cluster1_used = false;
                                b_cluster1_two_win_mode = true;
                                i_cluster1_used_dst_x_offset = 0;
                          }

                          if(b_cluster0_used && ((*iter_plane)->win_type() & DRM_PLANE_TYPE_CLUSTER0_WIN1) == 0)
                            b_cluster0_two_win_mode = false;

                          if(b_cluster1_used && ((*iter_plane)->win_type() & DRM_PLANE_TYPE_CLUSTER1_WIN1) == 0)
                            b_cluster1_two_win_mode = false;

                          if(((*iter_plane)->win_type() & DRM_PLANE_TYPE_CLUSTER0_WIN1) > 0){
                            if(!b_cluster0_two_win_mode){
                              ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) disable Cluster two win mode",(*iter_plane)->id());
                              continue;
                            }
                            int dst_x_offset = (*iter_layer)->display_frame.left;
                            if((i_cluster0_used_dst_x_offset % 2) !=  (dst_x_offset % 2)){
                              b_cluster0_two_win_mode = false;
                              ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) Cluster can't overlay win0-dst-x=%d,win1-dst-x=%d",(*iter_plane)->id(),i_cluster0_used_dst_x_offset,dst_x_offset);
                              continue;
                            }

                          }

                          if(((*iter_plane)->win_type() & DRM_PLANE_TYPE_CLUSTER1_WIN1) > 0){
                            if(!b_cluster1_two_win_mode){
                              ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) disable Cluster two win mode",(*iter_plane)->id());
                              continue;
                            }
                            int dst_x_offset = (*iter_layer)->display_frame.left;

                            if((i_cluster1_used_dst_x_offset % 2) !=  (dst_x_offset % 2)){
                              b_cluster1_two_win_mode = false;
                              ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) Cluster can't overlay win0-dst-x=%d,win1-dst-x=%d",(*iter_plane)->id(),i_cluster1_used_dst_x_offset,dst_x_offset);
                              continue;
                            }
                          }

                          if((*iter_plane)->is_support_format((*iter_layer)->uFourccFormat_,(*iter_layer)->bAfbcd_)){
                            bNeed = true;
                          }else{
                            ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) cann't support format=0x%x afbcd = %d",(*iter_plane)->id(),(*iter_layer)->iFormat_,(*iter_layer)->bAfbcd_);
                            continue;
                          }
                          int input_w = (int)((*iter_layer)->source_crop.right - (*iter_layer)->source_crop.left);
                          int input_h = (int)((*iter_layer)->source_crop.bottom - (*iter_layer)->source_crop.top);
                          if((*iter_plane)->is_support_input(input_w,input_h)){
                            bNeed = true;
                          }else{
                            ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) cann't support intput (%d,%d), max_input_range is (%d,%d)",
                                    (*iter_plane)->id(),input_w,input_h,(*iter_plane)->get_input_w_max(),(*iter_plane)->get_input_h_max());
                            continue;

                          }

                          int output_w = (*iter_layer)->display_frame.right - (*iter_layer)->display_frame.left;
                          int output_h = (*iter_layer)->display_frame.bottom - (*iter_layer)->display_frame.top;

                          if((*iter_plane)->is_support_output(output_w,output_h)){
                            bNeed = true;
                          }else{
                            ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) cann't support output (%d,%d), max_input_range is (%d,%d)",
                                    (*iter_plane)->id(),output_w,output_h,(*iter_plane)->get_output_w_max(),(*iter_plane)->get_output_h_max());
                            continue;

                          }

                          b_scale = (*iter_plane)->get_scale();
                          if((*iter_layer)->bScale_)
                          {
                              if(!b_scale)
                              {
                                  ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) cann't support scale",(*iter_plane)->id());
                                  continue;
                              }
                              else
                              {

                                  if((*iter_plane)->is_support_scale((*iter_layer)->fHScaleMul_) &&
                                      (*iter_plane)->is_support_scale((*iter_layer)->fVScaleMul_))
                                    bNeed = true;
                                  else{
                                    ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) cann't support scale factor(%f,%f)",
                                            (*iter_plane)->id(), (*iter_layer)->fHScaleMul_, (*iter_layer)->fVScaleMul_);
                                    continue;

                                  }
                              }
                          }

                          if ((*iter_layer)->blending == DrmHwcBlending::kPreMult)
                              alpha = (*iter_layer)->alpha;

                          b_alpha = (*iter_plane)->alpha_property().id()?true:false;
                          if(alpha != 0xFF)
                          {
                              if(!b_alpha)
                              {
                                  ALOGV("layer id=%d, plane id=%d",(*iter_layer)->uId_,(*iter_plane)->id());
                                  ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) cann't support alpha,layer alpha=0x%x,alpha id=%d",
                                          (*iter_plane)->id(),(*iter_layer)->alpha,(*iter_plane)->alpha_property().id());
                                  continue;
                              }
                              else
                                  bNeed = true;
                          }

                          eotf = (*iter_layer)->uEOTF;
                          b_hdr2sdr = (*iter_plane)->get_hdr2sdr();

                          if(bHdrSupport && eotf != TRADITIONAL_GAMMA_SDR)
                          {
                              if(!b_hdr2sdr)
                              {
                                  ALOGV("layer id=%d, plane id=%d",(*iter_layer)->uId_,(*iter_plane)->id());
                                  ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) cann't support etof,layer eotf=%d,hdr2sdr=%d",
                                          (*iter_plane)->id(),(*iter_layer)->uEOTF,(*iter_plane)->get_hdr2sdr());
                                  continue;
                              }
                              else
                                  bNeed = true;
                          }
//                          //Reserve some plane with no need for specific features in current layer.
//                          if(!bNeed && !bMulArea)
//                          {
//                              if(!(*iter_layer)->bFbTarget_ && b_afbc)
//                              {
//                                  if(HasGetNoAfbcUsablePlanes(crtc,plane_groups))
//                                  {
//                                      ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) don't need use afbc feature",(*iter_plane)->id());
//                                      continue;
//                                  }
//                              }

//                              if(!(*iter_layer)->bYuv_ && b_yuv)
//                              {
//                                  if(HasGetNoYuvUsablePlanes(crtc,plane_groups))
//                                  {
//                                      ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) don't need use yuv feature",(*iter_plane)->id());
//                                      continue;
//                                  }
//                              }

//                              if(!(*iter_layer)->bScale_ && b_scale)
//                              {
//                                  if(HasGetNoScaleUsablePlanes(crtc,plane_groups))
//                                  {
//                                      ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) don't need use scale feature",(*iter_plane)->id());
//                                      continue;
//                                  }
//                              }

//                              if(alpha == 0xFF && b_alpha)
//                              {
//                                  if(HasGetNoAlphaUsablePlanes(crtc,plane_groups))
//                                  {
//                                      ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) don't need use alpha feature",(*iter_plane)->id());
//                                      continue;
//                                  }
//                              }

//                              if(eotf == TRADITIONAL_GAMMA_SDR && b_hdr2sdr)
//                              {
//                                  if(HasGetNoEotfUsablePlanes(crtc,plane_groups))
//                                  {
//                                      ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) don't need use eotf feature",(*iter_plane)->id());
//                                      continue;
//                                  }
//                              }
//                          }
                          rotation = 0;
                          if ((*iter_layer)->transform & DrmHwcTransform::kFlipH)
                              rotation |= 1 << DRM_REFLECT_X;
                          if ((*iter_layer)->transform & DrmHwcTransform::kFlipV)
                              rotation |= 1 << DRM_REFLECT_Y;
                          if ((*iter_layer)->transform & DrmHwcTransform::kRotate90)
                              rotation |= 1 << DRM_ROTATE_90;
                          else if ((*iter_layer)->transform & DrmHwcTransform::kRotate180)
                              rotation |= 1 << DRM_ROTATE_180;
                          else if ((*iter_layer)->transform & DrmHwcTransform::kRotate270)
                              rotation |= 1 << DRM_ROTATE_270;
                          if(rotation && !(rotation & (*iter_plane)->get_rotate()))
                              continue;

                          ALOGD_IF(LogLevel(DBG_DEBUG),"MatchPlane: match layer id=%d, plane=%d,zops = %d",(*iter_layer)->uId_,
                              (*iter_plane)->id(),*zpos);
                          //Find the match plane for layer,it will be commit.
                          composition_planes->emplace_back(type, (*iter_plane), crtc, (*iter_layer)->iDrmZpos_);
                          (*iter_layer)->bMatch_ = true;
                          (*iter_plane)->set_use(true);
                          composition_planes->back().set_zpos(*zpos);
                          combine_layer_count++;

                          if((*iter_plane)->win_type() & DRM_PLANE_TYPE_CLUSTER0_WIN0){
                              b_cluster0_used = true;
                              i_cluster0_used_dst_x_offset = (*iter_layer)->display_frame.left;
                              if(input_w > 2048 || output_w > 2048 || rotation != 0){
                                  b_cluster0_two_win_mode = false;
                              }else{
                                  b_cluster0_two_win_mode = true;
                              }
                          }else if((*iter_plane)->win_type() & DRM_PLANE_TYPE_CLUSTER1_WIN0){
                              b_cluster0_used = true;
                              i_cluster1_used_dst_x_offset = (*iter_layer)->display_frame.left;
                              if(input_w > 2048 || output_w > 2048 || rotation != 0){
                                  b_cluster1_two_win_mode = false;
                              }else{
                                  b_cluster1_two_win_mode = true;
                              }
                          }
                          break;

                      }
                  }
              }
              if(combine_layer_count == layer_size)
              {
                  ALOGD_IF(LogLevel(DBG_DEBUG),"line=%d all match",__LINE__);
                  //update zpos for the next time.
                   *zpos += 1;
                  (*iter)->bUse = true;
                  return 0;
              }
          }
          /*else
          {
              //1. cut out combine_layer_count to (*iter)->planes.size().
              //2. combine_layer_count layer assign planes.
              //3. extern layers assign planes.
              return false;
          }*/
      }

  }


  return -1;
}

void PlanStageVop2::ResetPlaneGroups(std::vector<PlaneGroup *> &plane_groups){
  for (auto &plane_group : plane_groups){
    for(auto &p : plane_group->planes)
      p->set_use(false);
      plane_group->bUse = false;
  }
  return;
}

void PlanStageVop2::ResetLayer(std::vector<DrmHwcLayer*>& layers){
    for (auto &drmHwcLayer : layers){
      drmHwcLayer->bMatch_ = false;
    }
    return;
}

int PlanStageVop2::MatchBestPlanes(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  composition->clear();
  LayerMap layer_map;
  int ret = CombineLayer(layer_map, layers, plane_groups.size());

  // Fill up the remaining planes
  int zpos = 0;
  for (auto i = layer_map.begin(); i != layer_map.end(); i = layer_map.erase(i)) {
    ret = MatchPlane(composition, plane_groups, DrmCompositionPlane::Type::kLayer,
                      crtc, std::make_pair(i->first, i->second),&zpos, true);
    // We don't have any planes left
    if (ret == -ENOENT){
      ALOGD_IF(LogLevel(DBG_DEBUG),"Failed to match all layer, try other HWC policy ret = %d,line = %d",ret,__LINE__);
      ResetLayer(layers);
      ResetPlaneGroups(plane_groups);
      return ret;
    }else if (ret) {
      ALOGD_IF(LogLevel(DBG_DEBUG),"Failed to match all layer, try other HWC policy ret = %d, line = %d",ret,__LINE__);
      ResetLayer(layers);
      ResetPlaneGroups(plane_groups);
      return ret;
    }
  }

  return 0;
}


int PlanStageVop2::MatchPlanes(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  composition->clear();
  LayerMap layer_map;
  int ret = CombineLayer(layer_map, layers, plane_groups.size());

  // Fill up the remaining planes
  int zpos = 0;
  for (auto i = layer_map.begin(); i != layer_map.end(); i = layer_map.erase(i)) {
    ret = MatchPlane(composition, plane_groups, DrmCompositionPlane::Type::kLayer,
                      crtc, std::make_pair(i->first, i->second),&zpos);
    if (ret) {
      ALOGD_IF(LogLevel(DBG_DEBUG),"Failed to match all layer, try other HWC policy ret = %d, line = %d",ret,__LINE__);
      ResetLayer(layers);
      ResetPlaneGroups(plane_groups);
      composition->clear();
      return ret;
    }
  }

  return 0;
}
int  PlanStageVop2::GetPlaneGroups(DrmCrtc *crtc, std::vector<PlaneGroup *>&out_plane_groups){
  DrmDevice *drm = crtc->getDrmDevice();
  out_plane_groups.clear();
  std::vector<PlaneGroup *> all_plane_groups = drm->GetPlaneGroups();
  for(auto &plane_group : all_plane_groups){
    if(plane_group->acquire(1 << crtc->pipe()))
      out_plane_groups.push_back(plane_group);
  }
  return out_plane_groups.size() > 0 ? 0 : -1;
}

void PlanStageVop2::ResetLayerFromTmpExceptFB(std::vector<DrmHwcLayer*>& layers,
                                              std::vector<DrmHwcLayer*>& tmp_layers){
  for (auto i = layers.begin(); i != layers.end();){
      if((*i)->bFbTarget_){
          tmp_layers.emplace_back(std::move(*i));
          i = layers.erase(i);
          continue;
      }
      i++;
  }
  for (auto i = tmp_layers.begin(); i != tmp_layers.end();){
    if((*i)->bFbTarget_){
      i++;
      continue;
    }
    layers.emplace_back(std::move(*i));
    i = tmp_layers.erase(i);
  }
  //sort
  for (auto i = layers.begin(); i != layers.end()-1; i++){
     for (auto j = i+1; j != layers.end(); j++){
        if((*i)->iZpos_ > (*j)->iZpos_){
           std::swap(*i, *j);
        }
     }
  }

  return;
}


void PlanStageVop2::ResetLayerFromTmp(std::vector<DrmHwcLayer*>& layers,
                                              std::vector<DrmHwcLayer*>& tmp_layers){
  for (auto i = tmp_layers.begin(); i != tmp_layers.end();){
         layers.emplace_back(std::move(*i));
         i = tmp_layers.erase(i);
     }
     //sort
     for (auto i = layers.begin(); i != layers.end()-1; i++){
         for (auto j = i+1; j != layers.end(); j++){
             if((*i)->iZpos_ > (*j)->iZpos_){
                 std::swap(*i, *j);
             }
         }
     }

    return;
}

void PlanStageVop2::MoveFbToTmp(std::vector<DrmHwcLayer*>& layers,
                                       std::vector<DrmHwcLayer*>& tmp_layers){
  for (auto i = layers.begin(); i != layers.end();){
      if((*i)->bFbTarget_){
          tmp_layers.emplace_back(std::move(*i));
          i = layers.erase(i);
          continue;
      }
      i++;
  }
  int zpos = 0;
  for(auto &layer : layers){
    layer->iDrmZpos_ = zpos;
    zpos++;
  }

  zpos = 0;
  for(auto &layer : tmp_layers){
    layer->iDrmZpos_ = zpos;
    zpos++;
  }
  return;
}

void PlanStageVop2::OutputMatchLayer(int iFirst, int iLast,
                                          std::vector<DrmHwcLayer *>& layers,
                                          std::vector<DrmHwcLayer *>& tmp_layers){

  if(iFirst < 0 || iLast < 0 || iFirst > iLast)
  {
      ALOGE("invalid value iFirst=%d, iLast=%d", iFirst, iLast);
      return;
  }

  int interval = layers.size()-1-iLast;
  ALOGD_IF(LogLevel(DBG_DEBUG), "OutputMatchLayer iFirst=%d,iLast,=%d,interval=%d",iFirst,iLast,interval);
  for (auto i = layers.begin() + iFirst; i != layers.end() - interval;)
  {
      //move gles layers
      tmp_layers.emplace_back(std::move(*i));
      i = layers.erase(i);
  }

  //add fb layer.
  int pos = iFirst;
  for (auto i = tmp_layers.begin(); i != tmp_layers.end();)
  {
      if((*i)->bFbTarget_){
          layers.insert(layers.begin() + pos, std::move(*i));
          pos++;
          i = tmp_layers.erase(i);
          continue;
      }
      i++;
  }
  int zpos = 0;
  for(auto &layer : layers){
    layer->iDrmZpos_ = zpos;
    zpos++;
  }
  return;
}
int PlanStageVop2::TryOverlayPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  std::vector<DrmHwcLayer*> tmp_layers;
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  //save fb into tmp_layers
  MoveFbToTmp(layers, tmp_layers);
  int ret = MatchPlanes(composition,layers,crtc,plane_groups);
  if(!ret)
    return ret;
  else{
    ResetLayerFromTmp(layers,tmp_layers);
    return -1;
  }
  return 0;
}
int PlanStageVop2::TryMixSkipPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);

  int skipCnt = 0;

  int iPlaneSize = plane_groups.size();

  if(iReqAfbcdCnt == 0){
    for(auto &plane_group : plane_groups){
      if(plane_group->win_type & DRM_PLANE_TYPE_CLUSTER_MASK)
        iPlaneSize--;
    }
  }

  if(iPlaneSize == 0){
    ALOGE_IF(LogLevel(DBG_DEBUG), "%s:line=%d, iPlaneSize = %d, skip TryMixSkipPolicy",
              __FUNCTION__,__LINE__,iPlaneSize);
  }

  std::vector<DrmHwcLayer *> tmp_layers;
  // Since we can't composite HWC_SKIP_LAYERs by ourselves, we'll let SF
  // handle all layers in between the first and last skip layers. So find the
  // outer indices and mark everything in between as HWC_FRAMEBUFFER
  std::pair<int, int> skip_layer_indices(-1, -1);

  //save fb into tmp_layers
  MoveFbToTmp(layers, tmp_layers);

  //caculate the first and last skip layer
  int i = 0;
  for (auto &layer : layers) {
    if (!layer->bSkipLayer_ && !layer->bGlesCompose_){
      i++;
      continue;
    }

    if (skip_layer_indices.first == -1)
      skip_layer_indices.first = i;
    skip_layer_indices.second = i;
    i++;
  }

  if(skip_layer_indices.first != -1){
    skipCnt = skip_layer_indices.second - skip_layer_indices.first + 1;
  }else{
    ALOGE_IF(LogLevel(DBG_DEBUG), "%s:line=%d, can't find any skip layer, first = %d, second = %d",
              __FUNCTION__,__LINE__,skip_layer_indices.first,skip_layer_indices.second);
    return -1;
  }

  //OPT: Adjust skip_layer_indices.first and skip_layer_indices.second to limit in iPlaneSize.
  if(((int)layers.size() - skipCnt + 1) > iPlaneSize){
      int tmp_index = -1;
      if(skip_layer_indices.first != 0){
        tmp_index = skip_layer_indices.first;
        //try decrease first skip index to 0.
        skip_layer_indices.first = 0;
        skipCnt = skip_layer_indices.second - skip_layer_indices.first + 1;
        if(((int)layers.size() - skipCnt + 1) > iPlaneSize && skip_layer_indices.second != (int)layers.size()-1){
          skip_layer_indices.first = tmp_index;
          tmp_index = skip_layer_indices.second;
          //try increase second skip index to last index.
          skip_layer_indices.second = layers.size()-1;
          skipCnt = skip_layer_indices.second - skip_layer_indices.first + 1;
          if(((int)layers.size() - skipCnt + 1) > iPlaneSize){
            ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d fail match (%d,%d)",__FUNCTION__,__LINE__,skip_layer_indices.first, tmp_index);
            ResetLayerFromTmp(layers,tmp_layers);
            return -1;
          }
        }
      }else{
        if(skip_layer_indices.second != (int)layers.size()-1){
          //try increase second skip index to last index-1.
          skip_layer_indices.second = layers.size()-2;
          skipCnt = skip_layer_indices.second + 1;
          if(((int)layers.size() - skipCnt + 1) > iPlaneSize){
              ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d fail match (%d,%d)",__FUNCTION__,__LINE__,skip_layer_indices.first, tmp_index);
              ResetLayerFromTmp(layers,tmp_layers);
              return -1;
          }
        }else{
          ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d fail match (%d,%d)",__FUNCTION__,__LINE__,skip_layer_indices.first, tmp_index);
          ResetLayerFromTmp(layers,tmp_layers);
          return -1;
        }
     }
  }

  OutputMatchLayer(skip_layer_indices.first, skip_layer_indices.second, layers, tmp_layers);
  int ret = MatchPlanes(composition,layers,crtc,plane_groups);
  if(!ret)
    return ret;
  else{
    int first = skip_layer_indices.first;
    int last = skip_layer_indices.second;
    if(first > (layers.size() - 1 - last) && first != 0){
      for(first--; first >= 0; first--){
        OutputMatchLayer(first, last, layers, tmp_layers);
        int ret = MatchPlanes(composition,layers,crtc,plane_groups);
        if(ret){
          ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d fail match (%d,%d)",__FUNCTION__,__LINE__,first, last);
          ResetLayerFromTmpExceptFB(layers,tmp_layers);
          continue;
        }else
          return ret;
      }
      ResetLayerFromTmp(layers,tmp_layers);
    }else if(last < layers.size()){
      for(last++; last <= layers.size() - 1; last++){
        OutputMatchLayer(first, last, layers, tmp_layers);
        int ret = MatchPlanes(composition,layers,crtc,plane_groups);
        if(ret){
          ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d fail match (%d,%d)",__FUNCTION__,__LINE__,first, last);
          ResetLayerFromTmpExceptFB(layers,tmp_layers);
          continue;
        }else
          return ret;
      }
    }
    ResetLayerFromTmp(layers,tmp_layers);
    return -1;
  }
  return ret;
}

/*************************mix video*************************
 Video ovelay
-----------+----------+------+------+----+------+-------------+--------------------------------+------------------------+------
       HWC | 711aa61700 | 0000 | 0000 | 00 | 0100 | ? 00000017  |    0.0,    0.0, 3840.0, 2160.0 |  600,  562, 1160,  982 | SurfaceView - MediaView
      GLES | 711ab1e580 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0,  560.0,  420.0 |  600,  562, 1160,  982 | MediaView
      GLES | 70b34c9c80 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,    2.0 |    0,    0, 2400,    2 | StatusBar
      GLES | 70b34c9080 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,   84.0 |    0, 1516, 2400, 1600 | taskbar
      GLES | 711ec5a900 | 0000 | 0002 | 00 | 0105 | RGBA_8888   |    0.0,    0.0,   39.0,   49.0 | 1136, 1194, 1175, 1243 | Sprite
************************************************************/
int PlanStageVop2::TryMixVideoPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  std::vector<DrmHwcLayer *> tmp_layers;
  //save fb into tmp_layers
  MoveFbToTmp(layers, tmp_layers);

  int iPlaneSize = plane_groups.size();
  std::pair<int, int> layer_indices(-1, -1);

  if((int)layers.size() < 4)
    layer_indices.first = layers.size() - 2 <= 0 ? 1 : layers.size() - 2;
  else
    layer_indices.first = 3;
  layer_indices.second = layers.size() - 1;
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:mix video (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
  OutputMatchLayer(layer_indices.first, layer_indices.second, layers, tmp_layers);
  int ret = MatchPlanes(composition,layers,crtc,plane_groups);
  if(!ret)
    return ret;
  else{
    ResetLayerFromTmpExceptFB(layers,tmp_layers);
    for(--layer_indices.first; layer_indices.first > 0; --layer_indices.first){
      ResetLayerFromTmpExceptFB(layers,tmp_layers);
      ALOGD_IF(LogLevel(DBG_DEBUG), "%s:mix video (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
      OutputMatchLayer(layer_indices.first, layer_indices.second, layers, tmp_layers);
      int ret = MatchPlanes(composition,layers,crtc,plane_groups);
      if(!ret)
        return ret;
      else{
        ResetLayerFromTmp(layers,tmp_layers);
        return -1;
     }
   }
 }

  ResetLayerFromTmp(layers,tmp_layers);
  return ret;
}

/*************************mix up*************************
-----------+----------+------+------+----+------+-------------+--------------------------------+------------------------+------
       HWC | 711aa61e80 | 0000 | 0000 | 00 | 0100 | RGBx_8888   |    0.0,    0.0, 2400.0, 1600.0 |    0,    0, 2400, 1600 | com.android.systemui.ImageWallpaper
       HWC | 711ab1ef00 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0, 1600.0 |    0,    0, 2400, 1600 | com.android.launcher3/com.android.launcher3.Launcher
       HWC | 711aa61700 | 0000 | 0000 | 00 | 0100 | ? 00000017  |    0.0,    0.0, 3840.0, 2160.0 |  600,  562, 1160,  982 | SurfaceView - MediaView
      GLES | 711ab1e580 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0,  560.0,  420.0 |  600,  562, 1160,  982 | MediaView
      GLES | 70b34c9c80 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,    2.0 |    0,    0, 2400,    2 | StatusBar
      GLES | 70b34c9080 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,   84.0 |    0, 1516, 2400, 1600 | taskbar
      GLES | 711ec5a900 | 0000 | 0002 | 00 | 0105 | RGBA_8888   |    0.0,    0.0,   39.0,   49.0 | 1136, 1194, 1175, 1243 | Sprite
************************************************************/
int PlanStageVop2::TryMixUpPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  std::vector<DrmHwcLayer *> tmp_layers;
  //save fb into tmp_layers
  MoveFbToTmp(layers, tmp_layers);

  int iPlaneSize = plane_groups.size();

  if(iReqAfbcdCnt == 0){
    for(auto &plane_group : plane_groups){
      if(plane_group->win_type & DRM_PLANE_TYPE_CLUSTER_MASK)
        iPlaneSize--;
    }
  }

  if(iPlaneSize == 0){
    ALOGE_IF(LogLevel(DBG_DEBUG), "%s:line=%d, iPlaneSize = %d, skip TryMixSkipPolicy",
              __FUNCTION__,__LINE__,iPlaneSize);
  }



  std::pair<int, int> layer_indices(-1, -1);

  if((int)layers.size() < 4)
    layer_indices.first = layers.size() - 2 <= 0 ? 1 : layers.size() - 2;
  else
    layer_indices.first = 3;
  layer_indices.second = layers.size() - 1;
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:mix video (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
  OutputMatchLayer(layer_indices.first, layer_indices.second, layers, tmp_layers);
  int ret = MatchPlanes(composition,layers,crtc,plane_groups);
  if(!ret)
    return ret;
  else{
    ResetLayerFromTmpExceptFB(layers,tmp_layers);
    for(--layer_indices.first; layer_indices.first > 0; --layer_indices.first){
      ResetLayerFromTmpExceptFB(layers,tmp_layers);
      ALOGD_IF(LogLevel(DBG_DEBUG), "%s:mix video (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
      OutputMatchLayer(layer_indices.first, layer_indices.second, layers, tmp_layers);
      int ret = MatchPlanes(composition,layers,crtc,plane_groups);
      if(!ret)
        return ret;
      else{
        ResetLayerFromTmp(layers,tmp_layers);
        return -1;
     }
   }
 }

  ResetLayerFromTmp(layers,tmp_layers);
  return ret;
}

/*************************mix down*************************
 Sprite layer
-----------+----------+------+------+----+------+-------------+--------------------------------+------------------------+------
      GLES | 711aa61e80 | 0000 | 0000 | 00 | 0100 | RGBx_8888   |    0.0,    0.0, 2400.0, 1600.0 |    0,    0, 2400, 1600 | com.android.systemui.ImageWallpaper
      GLES | 711ab1ef00 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0, 1600.0 |    0,    0, 2400, 1600 | com.android.launcher3/com.android.launcher3.Launcher
      GLES | 711aa61100 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,    2.0 |    0,    0, 2400,    2 | StatusBar
       HWC | 711ec5ad80 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,   84.0 |    0, 1516, 2400, 1600 | taskbar
       HWC | 711ec5a900 | 0000 | 0002 | 00 | 0105 | RGBA_8888   |    0.0,    0.0,   39.0,   49.0 |  941,  810,  980,  859 | Sprite
************************************************************/
int PlanStageVop2::TryMixDownPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  std::vector<DrmHwcLayer *> tmp_layers;

  //save fb into tmp_layers
  MoveFbToTmp(layers, tmp_layers);

  if(layers.size() < 4 || layers.size() > 6 ){
    ResetLayerFromTmp(layers,tmp_layers);
    return -1;
  }

  std::pair<int, int> layer_indices(-1, -1);
  int iPlaneSize = plane_groups.size();
  layer_indices.first = 0;
  layer_indices.second = 2;
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:mix down (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
  OutputMatchLayer(layer_indices.first, layer_indices.second, layers, tmp_layers);
  int ret = MatchPlanes(composition,layers,crtc,plane_groups);
  if(!ret)
    return ret;
  else
    ResetLayerFromTmpExceptFB(layers,tmp_layers);

  if((int)layers.size() > iPlaneSize){
    layer_indices.first = 0;
    layer_indices.second = layers.size() - iPlaneSize;
    ALOGD_IF(LogLevel(DBG_DEBUG), "%s:mix down (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
    OutputMatchLayer(layer_indices.first, layer_indices.second, layers, tmp_layers);
    int ret = MatchPlanes(composition,layers,crtc,plane_groups);
    if(!ret)
      return ret;
    else{
      ResetLayerFromTmp(layers,tmp_layers);
      return -1;
    }
  }

  ResetLayerFromTmp(layers,tmp_layers);
  return ret;
}

int PlanStageVop2::TryMixPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  int ret;
  if(setHwcPolicy.count(HWC_MIX_SKIP_LOPICY)){
    ret = TryMixSkipPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;
    else
      return ret;
  }

  if(setHwcPolicy.count(HWC_MIX_VIDEO_LOPICY)){
    ret = TryMixVideoPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;
  }
  if(setHwcPolicy.count(HWC_MIX_UP_LOPICY)){
    ret = TryMixUpPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;

  }
  if(setHwcPolicy.count(HWC_MIX_DOWN_LOPICY)){
    ret = TryMixDownPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;
  }
  return -1;
}

int PlanStageVop2::TryGLESPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  std::vector<DrmHwcLayer*> fb_target;
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  //save fb into tmp_layers
  MoveFbToTmp(layers, fb_target);

  if(fb_target.size()==1){
    DrmHwcLayer* fb_layer = fb_target[0];
    if(fb_layer->bAfbcd_){
      fb_layer->iBestPlaneType = DRM_PLANE_TYPE_CLUSTER_MASK;
    }else if(fb_layer->bScale_){
      fb_layer->iBestPlaneType = DRM_PLANE_TYPE_ESMART0_MASK | DRM_PLANE_TYPE_ESMART1_MASK;
    }else{
      fb_layer->iBestPlaneType = DRM_PLANE_TYPE_SMART0_MASK | DRM_PLANE_TYPE_SMART1_MASK;
    }

  }
  int ret = MatchBestPlanes(composition,fb_target,crtc,plane_groups);
  if(!ret)
    return ret;
  else{
    ResetLayerFromTmp(layers,fb_target);
    return -1;
  }
  return 0;
}

int PlanStageVop2::TryMatchPolicyFirst(
    std::vector<DrmHwcLayer*> &layers,
    std::vector<PlaneGroup *> &plane_groups,
    bool gles_policy){

  setHwcPolicy.clear();

  //force go into GPU
  int iMode = hwc_get_int_property("vendor.hwc.compose_policy","0");
  if(iMode!=1 || gles_policy){
    setHwcPolicy.insert(HWC_GLES_POLICY);
    return 0;
  }

  // Collect layer info
  iReqAfbcdCnt=0;
  iReqScaleCnt=0;
  iReqYuvCnt=0;
  iReqSkipCnt=0;
  iReqRotateCnt=0;
  iReqHdrCnt=0;
  for(auto &layer : layers){
    if(layer->bFbTarget_)
      continue;

    if(layer->bSkipLayer_ || layer->bGlesCompose_){
      iReqSkipCnt++;
      continue;
    }
    if(layer->bAfbcd_)
      iReqAfbcdCnt++;
    if(layer->bScale_)
      iReqScaleCnt++;
    if(layer->bYuv_)
      iReqYuvCnt++;
    if(layer->transform != DrmHwcTransform::kRotate0)
      iReqRotateCnt++;
    if(layer->bHdr_)
      iReqHdrCnt++;
  }

  // Collect Plane resource info
  iSupportAfbcdCnt=0;
  iSupportScaleCnt=0;
  iSupportYuvCnt=0;
  iSupportRotateCnt=0;
  iSupportHdrCnt=0;

  for(auto &plane_group : plane_groups){
    for(auto &p : plane_group->planes){
      if(p->get_afbc())
        iSupportAfbcdCnt++;
      if(p->get_scale())
        iSupportScaleCnt++;
      if(p->get_yuv())
        iSupportYuvCnt++;
      if(p->get_rotate())
        iSupportRotateCnt++;
      if(p->get_hdr2sdr())
        iSupportHdrCnt++;
      break;
    }
  }

  ALOGD_IF(LogLevel(DBG_DEBUG),"request:afbcd=%d,scale=%d,yuv=%d,rotate=%d,hdr=%d,skip=%d\n"
          "support:afbcd=%d,scale=%d,yuv=%d,rotate=%d,hdr=%d, %s,line=%d,",
          iReqAfbcdCnt,iReqScaleCnt,iReqYuvCnt,iReqRotateCnt,iReqHdrCnt,iReqSkipCnt,
          iSupportAfbcdCnt,iSupportScaleCnt,iSupportYuvCnt,iSupportRotateCnt,iSupportHdrCnt,
          __FUNCTION__,__LINE__);
  // Match policy first
  if(iReqAfbcdCnt <= iSupportAfbcdCnt &&
     iReqScaleCnt <= iSupportScaleCnt &&
     iReqYuvCnt <= iSupportYuvCnt &&
     iReqRotateCnt <= iSupportRotateCnt &&
     iReqSkipCnt == 0)
    setHwcPolicy.insert(HWC_OVERLAY_LOPICY);
  else{
    setHwcPolicy.insert(HWC_MIX_LOPICY);
    setHwcPolicy.insert(HWC_MIX_UP_LOPICY);
    setHwcPolicy.insert(HWC_GLES_POLICY);
    if(iReqYuvCnt > 0) setHwcPolicy.insert(HWC_MIX_VIDEO_LOPICY);
    if(iReqSkipCnt > 0) setHwcPolicy.insert(HWC_MIX_SKIP_LOPICY);
  }

  return 0;
}
int PlanStageVop2::TryHwcPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    bool gles_policy) {

  Init();
  // Get PlaneGroup
  std::vector<PlaneGroup *> plane_groups;
  int ret = GetPlaneGroups(crtc,plane_groups);
  if(ret){
    ALOGE("%s,line=%d can't get plane_groups size=%zu",__FUNCTION__,__LINE__,plane_groups.size());
    return -1;
  }

  // Clear HWC policy list
  TryMatchPolicyFirst(layers,plane_groups,gles_policy);

  // Try to match overlay policy
  if(setHwcPolicy.count(HWC_OVERLAY_LOPICY)){
    ret = TryOverlayPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;
    else{
      ALOGD_IF(LogLevel(DBG_DEBUG),"Match overlay policy fail, try to match other policy.");
      setHwcPolicy.insert(HWC_MIX_LOPICY);
      setHwcPolicy.insert(HWC_MIX_UP_LOPICY);
      setHwcPolicy.insert(HWC_GLES_POLICY);
      if(iSupportYuvCnt > 0) setHwcPolicy.insert(HWC_MIX_VIDEO_LOPICY);
      if(iReqSkipCnt > 0) setHwcPolicy.insert(HWC_MIX_SKIP_LOPICY);
    }
  }

  // Try to match mix policy
  if(setHwcPolicy.count(HWC_MIX_LOPICY)){
    ret = TryMixPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;
    else{
      ALOGD_IF(LogLevel(DBG_DEBUG),"Match mix policy fail, try to match other policy.");
      setHwcPolicy.insert(HWC_GLES_POLICY);
    }
  }

  // Try to match GLES policy
  if(setHwcPolicy.count(HWC_GLES_POLICY)){
    ret = TryGLESPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;
  }

  ALOGE("%s,%d Can't match HWC policy",__FUNCTION__,__LINE__);
  return -1;
}

bool PlanStageVop2::SupportPlatform(uint32_t soc_id){
  switch(soc_id){
    case 0x3566:
    case 0x3568:
    // after ECO
    case 0x3566a:
    case 0x3568a:
      return true;
    default:
      break;
  }
  return false;
}

}

