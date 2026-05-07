/* ​​​​Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.​
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#ifndef __SDM_LAYER_H__
#define __SDM_LAYER_H__

#include "config.h"
#include <color_metadata.h>
#include <QtiMatrixCoEfficients.h>
#include <Dataspace.h>
#include <QtiColorRange.h>
#include "sdm_display_interface.h"
#include <utils/fence.h>
#include <core/sdm_types.h>

namespace sdm {
using Dataspace = vendor_qti_hardware_display_common_Dataspace;
using QtiColorPrimaries = vendor_qti_hardware_display_common_QtiColorPrimaries;
using QtiColorRange = vendor_qti_hardware_display_common_QtiColorRange;
using QtiMatrixCoEfficients = vendor_qti_hardware_display_common_QtiMatrixCoEfficients;
void CovertColorMetaToDataSpace(struct ColorMetaData *color_metadata, Dataspace *data_space,
  QtiMatrixCoEfficients *matrixCoefficients);
bool SetCSC(int32_t color_space, Dataspace *data_space, QtiMatrixCoEfficients *matrixCoefficients);

/* Layer geometry information filled by compositor */
/* TODO: Check if LayerGeometry from sdm layer could be re-used  */
struct LayerGeometry {
  /* Buffer information */
  uint32_t               width;
  uint32_t               height;
  uint32_t               unaligned_width;
  uint32_t               unaligned_height;
  uint32_t               format;
  uint32_t               fb_id;
  uint32_t               handle_id;
  int32_t                ion_fd;
  /* Layer information */
  uint32_t               composition; /*GPU, Overlay, HWCursor*/
  struct Rect            src_rect; /* srouce rectangle */
  struct Rect            dst_rect; /* destination rectangle */
  struct RectArray       visible_regions;
  struct RectArray       dirty_regions;
  uint32_t               blending;
  uint32_t               transform;
  uint16_t             plane_alpha; /* global alpha */
  struct LayerGeometryFlags        flags;
  Dataspace dataspace;
  QtiMatrixCoEfficients matrixCoefficients;

  /*Hook for storing information relative to compositor. DO NOT MODIFY IT!!!*/
  const void *usr_data;
  shared_ptr<Fence> acquire_fence;
};

}
#endif   //__SDM_LAYER_H__
