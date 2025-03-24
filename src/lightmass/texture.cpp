/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <cstdio>
#include <algorithm>

#include "texture.h"

CCL_NAMESPACE_BEGIN

CCL_NAMESPACE_END

namespace Lightmass {
void FTexture2D::Init(ETexture2DFormats InFormat, int32 InSizeX, int32 InSizeY)
{
  check(InSizeX > 0 && InSizeY > 0);
  SizeX = InSizeX;
  SizeY = InSizeY;
  Format = InFormat;

  // Only supporting these formats
  check(InFormat == TF_ARGB8 || InFormat == TF_ARGB16F);

  switch (InFormat) {
    case TF_ARGB8:
      ElementSize = sizeof(FColor);
      break;
    case TF_ARGB16F:
      ElementSize = sizeof(FFloat16Color);
      break;
  }

  Data = (uint8 *)(ccl::util_aligned_malloc(ElementSize * SizeX * SizeY, 4));
  memset(Data, ElementSize * SizeX * SizeY, 0);
}
void FTexture2D::Release()
{
  if (Data) {
    ccl::util_aligned_free(Data, ElementSize * SizeX * SizeY);
    Data = nullptr;
  }
}
}  // namespace Lightmass
