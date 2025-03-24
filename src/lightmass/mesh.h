/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util.h"

#include "material.h"

CCL_NAMESPACE_BEGIN

CCL_NAMESPACE_END

namespace Lightmass {
//----------------------------------------------------------------------------
//	Mesh export file header
//----------------------------------------------------------------------------
struct FMeshFileHeader {
  /** FourCC cookie: 'MESH' */
  uint32 Cookie;
  FGuid FormatVersion;

  // These structs follow immediately after this struct.
  //
  //	FBaseMeshData			BaseMeshData;
  //	FStaticMeshData			MeshData;
  //	StaticMeshLODAggregate	MeshLODs[ MeshData.NumLODs ];
  //
  //	Where:
  //
  // 	struct StaticMeshLODAggregate
  // 	{
  //		FStaticMeshLODData		LOD;
  // 		FStaticMeshElementData	MeshElements[ LOD.NumElements ];
  //		UINT16					Indices[ LOD.NumIndices ];
  //		FStaticMeshVertex		Vertices[ LOD.NumVertices ];
  // 	};
};

//----------------------------------------------------------------------------
//	Base mesh
//----------------------------------------------------------------------------
struct FBaseMeshData {
  FGuid Guid;
};

//----------------------------------------------------------------------------
//	Static mesh, builds upon FBaseMeshData
//----------------------------------------------------------------------------
struct FStaticMeshData {
  uint32 LightmapCoordinateIndex;
  uint32 NumLODs;
};

//----------------------------------------------------------------------------
//	Static mesh LOD
//----------------------------------------------------------------------------
struct FStaticMeshLODData {
  uint32 NumElements;
  /** Total number of triangles for all elements in the LOD. */
  uint32 NumTriangles;
  /** Total number of indices in the LOD. */
  uint32 NumIndices;
  /** Total number of vertices in the LOD. */
  uint32 NumVertices;
};

//----------------------------------------------------------------------------
//	Static mesh element
//----------------------------------------------------------------------------
struct FStaticMeshElementData {
  uint32 FirstIndex;
  uint32 NumTriangles;
  uint32 bEnableShadowCasting : 1;
};

//----------------------------------------------------------------------------
//	Static mesh vertex
//----------------------------------------------------------------------------
struct FStaticMeshVertex {
  FVector4 Position;
  FVector4 TangentX;
  FVector4 TangentY;
  FVector4 TangentZ;
  FVector2D UVs[MAX_TEXCOORDS];
};

class FStaticMeshLOD;
class FStaticMeshElement;

//----------------------------------------------------------------------------
//	Mesh base class
//----------------------------------------------------------------------------

class FBaseMesh : public FBaseMeshData {
 public:
  virtual ~FBaseMesh() {}
  virtual void Import(class FImporter &Importer);
};

//----------------------------------------------------------------------------
//	Static mesh class
//----------------------------------------------------------------------------

class FStaticMesh : public FBaseMesh, public FStaticMeshData {
 public:
  virtual void Import(class FImporter &Importer);

  /**
   * @return the given LOD by index
   */
  inline const FStaticMeshLOD &GetLOD(int32 Index) const
  {
    return LODs[Index];
  }

  class FDefaultAggregateMesh *VoxelizationMesh = nullptr;

 protected:
  /** array of LODs (same number as FStaticMeshData.NumLODs) */
  ccl::vector<FStaticMeshLOD> LODs;
};

//----------------------------------------------------------------------------
//	Static mesh LOD class
//----------------------------------------------------------------------------

class FStaticMeshLOD : public FStaticMeshLODData {
 public:
  virtual ~FStaticMeshLOD() {}
  virtual void Import(class FImporter &Importer);

  /**
   * @return the given element by index
   */
  inline const FStaticMeshElement &GetElement(int32 Index) const
  {
    return Elements[Index];
  }

  /**
   * @return the given vertex index by index
   */
  inline const uint32 GetIndex(int32 Index) const
  {
    return Indices[Index];
  }

  /**
   * @return the given vertex by index
   */
  inline const FStaticMeshVertex &GetVertex(int32 Index) const
  {
    return Vertices[Index];
  }

 protected:
  /** array of Elements for this LOD (same number as FStaticMeshLODData.NumElements) */
  ccl::vector<FStaticMeshElement> Elements;

  /** array of Indices for this LOD (same number as FStaticMeshLODData.NumIndices) */
  ccl::vector<uint32> Indices;

  /** array of vertices for this LOD (same number as FStaticMeshLODData.NumVertices) */
  ccl::vector<FStaticMeshVertex> Vertices;
};

//----------------------------------------------------------------------------
//	Static mesh element class
//----------------------------------------------------------------------------

class FStaticMeshElement : public FStaticMeshElementData {
 public:
 protected:
};

#define LANDSCAPE_ZSCALE (1.0f / 128.0f)

/** Stores information about an element of the mesh which can have its own material. */
class FMaterialElement : public FMaterialElementData {
 public:
  /** Whether Material has transmission, cached here to avoid dereferencing Material. */
  bool bTranslucent;
  /** Whether Material is Masked, cached here to avoid dereferencing Material. */
  bool bIsMasked;
  /**
   * Whether Material is TwoSided, cached here to avoid dereferencing Material.
   * This is different from FMaterialElementData::bUseTwoSidedLighting, because a two sided
   * material may still want to use one sided lighting for the most part. It just indicates whether
   * backfaces will be visible, and therefore artifacts on backfaces should be avoided.
   */
  bool bIsTwoSided;
  /** Whether Material wants to cast shadows as masked, cached here to avoid dereferencing
   * Material. */
  bool bCastShadowAsMasked;
  bool bSurfaceDomain;
  /** The material associated with this element.  After import, Material is always valid (non-null
   * and points to an FMaterial). */
  FMaterial *Material;

  FMaterialElement()
      : bTranslucent(false),
        bIsMasked(false),
        bIsTwoSided(false),
        bCastShadowAsMasked(false),
        bSurfaceDomain(true),
        Material(NULL)
  {
  }
};

class FStaticMeshStaticLightingMesh;
class FLight;
/** A mesh which is used for computing static lighting. */
class FStaticLightingMesh : public FStaticLightingMeshInstanceData {
 public:
  ccl::vector<uint16> FullName;
  /** The lights which affect the mesh's primitive. */
  ccl::vector<FLight *> RelevantLights;

  /**
   * Visibility Id's corresponding to this static lighting mesh.
   * Has to be an array because BSP exports FStaticLightingMesh's per combined group of surfaces
   * that should be lit together, Instead of per-component geometry that should be visibility
   * culled together.
   */
  ccl::vector<int32> VisibilityIds;

 protected:
  /** Whether to color texels whose lightmap UV's are invalid. */
  bool bColorInvalidTexels;

  /** Indicates whether DebugDiffuse should override the materials associated with this mesh. */
  bool bUseDebugMaterial;
  FLinearColor DebugDiffuse;

  /**
   * Materials used by the mesh, guaranteed to contain at least one.
   * These are indexed by the primitive type specific ElementIndex.
   */
  ccl::vector<FMaterialElement> MaterialElements;

  /**
   * Map from FStaticLightingMesh to the index given to uniquely identify all instances of the same
   * primitive component. This is used to give all LOD's of the same primitive component the same
   * mesh index.
   */
  static ccl::map<FStaticLightingMesh *, int32> MeshToIndexMap;

 public:
  virtual FStaticMeshStaticLightingMesh *GetInstanceableStaticMesh()
  {
    return nullptr;
  }

  virtual const FStaticMeshStaticLightingMesh *GetInstanceableStaticMesh() const
  {
    return nullptr;
  }

  /** Virtual destructor. */
  virtual ~FStaticLightingMesh() {}

  /**
   *	Returns the SourceObject type id.
   */
  virtual ESourceObjectType GetObjectType() const
  {
    return SOURCEOBJECTTYPE_Unknown;
  }

  /**
   * Accesses a triangle's vertex indices for visibility testing.
   * @param TriangleIndex - The triangle to access, valid range is [0, NumTriangles).
   * @param OutI0 - Upon return, should contain the first vertex index of the triangle.
   * @param OutI1 - Upon return, should contain the second vertex index of the triangle.
   * @param OutI2 - Upon return, should contain the third vertex index of the triangle.
   */
  virtual void GetTriangleIndices(int32 TriangleIndex,
                                  int32 &OutI0,
                                  int32 &OutI1,
                                  int32 &OutI2) const = 0;

  /**
   * Accesses a triangle's vertex indices for shading.
   * @param TriangleIndex - The triangle to access, valid range is [0, NumShadingTriangles).
   * @param OutI0 - Upon return, should contain the first vertex index of the triangle.
   * @param OutI1 - Upon return, should contain the second vertex index of the triangle.
   * @param OutI2 - Upon return, should contain the third vertex index of the triangle.
   */
  virtual void GetShadingTriangleIndices(int32 TriangleIndex,
                                         int32 &OutI0,
                                         int32 &OutI1,
                                         int32 &OutI2) const
  {
    checkSlow(NumTriangles == NumShadingTriangles);
    // By default the geometry used for shading is the same as the geometry used for visibility
    // testing.
    GetTriangleIndices(TriangleIndex, OutI0, OutI1, OutI2);
  }

  virtual bool IsElementCastingShadow(int32 ElementIndex) const
  {
    return true;
  }

  /** Returns the LOD of this instance. */
  virtual uint32 GetLODIndices() const
  {
    return 0;
  }
  
  virtual uint32 GetHLODRange() const
  {
    return 0;
  }

  /** For debugging */
  virtual void SetDebugMaterial(bool bUseDebugMaterial, FLinearColor Diffuse);

  /**
   * Whether mesh is always opaque for visibility calculations,
   * otherwise opaque property will be checked for each triangle
   */
  virtual bool IsAlwaysOpaqueForVisibility() const
  {
    return false;
  }

  virtual void Import(class FImporter &Importer);
 private:
};

/** Represents the triangles of a Landscape primitive to the static lighting system. */
class FLandscapeStaticLightingMesh : public FStaticLightingMesh,
                                     public FLandscapeStaticLightingMeshData {
 public:
  virtual void Import(class FImporter &Importer);

  // Accessors from FLandscapeDataInterface
  FORCEINLINE void VertexIndexToXY(int32 VertexIndex, int32 &OutX, int32 &OutY) const;
  FORCEINLINE void QuadIndexToXY(int32 QuadIndex, int32 &OutX, int32 &OutY) const;
  FORCEINLINE const FColor *GetHeightData(int32 LocalX, int32 LocalY) const;
  FORCEINLINE void GetTriangleIndices(
      int32 QuadIndex, int32 TriNum, int32 &OutI0, int32 &OutI1, int32 &OutI2) const
  {}
  void GetTriangleIndices(int32 TriangleIndex,
                          int32 &OutI0,
                          int32 &OutI1,
                          int32 &OutI2) const override;
  // We always want to compute visibility for landscape meshes, regardless of material blend mode
  virtual bool IsAlwaysOpaqueForVisibility() const override
  {
    return true;
  }

 private:
  ccl::vector<FColor> HeightMap;
  // Cache
  int32 NumVertices;
  int32 NumQuads;
  float UVFactor;
  bool bReverseWinding;
};

/** Represents the triangles of one LOD of a static mesh primitive to the static lighting system.
 */
class FStaticMeshStaticLightingMesh : public FStaticLightingMesh,
                                      public FStaticMeshStaticLightingMeshData {
 public:
  /** The static mesh this mesh represents. */
  FStaticMesh *StaticMesh;

  class FStaticLightingMapping *Mapping;

  /** World to local transform, for box intersection w/ instances. */
  FMatrix WorldToLocal;

  virtual FStaticMeshStaticLightingMesh *GetInstanceableStaticMesh() override
  {
    return !bIsSplineMesh ? this : nullptr;
  }

  virtual const FStaticMeshStaticLightingMesh *GetInstanceableStaticMesh() const override
  {
    return !bIsSplineMesh ? this : nullptr;
  }

  // FStaticLightingMesh interface.
  /**
   *	Returns the Guid for the object associated with this lighting mesh.
   *	Ie, for a StaticMeshStaticLightingMesh, it would return the Guid of the source static mesh.
   *	The GetObjectType function should also be used to determine the TypeId of the source object.
   */
  virtual FGuid GetObjectGuid() const
  {
    if (StaticMesh) {
      return StaticMesh->Guid;
    }
    return FGuid{0, 0, 0, 0};
  }

  /** Returns whether the given element index is translucent. */
  inline bool IsTranslucent(int32 ElementIndex) const { return MaterialElements[ElementIndex].bTranslucent; }
  /** Returns whether the given element index is masked. */
  inline bool IsMasked(int32 ElementIndex) const { return MaterialElements[ElementIndex].bIsMasked; }
  /** Whether samples using the given element accept lighting from both sides of the triangle. */
  inline bool UsesTwoSidedLighting(int32 ElementIndex) const { return MaterialElements[ElementIndex].bUseTwoSidedLighting; }
  /** Whether samples using the given element are going to have backfaces visible, and therefore artifacts on backfaces should be avoided. */
  inline bool IsTwoSided(int32 ElementIndex) const { return MaterialElements[ElementIndex].bIsTwoSided || MaterialElements[ElementIndex].bUseTwoSidedLighting; }
  inline bool IsCastingShadowsAsMasked(int32 ElementIndex) const { return MaterialElements[ElementIndex].bCastShadowAsMasked; }
  inline bool IsSurfaceDomain(int32 ElementIndex) const { return MaterialElements[ElementIndex].bSurfaceDomain; }
  inline bool IsCastingShadowAsTwoSided() const { return bCastShadowAsTwoSided; }
  inline bool IsEmissive(int32 ElementIndex) const { return MaterialElements[ElementIndex].bUseEmissiveForStaticLighting; }
  inline bool IsIndirectlyShadowedOnly(int32 ElementIndex) const { return MaterialElements[ElementIndex].bShadowIndirectOnly; }
  inline float GetFullyOccludedSamplesFraction(int32 ElementIndex) const { return MaterialElements[ElementIndex].FullyOccludedSamplesFraction; }
  inline int32 GetNumElements() const { return (int)MaterialElements.size(); }
  inline bool ShouldColorInvalidTexels() const { return bColorInvalidTexels; }
  inline bool HasImportedNormal(int32 ElementIndex) const { return MaterialElements[ElementIndex].Material->NormalSize > 0; }
  inline bool UseVertexNormalForHemisphereGather(int32 ElementIndex) const { return MaterialElements[ElementIndex].bUseVertexNormalForHemisphereGather; }
  inline const FMaterialElement &GetElement(int32 ElementIndex) const { return MaterialElements[ElementIndex]; }

  /**
   *	Returns the SourceObject type id.
   */
  virtual ESourceObjectType GetObjectType() const
  {
    return SOURCEOBJECTTYPE_StaticMesh;
  }

  virtual void GetTriangleIndices(int32 TriangleIndex,
                                  int32 &OutI0,
                                  int32 &OutI1,
                                  int32 &OutI2) const;
  virtual void GetNonTransformedTriangle(int32 TriangleIndex,
                                         FStaticLightingVertexData &OutV0,
                                         FStaticLightingVertexData &OutV1,
                                         FStaticLightingVertexData &OutV2,
                                         int32 &ElementIndex) const;
  virtual void GetNonTransformedTriangleIndices(int32 TriangleIndex,
                                                int32 &OutI0,
                                                int32 &OutI1,
                                                int32 &OutI2) const;

  virtual bool IsElementCastingShadow(int32 ElementIndex) const;

  virtual uint32 GetLODIndices() const
  {
    return EncodedLODIndices;
  }
  virtual uint32 GetHLODRange() const
  {
    return EncodedHLODRange;
  }

  /**
   * @return the portion of the LOD index variable that is actually the mesh LOD level
   * It strips off the MassiveLOD portion, which is in the high bytes. The MassiveLOD
   * portion is needed for disallowing shadow casting between parents/children
   */
  virtual uint32 GetMeshLODIndex() const
  {
    return EncodedLODIndices & 0xFFFF;
  }
  virtual uint32 GetMeshHLODIndex() const
  {
    return (EncodedLODIndices & 0xFFFF0000) >> 16;
  }

  virtual uint32 GetMeshHLODRangeStart() const
  {
    return EncodedHLODRange & 0xFFFF;
  }
  virtual uint32 GetMeshHLODRangeEnd() const
  {
    return (EncodedHLODRange & 0xFFFF0000) >> 16;
  }

  virtual void Import(class FImporter &Importer);

  /** The inverse transpose of the primitive's local to world transform. */
  FMatrix LocalToWorldInverseTranspose;
};

/** Represents the triangles of a fluid surface primitive to the static lighting system. */
class FFluidSurfaceStaticLightingMesh : public FStaticLightingMesh,
                                        public FFluidSurfaceStaticLightingMeshData {
 public:
  // FStaticLightingMesh interface.
  virtual void GetTriangleIndices(int32 TriangleIndex,
                                  int32 &OutI0,
                                  int32 &OutI1,
                                  int32 &OutI2) const;

  virtual void Import(class FImporter &Importer);
};


}
