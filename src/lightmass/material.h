/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util.h"
#include "texture.h"

CCL_NAMESPACE_BEGIN

CCL_NAMESPACE_END

namespace Lightmass {
#pragma pack(push, 1)
struct FMaterialFileHeader {
  /** FourCC cookie: 'MTRL' */
  uint32 Cookie;
  FGuid FormatVersion;
};

//----------------------------------------------------------------------------
//	Base material
//----------------------------------------------------------------------------
struct FBaseMaterialData {
  FGuid Guid;
};

//----------------------------------------------------------------------------
//	Material data, builds upon FBaseMaterialData
//----------------------------------------------------------------------------
/**
 *	Material blend mode.
 *	MUST MATCH UNREAL EXACTLY!!!
 */
enum EBlendMode {
  BLEND_Opaque = 0,
  BLEND_Masked = 1,
  BLEND_Translucent = 2,
  BLEND_Additive = 3,
  BLEND_Modulate = 4,
  BLEND_AlphaComposite = 5,
  BLEND_AlphaHoldout = 6,
  BLEND_MAX = 7,
};

struct FMaterialData {
  /** The BLEND mode of the material */
  EBlendMode BlendMode;
  /** Whether the material is two-sided or not */
  uint32 bTwoSided : 1;
  /** Whether the material should cast shadows as masked even though it has a translucent blend
   * mode. */
  uint32 bCastShadowAsMasked : 1;
  uint32 bSurfaceDomain : 1;
  /** Scales the emissive contribution for this material. */
  float EmissiveBoost;
  /** Scales the diffuse contribution for this material. */
  float DiffuseBoost;
  /** The clip value for masked rendering */
  float OpacityMaskClipValue;

  /**
   *	The sizes of the material property samples
   */
  int32 EmissiveSize;
  int32 DiffuseSize;
  int32 TransmissionSize;
  int32 NormalSize;

  FMaterialData()
      : EmissiveBoost(1.0f),
        DiffuseBoost(1.0f),
        EmissiveSize(0),
        DiffuseSize(0),
        TransmissionSize(0),
        NormalSize(0)
  {
  }
};
#pragma pack(pop)

//----------------------------------------------------------------------------
//	Material base class
//----------------------------------------------------------------------------

class FBaseMaterial : public FBaseMaterialData {
 public:
  virtual ~FBaseMaterial() {}
  virtual void Import(class FImporter &Importer);
};

//----------------------------------------------------------------------------
//	Material class
//----------------------------------------------------------------------------
class FMaterial : public FBaseMaterial, public FMaterialData {
 public:
  virtual void Import(class FImporter &Importer);

  FORCEINLINE const FTexture2D &Emissive() const
  {
    return MaterialEmissive;
  }
  FORCEINLINE const FTexture2D &Diffuse() const
  {
    return MaterialDiffuse;
  }
  FORCEINLINE const FTexture2D &Transmission() const
  {
    return MaterialTransmission;
  }
  FORCEINLINE const FTexture2D &Normal() const
  {
    return MaterialNormal;
  }

 protected:
  FTexture2D MaterialEmissive;
  FTexture2D MaterialDiffuse;
  FTexture2D MaterialTransmission;
  FTexture2D MaterialNormal;
};

}
