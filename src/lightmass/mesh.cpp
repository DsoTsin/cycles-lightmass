/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <cstdio>
#include <algorithm>

#include "scene.h"
#include "light.h"
#include "mesh.h"
#include "mapping.h"
#include "interchange.h"

CCL_NAMESPACE_BEGIN

CCL_NAMESPACE_END

namespace Lightmass {

//----------------------------------------------------------------------------
//	Mesh base class
//----------------------------------------------------------------------------

void FBaseMesh::Import(FImporter &Importer)
{
  Importer.ImportData((FBaseMeshData *)this);
}

//----------------------------------------------------------------------------
//	Static mesh class
//----------------------------------------------------------------------------

void FStaticMesh::Import(FImporter &Importer)
{
  // import super class
  FBaseMesh::Import(Importer);

  // import the shared data structure
  Importer.ImportData((FStaticMeshData *)this);

  checkf(NumLODs > 0, TEXT("Imported a static mesh with 0 LODs. Uhoh"));

  // create the LODs
  LODs.resize(NumLODs);

  // import each of the LODs
  for (uint32 LODIndex = 0; LODIndex < NumLODs; LODIndex++) {
    // import each LOD separately
    LODs[LODIndex].Import(Importer);
  }
}

//----------------------------------------------------------------------------
//	Static mesh LOD class
//----------------------------------------------------------------------------

void FStaticMeshLOD::Import(class FImporter &Importer)
{
  // import the shared data structure
  Importer.ImportData((FStaticMeshLODData *)this);

  Importer.ImportArray(Elements, NumElements);

  for (uint32 MeshElementIndex = 0; MeshElementIndex < NumElements; MeshElementIndex++) {
    // Only triangle lists are supported for now
    check(Elements[MeshElementIndex].FirstIndex % 3 == 0);
  }
  Importer.ImportArray(Indices, NumIndices);
  Importer.ImportArray(Vertices, NumVertices);
}

void FStaticLightingMesh::SetDebugMaterial(bool bUseDebugMaterial, FLinearColor Diffuse) {}

void FStaticLightingMesh::Import(FImporter &Importer)
{

  // Import into a temporary struct and manually copy settings over,
  // Since the import will overwrite padding in FStaticLightingMeshInstanceData which is actual
  // data in derived classes.
  FStaticLightingMeshInstanceData TempData;
  Importer.ImportData(&TempData);
  int32 MeshNameLen = 0;
  Importer.ImportData(&MeshNameLen);
  ccl::vector<TCHAR> MeshNameCharArray;
  Importer.ImportArray(MeshNameCharArray, MeshNameLen);
  Guid = TempData.Guid;
  NumTriangles = TempData.NumTriangles;
  NumShadingTriangles = TempData.NumShadingTriangles;
  NumVertices = TempData.NumVertices;
  NumShadingVertices = TempData.NumShadingVertices;
  MeshIndex = TempData.MeshIndex;
  LevelGuid = TempData.LevelGuid;
  TextureCoordinateIndex = TempData.TextureCoordinateIndex;
  LightingFlags = TempData.LightingFlags;
  bCastShadowAsTwoSided = TempData.bCastShadowAsTwoSided;
  bMovable = TempData.bMovable;
  NumRelevantLights = TempData.NumRelevantLights;
  BoundingBox = TempData.BoundingBox;
  Importer.ImportGuidArray(RelevantLights, NumRelevantLights, Importer.GetLights());

  int32 NumVisibilityIds = 0;
  Importer.ImportData(&NumVisibilityIds);
  Importer.ImportArray(VisibilityIds, NumVisibilityIds);

  int32 NumMaterialElements = 0;
  Importer.ImportData(&NumMaterialElements);
  check(NumMaterialElements > 0);
  MaterialElements.resize(NumMaterialElements);
  for (int32 MtrlIdx = 0; MtrlIdx < NumMaterialElements; MtrlIdx++) {
    FMaterialElement &CurrentMaterialElement = MaterialElements[MtrlIdx];
    FMaterialElementData METempData;
    Importer.ImportData(&METempData);
    CurrentMaterialElement.MaterialHash = METempData.MaterialHash;
    CurrentMaterialElement.bUseTwoSidedLighting = METempData.bUseTwoSidedLighting;
    CurrentMaterialElement.bShadowIndirectOnly = METempData.bShadowIndirectOnly;
    CurrentMaterialElement.bUseEmissiveForStaticLighting =
        METempData.bUseEmissiveForStaticLighting;
    CurrentMaterialElement.bUseVertexNormalForHemisphereGather =
        METempData.bUseVertexNormalForHemisphereGather;
    // Validating data here instead of in Unreal since EmissiveLightFalloffExponent is used in so
    // many different object types
    CurrentMaterialElement.EmissiveLightFalloffExponent = std::max(
        METempData.EmissiveLightFalloffExponent, 0.0f);
    CurrentMaterialElement.EmissiveLightExplicitInfluenceRadius = std::max(
        METempData.EmissiveLightExplicitInfluenceRadius, 0.0f);
    CurrentMaterialElement.EmissiveBoost = METempData.EmissiveBoost;
    CurrentMaterialElement.DiffuseBoost = METempData.DiffuseBoost;
    CurrentMaterialElement.FullyOccludedSamplesFraction = METempData.FullyOccludedSamplesFraction;
    CurrentMaterialElement.Material = Importer.ConditionalImportObject<FMaterial>(
        CurrentMaterialElement.MaterialHash,
        LM_MATERIAL_VERSION,
        LM_MATERIAL_EXTENSION,
        LM_MATERIAL_CHANNEL_FLAGS,
        Importer.GetMaterials());
    //checkf(CurrentMaterialElement.Material,
    //       TEXT("Failed to import material with Hash %s"),
    //       *CurrentMaterialElement.MaterialHash.ToString());

    const bool bMasked = CurrentMaterialElement.Material->BlendMode == BLEND_Masked;

    CurrentMaterialElement.bIsMasked = bMasked &&
                                       CurrentMaterialElement.Material->TransmissionSize > 0;
    CurrentMaterialElement.bIsTwoSided = CurrentMaterialElement.Material->bTwoSided;
    CurrentMaterialElement.bTranslucent = !CurrentMaterialElement.bIsMasked &&
                                          CurrentMaterialElement.Material->TransmissionSize > 0;
    CurrentMaterialElement.bCastShadowAsMasked =
        CurrentMaterialElement.Material->bCastShadowAsMasked;
    CurrentMaterialElement.bSurfaceDomain = CurrentMaterialElement.Material->bSurfaceDomain;
  }
  bColorInvalidTexels = true;
  bUseDebugMaterial = false;
//  DebugDiffuse = FLinearColor::Black;
}

void FLandscapeStaticLightingMesh::Import(class FImporter &Importer)
{
  // import super class
  FStaticLightingMesh::Import(Importer);
  Importer.ImportData((FLandscapeStaticLightingMeshData *)this);

  // we have the guid for the mesh, now hook it up to the actual static mesh
  int32 ReadSize = ccl::sqr(ComponentSizeQuads + 2 * ExpandQuadsX + 1);
  checkf(ReadSize > 0, TEXT("Failed to import Landscape Heightmap data!"));
  Importer.ImportArray(HeightMap, ReadSize);
  check(HeightMap.size() == ReadSize);

  NumVertices = ComponentSizeQuads + 2 * ExpandQuadsX + 1;
  NumQuads = NumVertices - 1;
  UVFactor = LightMapRatio / NumVertices;
  //todo: bReverseWinding = (LocalToWorld.RotDeterminant() < 0.0f);
}

void FLandscapeStaticLightingMesh::GetTriangleIndices(int32 TriangleIndex,
                                                      int32 &OutI0,
                                                      int32 &OutI1,
                                                      int32 &OutI2) const
{
  // OutI0 = OutI1 = OutI2 = 0;
  int32 QuadIndex = TriangleIndex >> 1;
  int32 QuadTriIndex = TriangleIndex & 1;

  GetTriangleIndices(QuadIndex, QuadTriIndex, OutI0, OutI1, OutI2);
}

// FStaticLightingMesh interface.

void FStaticMeshStaticLightingMesh::GetTriangleIndices(int32 TriangleIndex,
                                                       int32 &OutI0,
                                                       int32 &OutI1,
                                                       int32 &OutI2) const
{
  const FStaticMeshLOD &LODRenderData = StaticMesh->GetLOD(GetMeshLODIndex());

  // Lookup the triangle's vertex indices.
  OutI0 = LODRenderData.GetIndex(TriangleIndex * 3 + 0);
  OutI1 = LODRenderData.GetIndex(TriangleIndex * 3 + (bReverseWinding ? 2 : 1));
  OutI2 = LODRenderData.GetIndex(TriangleIndex * 3 + (bReverseWinding ? 1 : 2));
}

static void GetStaticLightingVertex(const FStaticMeshVertex &InVertex,
                                    //const FMatrix &LocalToWorld,
                                    //const FMatrix &LocalToWorldInverseTranspose,
                                    //bool bIsSplineMesh,
                                    //const FSplineMeshParams &SplineParams,
                                    FStaticLightingVertexData &OutVertex)
{
    OutVertex.WorldPosition = InVertex.Position;
    OutVertex.WorldTangentX = ccl::safe_normalize(InVertex.TangentX);
    OutVertex.WorldTangentY = ccl::safe_normalize(InVertex.TangentY);
    OutVertex.WorldTangentZ =ccl::safe_normalize(InVertex.TangentZ);

  // WorldTangentZ can end up a 0 vector if it was small to begin with and LocalToWorld contains
  // large scale factors.
  if (ccl::abs( ccl::len( OutVertex.WorldTangentZ ) - 1.0) > 0.0001f ) {
      OutVertex.WorldTangentZ = ccl::safe_normalize(ccl::cross(OutVertex.WorldTangentX, OutVertex.WorldTangentY));
  }

  for (uint32 LightmapTextureCoordinateIndex = 0; LightmapTextureCoordinateIndex < MAX_TEXCOORDS;
       LightmapTextureCoordinateIndex++)
  {
    OutVertex.TextureCoordinates[LightmapTextureCoordinateIndex] = InVertex.UVs[LightmapTextureCoordinateIndex];
  }
}


void FStaticMeshStaticLightingMesh::GetNonTransformedTriangle(int32 TriangleIndex,
                                                              FStaticLightingVertexData &OutV0,
                                                              FStaticLightingVertexData &OutV1,
                                                              FStaticLightingVertexData &OutV2,
                                                              int32 &ElementIndex) const
{

  const FStaticMeshLOD &LODRenderData = StaticMesh->GetLOD(GetMeshLODIndex());

  // Lookup the triangle's vertex indices.
  const uint32 I0 = LODRenderData.GetIndex(TriangleIndex * 3 + 0);
  const uint32 I1 = LODRenderData.GetIndex(TriangleIndex * 3 + 1);
  const uint32 I2 = LODRenderData.GetIndex(TriangleIndex * 3 + 2);
  GetStaticLightingVertex(LODRenderData.GetVertex(I0), OutV0);
  GetStaticLightingVertex(LODRenderData.GetVertex(I1), OutV1);
  GetStaticLightingVertex(LODRenderData.GetVertex(I2), OutV2);
  const FStaticMeshLOD &MeshLOD = StaticMesh->GetLOD(GetMeshLODIndex());
  ElementIndex = -1;
  for (uint32 MeshElementIndex = 0; MeshElementIndex < MeshLOD.NumElements; MeshElementIndex++) {
    const FStaticMeshElement &CurrentElement = MeshLOD.GetElement(MeshElementIndex);
    if ((uint32)TriangleIndex >= CurrentElement.FirstIndex / 3 &&
        (uint32)TriangleIndex < CurrentElement.FirstIndex / 3 + CurrentElement.NumTriangles)
    {
      ElementIndex = MeshElementIndex;
      break;
    }
  }
  check(ElementIndex >= 0);
}

void FStaticMeshStaticLightingMesh::GetNonTransformedTriangleIndices(int32 TriangleIndex,
                                                                     int32 &OutI0,
                                                                     int32 &OutI1,
                                                                     int32 &OutI2) const
{
  const FStaticMeshLOD &LODRenderData = StaticMesh->GetLOD(GetMeshLODIndex());

  // Lookup the triangle's vertex indices.
  OutI0 = LODRenderData.GetIndex(TriangleIndex * 3 + 0);
  OutI1 = LODRenderData.GetIndex(TriangleIndex * 3 + 1);
  OutI2 = LODRenderData.GetIndex(TriangleIndex * 3 + 2);
}

bool FStaticMeshStaticLightingMesh::IsElementCastingShadow(int32 ElementIndex) const
{
  const FStaticMeshLOD &LODRenderData = StaticMesh->GetLOD(GetMeshLODIndex());
  const FStaticMeshElement &Element = LODRenderData.GetElement(ElementIndex);
  return Element.bEnableShadowCasting;
}

void FStaticMeshStaticLightingMesh::Import(FImporter &Importer)
{
  // import super class
  FStaticLightingMesh::Import(Importer);

  // import the shared data
  //	Importer.ImportData( (FStaticMeshStaticLightingMeshData*) this );
  FStaticMeshStaticLightingMeshData TempSMSLMD;
  Importer.ImportData(&TempSMSLMD);
  EncodedLODIndices = TempSMSLMD.EncodedLODIndices;
  EncodedHLODRange = TempSMSLMD.EncodedHLODRange;
  //todo: LocalToWorld = TempSMSLMD.LocalToWorld;
  bReverseWinding = TempSMSLMD.bReverseWinding;
  bShouldSelfShadow = TempSMSLMD.bShouldSelfShadow;
  StaticMeshGuid = TempSMSLMD.StaticMeshGuid;
  bIsSplineMesh = TempSMSLMD.bIsSplineMesh;
  SplineParameters = TempSMSLMD.SplineParameters;

  // calculate the inverse transpose
  // todo: WorldToLocal = LocalToWorld.InverseFast();
  // todo: LocalToWorldInverseTranspose = LocalToWorld.InverseFast().GetTransposed();

  // we have the guid for the mesh, now hook it up to the actual static mesh
  StaticMesh = Importer.ConditionalImportObject<FStaticMesh>(StaticMeshGuid,
                                                             LM_STATICMESH_VERSION,
                                                             LM_STATICMESH_EXTENSION,
                                                             LM_STATICMESH_CHANNEL_FLAGS,
                                                             Importer.GetStaticMeshes());
  /*checkf(
      StaticMesh, TEXT("Failed to import static mesh with GUID %s"), *StaticMeshGuid.ToString());*/
  check(GetMeshLODIndex() >= 0 && GetMeshLODIndex() < (int32)StaticMesh->NumLODs);
  checkf(StaticMesh->GetLOD(GetMeshLODIndex()).NumElements == MaterialElements.size(),
         TEXT("Static mesh element count did not match mesh instance element count!"));
}

void FFluidSurfaceStaticLightingMesh::GetTriangleIndices(int32 TriangleIndex,
                                                         int32 &OutI0,
                                                         int32 &OutI1,
                                                         int32 &OutI2) const
{
  OutI0 = QuadIndices[TriangleIndex * 3 + 0];
  OutI1 = QuadIndices[TriangleIndex * 3 + 1];
  OutI2 = QuadIndices[TriangleIndex * 3 + 2];
}

void FFluidSurfaceStaticLightingMesh::Import(class FImporter &Importer)
{
  // import super class
  FStaticLightingMesh::Import(Importer);
  Importer.ImportData((FFluidSurfaceStaticLightingMeshData *)this);
  check(MaterialElements.size() > 0);
}

}  // namespace Lightmass
