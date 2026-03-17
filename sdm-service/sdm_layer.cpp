/* ​​​​Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.​
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <gbm_priv.h>
#include "sdm_layer.h"
#include "sdm_display_debugger.h"
#define __CLASS__ "sdmlayer"

namespace sdm {
void CovertColorMetaToDataSpace(struct ColorMetaData *color_metadata, Dataspace *data_space,
  QtiMatrixCoEfficients *matrixCoefficients)
{
  if (color_metadata) {
    /*todo: translate them one by one */
    data_space->colorPrimaries = (QtiColorPrimaries)color_metadata->colorPrimaries;
    data_space->range = (QtiColorRange)color_metadata->range;
    *matrixCoefficients = (QtiMatrixCoEfficients)color_metadata->matrixCoefficients;
  }
}

bool SetCSC(int32_t color_space, Dataspace *data_space, QtiMatrixCoEfficients *matrixCoefficients) {
  bool csc_updated = false;
  /*
   * As any GBM color space definition can't be 0, if color_space is still 0
   * here, which means nobody touched color space or meta data info before, so
   * we should skip the following update.
   */
  if (!data_space || !matrixCoefficients || !color_space)
    return csc_updated;

  /*
   * A tricky design in gbm is, the color space will be updated according to
   * the color_primaries and range setting in meta data info if
   * GBM_METADATA_SET_COLOR_METADATA was ever called before while calling
   * GBM_METADATA_GET_COLOR_SPACE. In this case, we just update those two fields
   * of meta data.
   */
  if (color_space == GBM_METADATA_COLOR_SPACE_ITU_R_601_FR ||
      color_space == GBM_METADATA_COLOR_SPACE_ITU_R_2020_FR)
    data_space->range = QtiRange_Full;

  switch (color_space) {
    case GBM_METADATA_COLOR_SPACE_ITU_R_601:
    case GBM_METADATA_COLOR_SPACE_ITU_R_601_FR:
      data_space->colorPrimaries = QtiColorPrimaries_BT601_6_625;
      *matrixCoefficients = QtiMatrixCoEff_BT601_6_625;
      csc_updated = true;
      break;
    case GBM_METADATA_COLOR_SPACE_ITU_R_709:
      data_space->colorPrimaries = QtiColorPrimaries_BT709_5;
      *matrixCoefficients = QtiMatrixCoEff_BT709_5;
      csc_updated = true;
      break;
    case GBM_METADATA_COLOR_SPACE_ITU_R_2020:
    case GBM_METADATA_COLOR_SPACE_ITU_R_2020_FR:
      data_space->colorPrimaries = QtiColorPrimaries_BT2020;
      *matrixCoefficients = QtiMatrixCoEff_BT2020;
      csc_updated = true;
      break;
    default:
      DLOGE("unsupported CSC: %d", color_space);
      break;
  }

  return csc_updated;
}
}
