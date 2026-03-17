/*
* Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*    * Redistributions of source code must retain the above copyright
*      notice, this list of conditions and the following disclaimer.
*    * Redistributions in binary form must reproduce the above
*      copyright notice, this list of conditions and the following
*      disclaimer in the documentation and/or other materials provided
*      with the distribution.
*    * Neither the name of The Linux Foundation nor the names of its
*      contributors may be used to endorse or promote products derived
*      from this software without specific prior written permission.

* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
* ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* Changes from Qualcomm Technologies, Inc. are provided under the following license:
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include "sdm_display.h"
#include "sdm_display_connect.h"
#include "uevent.h"

#ifdef __cplusplus
extern "C" {
#endif

#define __CLASS__ "SdmDisplayConnect"
namespace sdm {

#define SDM_DISPLAY_DEBUG 0

// 32 displays are enough
#define MAX_SUPPORT_DISPLAYS 32

enum {
  FAIL,
  SUCCESS
};

CoreInterface *core_intf_ = NULL;

SdmDisplayBufferAllocator buffer_allocator_;
SdmDisplayBufferSyncHandler buffer_sync_handler_;
SdmDisplaySocketHandler socket_handler_;
HWDisplayInterfaceInfo hw_disp_info_[MAX_SUPPORT_DISPLAYS] = {};
SdmDisplayProxy *display_[MAX_SUPPORT_DISPLAYS] = {0};
HWDisplaysInfo hw_displays_info_ = {};
// ordered by output id
SdmDisplaysInfo sdm_displays_info_ = {};
//store new plug-in displays
static std::map<uint32_t, uint32_t> plug_displays_ = {};

int CreateCore()
{
  DisplayError error = kErrorNone;
  if (core_intf_) {
    DLOGW("Core was already created.");
    return kErrorNone;
  }

  error = CoreInterface::CreateCore(&buffer_allocator_,
                                    &buffer_sync_handler_,
                                    &socket_handler_, nullptr,
                                    &core_intf_);
  if (!core_intf_) {
    DLOGE("function failed. Error = %d", error);
    return error;
  }

  #if SDM_DISPLAY_DEBUG
  DLOGD("successfully created.");
  #endif

  core_intf_->SetMaxBandwidthMode(kBwVFEOn);

  return kErrorNone;
}

int DestroyCore() {
  DisplayError error = kErrorNone;

  if (!core_intf_) {
    DLOGE("Core was already destroyed => core_intf_ = NULL");
    return kErrorNone;
  }

  for(int i = 0; i < MAX_SUPPORT_DISPLAYS; i++) {
    if (display_[i] != NULL) {
      error = display_[i]->DestroyDisplay();
      if (error != kErrorNone) {
        DLOGE("Destroy action failed for display(%d). error = %d",
              i, error);
        DLOGE("Trying to delete display(%d) anyways.", i);
      }
      delete display_[i];
      display_[i] = NULL;
    }
  }

  error = CoreInterface::DestroyCore();
  if (error != kErrorNone) {
    DLOGE("function failed. Error = %d", error);
    return error;
  }
  core_intf_ = NULL;

  #if SDM_DISPLAY_DEBUG
  DLOGD("Core was destroyed successfully");
  #endif

  return kErrorNone;
}

uint32_t GetDisplayCount(void) {
  uint32_t count = 0;

  count = sdm_displays_info_.size();

  return count;
}

void HandlePrimaryDisplayInfo() {
  HWDisplaysInfo::iterator iter = hw_displays_info_.begin();
  int slot = sdm_displays_info_.size();

  for (iter; iter != hw_displays_info_.end(); ++iter) {
    if (!iter->second.is_primary)
      continue;
    if (iter->second.display_type == sdm::kVirtual)
      continue;
    if (!iter->second.is_connected)
      continue;

    // only one primary display
    sdm_displays_info_[slot] = iter->second;
    break;
  }
}

void HandleNonPrimaryDisplayInfos(SDMDisplayType type) {
  HWDisplaysInfo::iterator iter = hw_displays_info_.begin();
  int slot = sdm_displays_info_.size();

  for (iter; iter != hw_displays_info_.end(); ++iter) {
    if (iter->second.is_primary)
      continue;
    if (iter->second.display_type != type)
      continue;
    if (!iter->second.is_connected)
      continue;
    sdm_displays_info_[slot] = iter->second;
    slot++;
  }
}

int GetDisplayInfos(void) {
  DisplayError error = kErrorNone;
  int32_t count = 0;
  HWDisplayInfo primary_disp_info = {};
  int32_t primary_slot = -1;
  bool has_ordered_display = false;

  error = core_intf_->GetDisplaysStatus(&hw_displays_info_);
  if (error != kErrorNone) {
    DLOGE("function GetDisplaysStatus failed. Error = %d", error);
    return error;
  }

  // Only create non-virtual display first
  /* primary display*/
  HandlePrimaryDisplayInfo();
  /* builtin display*/
  HandleNonPrimaryDisplayInfos(sdm::kBuiltIn);
  /* pluggable display*/
  HandleNonPrimaryDisplayInfos(sdm::kPluggable);
  return 0;
}

int UpdateDisplayInfos(void) {
  DisplayError error = kErrorNone;
  int32_t count = 0;
  HWDisplaysInfo disp_info = {};
  int32_t slot = -1;
  //bool has_newplug = false;
  int32_t new_plug = 0;
  bool connected = false;
  uint32_t connector_id;

  plug_displays_.clear();

  slot = sdm_displays_info_.size();
  error = core_intf_->GetDisplaysStatus(&disp_info);
  if (error != kErrorNone) {
    DLOGE("function GetDisplaysStatus failed. Error = %d", error);
    return -1;
  }

  auto iter = disp_info.begin();
  for (iter; iter != disp_info.end(); ++iter) {
    if ((iter->second.display_type == sdm::kVirtual) ||
      (iter->second.display_type == sdm::kBuiltIn))
      continue;

    connected = iter->second.is_connected;
    connector_id = iter->second.display_id;

    auto iter1 = sdm_displays_info_.begin();
    for (iter1; iter1 != sdm_displays_info_.end(); ++iter1) {
      if (iter1->second.display_id == connector_id) {
        if (iter1->second.is_connected != connected) {
          if (display_[iter1->first] != NULL) {
            error = display_[iter1->first]->HandleHotplug(connected);
            /* update previous display info if hotplug success */
            if (error == kErrorNone)
              iter1->second = iter->second;
          } else {
            /* this case happens in create display always fail by previous handling */
            plug_displays_[new_plug] = iter1->first;
            iter1->second = iter->second;
            new_plug++;
          }
        }

        break;
      }
    }

    if (iter1 != sdm_displays_info_.end() || connected == false)
      continue;

    plug_displays_[new_plug] = slot;
    sdm_displays_info_[slot] = iter->second;
    new_plug++;
    slot++;
  }

  return new_plug;
}

void UpdateDisplayStatus(int display_id, bool connected)
{
  auto iter = sdm_displays_info_.find(display_id);

  if (iter != sdm_displays_info_.end())
    iter->second.is_connected = connected;
}

int GetNewPlugDisplayID(int idx)
{
  std::map<uint32_t, uint32_t>::iterator iter = plug_displays_.find(idx);
  if (iter != plug_displays_.end())
    return iter->second;

  DLOGE("No new plug displays\n");

  return -1;
}

char *GetConnectorName(uint32_t display_id) {
  char name[100]={};
  const char *type_name = NULL;
  auto iter = sdm_displays_info_.find(display_id);

  switch(iter->second.display_type) {
    case kBuiltIn:
      type_name = "DSI";
      break;
    case kPluggable:
      type_name = "DP";
      break;
    default:
      type_name = "unKnown";
      break;
  }

  snprintf(name, sizeof name, "%s-%d", type_name, iter->second.display_id);
  return strdup(name);
}

uint32_t GetConnectorId(uint32_t display_id) {
  auto iter = sdm_displays_info_.find(display_id);

  return iter->second.display_id;
}

static HWDisplayInfo GetSdmDisplayInfo(int display_id) {
  auto iter = sdm_displays_info_.find(display_id);

  return iter->second;
}

int CreateDisplay(int display_id) {
  DisplayError error = kErrorNone;
  enum SDMDisplayType display_type = kDisplayMax;
  HWDisplayInfo display_info = {};

  if (display_id >= MAX_SUPPORT_DISPLAYS || display_id < 0) {
    DLOGE("Display id(%d) out of range.", display_id);
    return kErrorParameters;
  }

  if (display_[display_id] != NULL) {
    DLOGE("Display(%d) was already created.", display_id);
    return kErrorNone;
  }

  if (core_intf_ == NULL) {
    DLOGE("Core is not created yet.");
    return kErrorNotSupported;
  }

  display_info = GetSdmDisplayInfo(display_id);
  SdmDisplayProxy *sdm_display = new SdmDisplayProxy(display_info.display_id,
                                                     display_info.display_type,
                                                     core_intf_);
  display_[display_id] = sdm_display;
  error = display_[display_id]->CreateDisplay() ;
  if (error != kErrorNone) {
    DLOGE("Failed to create display(%)", display_id);
    delete display_[display_id];
    display_[display_id] = NULL;

    return error;
  }

  #if SDM_DISPLAY_DEBUG
  DLOGD("Display(%d) created successfully.", display_id);
  #endif

  return kErrorNone;
}

int Prepare(int display_id, struct drm_output *output) {
  DisplayError error = kErrorNone;

  if (display_id >= MAX_SUPPORT_DISPLAYS || display_id < 0) {
    DLOGE("Display id(%d) out of range.", display_id);
    return kErrorParameters;
  }

  if (!display_[display_id]) {
    DLOGE("Failed as Display(%d) not created yet.",
        display_id);
    return kErrorNotSupported;
  }

  error = display_[display_id]->Prepare(output);
  if (error != kErrorNone) {
    DLOGE("function failed with error = %d", error);
    return error;
  }

  #if SDM_DISPLAY_DEBUG
  DLOGD("function successful.");
  #endif

  return kErrorNone;
}

int Commit(int display_id, struct drm_output *output) {
  DisplayError error = kErrorNone;

  if (display_id >= MAX_SUPPORT_DISPLAYS || display_id < 0) {
    DLOGE("Display id(%d) out of range.", display_id);
    return kErrorParameters;
  }

  if (!display_[display_id]) {
    DLOGE("function failed as Display(%d) not created yet.",
        display_id);
    return kErrorNotSupported;
  }

  error = display_[display_id]->Commit(output);
  if (error != kErrorNone) {
    DLOGE("function failed with error = %d", error);
    return error;
  }

  #if SDM_DISPLAY_DEBUG
  DLOGD("function successful.");
  #endif

  return kErrorNone;
}

int Flush(int display_id) {
  DisplayError error = kErrorNone;

  if (display_id >= MAX_SUPPORT_DISPLAYS || display_id < 0) {
    DLOGE("Display id(%d) out of range.", display_id);
    return kErrorParameters;
  }

  if (!display_[display_id]) {
    DLOGE("function failed as Display(%d) not created yet.",
        display_id);
    return kErrorNotSupported;
  }

  error = display_[display_id]->Flush();
  if (error != kErrorNone) {
    DLOGE("function failed with error = %d", error);
    return error;
  }

  #if SDM_DISPLAY_DEBUG
  DLOGD("function successful.");
  #endif

  return kErrorNone;
}

int DestroyDisplay(int display_id) {
  DisplayError error = kErrorNone;

  if (display_id >= MAX_SUPPORT_DISPLAYS || display_id < 0) {
    DLOGE("Display id(%d) out of range.", display_id);
    return kErrorParameters;
  }

  if (!display_[display_id]) {
    DLOGE("Display(%d) was already destroyed.", display_id);
    return kErrorNone;
  }

  SdmDisplayProxy *temp_display = display_[display_id];
  error = temp_display->DestroyDisplay();
  delete temp_display;
  display_[display_id] = NULL;

  if (error != kErrorNone) {
    DLOGE("function failed with error = %d", error);
    return error;
  }

  #if SDM_DISPLAY_DEBUG
  DLOGD("function successful.");
  #endif

  return kErrorNone;
}

bool GetDisplayConfiguration(int display_id, struct DisplayConfigInfo *display_config) {
  DisplayError error = kErrorNone;

  if (display_id >= MAX_SUPPORT_DISPLAYS || display_id < 0) {
    DLOGE("Display id(%d) out of range.", display_id);
    return FAIL;
  }

  if (!display_[display_id]) {
    DLOGE("function failed. Display(%d) not created yet.", display_id);
    return FAIL;
  }

  error = display_[display_id]->GetDisplayConfiguration(display_config);

  if (error != kErrorNone) {
    DLOGE("function failed with error = %d", error);
    return FAIL;
  }

  #if SDM_DISPLAY_DEBUG
  DLOGD("function successful.");
  #endif

  return SUCCESS;
}

bool GetDisplayHdrInfo(int display_id, struct DisplayHdrInfo *display_hdr_info) {
  DisplayError error = kErrorNone;

  if (display_id >= MAX_SUPPORT_DISPLAYS || display_id < 0) {
    DLOGE("Display id(%d) out of range.", display_id);
    return FAIL;
  }

  if (!display_[display_id]) {
    DLOGE("function failed. Display(%d) not created yet.", display_id);
    return FAIL;
  }

  error = display_[display_id]->GetHdrInfo(display_hdr_info);

  if (error != kErrorNone) {
    DLOGE("function failed with error = %d", error);
    return FAIL;
  }

  #if SDM_DISPLAY_DEBUG
  DLOGD("function successful.");
  #endif

  return SUCCESS;
}

int RegisterCbs(int display_id, sdm_cbs *cbs) {
  DisplayError error = kErrorNone;

  if (display_id >= MAX_SUPPORT_DISPLAYS || display_id < 0) {
    DLOGE("Display id(%d) out of range.", display_id);
    return kErrorParameters;
  }

  if (!display_[display_id]) {
    DLOGE("function failed. Display(%d) not created yet.",
        display_id);
    return kErrorParameters;
  }

  error = display_[display_id]->RegisterCbs(display_id, cbs);

  if (error != kErrorNone) {
    DLOGE("function failed with error = %d", error);
    return error;
  }

  #if SDM_DISPLAY_DEBUG
  DLOGD("function successful.");
  #endif

  return kErrorNone;
}

int get_drm_master_fd() {

  int fd = SdmDisplayInterface::GetDrmMasterFd();

  #if SDM_DISPLAY_DEBUG
  DLOGD("fd is: %d \n", fd);
  #endif

  return fd;
}

int SetDisplayState(int display_id, int power_mode) {
  DisplayError error = kErrorNone;

  if (display_id >= MAX_SUPPORT_DISPLAYS || display_id < 0) {
    DLOGE("Display id(%d) out of range.", display_id);
    return kErrorParameters;
  }

  if (!display_[display_id]) {
    DLOGE("function failed. Display(%d) not created yet.",
        display_id);
    return kErrorParameters;
  }

  /* When WESTON_DPMS_ON == 0, set state ON (kStateOn)     */
  /* for all other power modes, i.e. WESTON_DPMS_STANDBY,  */
  /* WESTON_DPMS_SUSPEND, WESTON_DPMS_OFF turn off display */
  /* set state off (kStateOff)                             */
  error = display_[display_id]->SetDisplayState((power_mode == \
                                                WESTON_DPMS_ON)? \
                                                kStateOn: kStateOff);
  if (error != kErrorNone) {
    DLOGE("function failed with error = %d", error);
    return error;
  }

  #if SDM_DISPLAY_DEBUG
  DLOGD("function successful.");
  #endif

  return kErrorNone;
}

int SetVSyncState(int display_id, bool state, struct drm_output *output) {
  DisplayError error = kErrorNone;

  if (display_id >= MAX_SUPPORT_DISPLAYS || display_id < 0) {
    DLOGE("Display id(%d) out of range.", display_id);
    return kErrorParameters;
  }

  if (!display_[display_id]) {
    DLOGE("function failed. Display(%d) not created yet.",
        display_id);
    return kErrorParameters;
  }

  error = display_[display_id]->SetVSyncState(state, output);
  if (error != kErrorNone) {
    DLOGE("function failed with error = %d", error);
    return error;
  }

  #if SDM_DISPLAY_DEBUG
  DLOGD("function successful.");
  #endif

  return kErrorNone;
}

int EnablePllUpdate(int display_id, int enable) {
  return display_[display_id]->EnablePllUpdate(enable);
}

int UpdateDisplayPll(int display_id, int enable) {
  return display_[display_id]->UpdateDisplayPll(enable);
}

int SetPlaneInitState() {
  /* TODO notify SDM the plane state */
  return 0;
}

int SetHead(int display_id, drm_head* head) {
  DisplayError error = kErrorNone;

  if (display_id >= MAX_SUPPORT_DISPLAYS || display_id < 0) {
    DLOGE("Display id(%d) out of range.", display_id);
    return kErrorParameters;
  }

  if (!display_[display_id]) {
    DLOGE("function failed. Display(%d) not created yet.",
        display_id);
    return kErrorParameters;
  }

  error = display_[display_id]->SetHead(head);
  if (error != kErrorNone) {
    DLOGE("function failed with error = %d", error);
    return error;
  }

  return kErrorNone;
}

WL_EXPORT struct sdm_service_interface sdm_service_interface {
  .CreateCore = CreateCore,
  .DestroyCore = DestroyCore,
  .GetDisplayCount = GetDisplayCount,
  .GetDisplayInfos = GetDisplayInfos,
  .CreateDisplay = CreateDisplay,
  .DestroyDisplay = DestroyDisplay,
  .ReconfigureDisplay = NULL,
  .Prepare = Prepare,
  .Commit = Commit,
  .Flush = Flush,
  .GetDisplayConfiguration = GetDisplayConfiguration,
  .GetDisplayHdrInfo = GetDisplayHdrInfo,
  .RegisterCbs = RegisterCbs,
  .SetDisplayState = SetDisplayState,
  .SetVSyncState = SetVSyncState,
  .EnablePllUpdate = EnablePllUpdate,
  .UpdateDisplayPll = UpdateDisplayPll,
  .SetPlaneInitState = SetPlaneInitState,
  .GetConnectorName = GetConnectorName,
  .GetConnectorId = GetConnectorId,
  .UpdateDisplayInfos = UpdateDisplayInfos,
  .GetNewPlugDisplayID = GetNewPlugDisplayID,
  .SetHead = SetHead,
  .UpdateDisplayStatus = UpdateDisplayStatus
};

}// namespace sdm
#ifdef __cplusplus
}
#endif
