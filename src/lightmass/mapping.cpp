/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <cstdio>
#include <algorithm>
#include "scene.h"
#include "mesh.h"
#include "interchange.h"
#include "mapping.h"


CCL_NAMESPACE_BEGIN

CCL_NAMESPACE_END

namespace Lightmass {

void FStaticLightingMapping::Import(class FImporter &Importer)
{
  Importer.ImportData((FStaticLightingMappingData *)this);
  auto iter = Importer.GetStaticMeshInstances().find(Guid);
  if (iter != Importer.GetStaticMeshInstances().end()) {
    Mesh = iter->second;
  }
}

void FStaticLightingTextureMapping::Import(class FImporter &Importer)
{
  FStaticLightingMapping::Import(Importer);
  Importer.ImportData((FStaticLightingTextureMappingData *)this);
  CachedSizeX = SizeX;
  CachedSizeY = SizeY;

  SurfaceCacheSizeX = 0;
  SurfaceCacheSizeY = 0;
}

void FStaticMeshStaticLightingTextureMapping::Import(FImporter &Importer)
{
  // import the super class
  FStaticLightingTextureMapping::Import(Importer);
  check(Mesh);
}

void FLandscapeStaticLightingTextureMapping::Import(class FImporter &Importer)
{
  FStaticLightingTextureMapping::Import(Importer);

  // Can't use the FStaticLightingMapping Import functionality for this
  // as it only looks in the StaticMeshInstances map...
  Mesh = Importer.GetLandscapeMeshInstances().find(Guid)->second;
  check(Mesh);
}

void FFluidSurfaceStaticLightingTextureMapping::Import(class FImporter &Importer)
{
  FStaticLightingTextureMapping::Import(Importer);

  // Can't use the FStaticLightingMapping Import functionality for this
  // as it only looks in the StaticMeshInstances map...
  Mesh = Importer.GetFluidMeshInstances().find(Guid)->second;
  check(Mesh);
}
void FBSPSurfaceStaticLighting::GetTriangleIndices(int32 TriangleIndex,
                                                   int32 &OutI0,
                                                   int32 &OutI1,
                                                   int32 &OutI2) const
{
  OutI0 = TriangleVertexIndices[TriangleIndex * 3 + 0];
  OutI1 = TriangleVertexIndices[TriangleIndex * 3 + 1];
  OutI2 = TriangleVertexIndices[TriangleIndex * 3 + 2];
}

void FBSPSurfaceStaticLighting::Import(FImporter &Importer)
{
  FStaticLightingMesh::Import(Importer);
  Mapping.Import(Importer);
  // bsp mapping/mesh are the same
  Mapping.Mesh = this;

  Importer.ImportData((FBSPSurfaceStaticLightingData *)this);
  Importer.ImportArray(Vertices, NumVertices);
  Importer.ImportArray(TriangleVertexIndices, NumTriangles * 3);
  Importer.ImportArray(TriangleLightmassSettings, NumTriangles);

  // Ignore invalid BSP lightmap UV's since they are generated and not something that artists have
  // direct control over.
  //@todo - fix the root cause that results in some texels having their centers mapped to multiple
  // texels.
  bColorInvalidTexels = false;
}
}