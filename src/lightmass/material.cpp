/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <cstdio>
#include <algorithm>
#include "material.h"
#include "interchange.h"

CCL_NAMESPACE_BEGIN

CCL_NAMESPACE_END

namespace Lightmass {
//----------------------------------------------------------------------------
//	Material base class
//----------------------------------------------------------------------------
void FBaseMaterial::Import(class FImporter &Importer)
{
  verify(Importer.ImportData((FBaseMaterialData *)this));
}

//----------------------------------------------------------------------------
//	Material class
//----------------------------------------------------------------------------
void FMaterial::Import(class FImporter &Importer)
{
  // import super class
  FBaseMaterial::Import(Importer);

  // import the shared data structure
  verify(Importer.ImportData((FMaterialData *)this));

  // import the actual material samples...
  int32 ReadSize;

  // Emissive
  ReadSize = EmissiveSize * EmissiveSize;
  checkf(ReadSize > 0, TEXT("Failed to import emissive data!"));
  MaterialEmissive.Init(TF_ARGB16F, EmissiveSize, EmissiveSize);
  verify(Importer.Read(MaterialEmissive.GetData(), ReadSize * sizeof(FFloat16Color)));

  // Diffuse
  ReadSize = DiffuseSize * DiffuseSize;
  if (ReadSize > 0) {
    MaterialDiffuse.Init(TF_ARGB16F, DiffuseSize, DiffuseSize);
    verify(Importer.Read(MaterialDiffuse.GetData(), ReadSize * sizeof(FFloat16Color)));
  }
  else {
    // Opaque materials should always import diffuse
    check(BlendMode != BLEND_Opaque && BlendMode != BLEND_Masked);
  }

  // Transmission
  ReadSize = TransmissionSize * TransmissionSize;
  if (ReadSize > 0) {
    MaterialTransmission.Init(TF_ARGB16F, TransmissionSize, TransmissionSize);
    verify(Importer.Read(MaterialTransmission.GetData(), ReadSize * sizeof(FFloat16Color)));
  }
  else {
    // Materials with a translucent blend mode should always import transmission
    check(BlendMode != BLEND_Translucent && BlendMode != BLEND_Additive &&
          BlendMode != BLEND_Modulate && BlendMode != BLEND_AlphaComposite &&
          BlendMode != BLEND_AlphaHoldout);
  }

  // Normal
  ReadSize = NormalSize * NormalSize;
  if (ReadSize > 0) {
    MaterialNormal.Init(TF_ARGB16F, NormalSize, NormalSize);
    verify(Importer.Read(MaterialNormal.GetData(), ReadSize * sizeof(FFloat16Color)));
  }
}

}
