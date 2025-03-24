/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util.h"

CCL_NAMESPACE_BEGIN

CCL_NAMESPACE_END

namespace Lightmass {
/** Texture formats used by FTexture2D */
enum ETexture2DFormats { TF_UNKNOWN, TF_ARGB8, TF_ARGB16F };

/** A 2D texture */
class FTexture2D {
 protected:
  /** Texture dimensions */
  int32 SizeX;
  int32 SizeY;
  /** Format of texture which indicates how to interpret Data */
  ETexture2DFormats Format;
  /** The size of each element in the data array */
  int32 ElementSize;
  /** Mip 0 texture data */
  uint8 *Data;

 public:
  FTexture2D() : SizeX(0), SizeY(0), Format(TF_UNKNOWN), ElementSize(0), Data(NULL) {}

  FTexture2D(ETexture2DFormats InFormat, int32 InSizeX, int32 InSizeY)
      : SizeX(InSizeX), SizeY(InSizeY), Format(InFormat), ElementSize(0), Data(NULL)
  {
    Init(InFormat, InSizeX, InSizeY);
  }

  virtual ~FTexture2D()
  {
    Release();
  }

  /** Accessors */
  int32 GetSizeX() const
  {
    return SizeX;
  }
  int32 GetSizeY() const
  {
    return SizeY;
  }
  uint8 *GetData() const
  {
    return Data;
  }
  ETexture2DFormats GetFormat() const
  {
    return Format;
  }
  size_t GetMemorySize() const
  {
    return (size_t)ElementSize * SizeX * SizeY;
  }
  void Init(ETexture2DFormats InFormat, int32 InSizeX, int32 InSizeY);
  void Release();
};

}