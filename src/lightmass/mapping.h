/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util.h"
#include "scene.h"
#include "mesh.h"

namespace Lightmass {

/** A mapping between world-space surfaces and a static lighting cache. */
class FStaticLightingMapping : public FStaticLightingMappingData {
 public:
  /** The mesh associated with the mapping, guaranteed to be valid (non-NULL) after import. */
  class FStaticLightingMesh *Mesh;

  /** Whether the mapping has been processed. */
  volatile int32 bProcessed;

  /** If true, the mapping is being padded */
  bool bPadded;

  /** A static indicating that debug borders should be used around padded mappings. */
  static bool s_bShowLightmapBorders;

  /** Index of this mapping in FStaticLightingSystem::AllMappings */
  int32 SceneMappingIndex;


 public:
  /** Initialization constructor. */
  FStaticLightingMapping()
      : Mesh(nullptr), bProcessed(false), bPadded(false), SceneMappingIndex(-1)
  {
  }

  /** Virtual destructor. */
  virtual ~FStaticLightingMapping() {}

  /** @return If the mapping is a texture mapping, returns a pointer to this mapping as a texture
   * mapping.  Otherwise, returns NULL. */
  virtual class FStaticLightingTextureMapping *GetTextureMapping()
  {
    return NULL;
  }

  virtual const FStaticLightingTextureMapping *GetTextureMapping() const
  {
    return NULL;
  }

  virtual class FStaticLightingGlobalVolumeMapping *GetVolumeMapping()
  {
    return NULL;
  }

  virtual const FStaticLightingGlobalVolumeMapping *GetVolumeMapping() const
  {
    return NULL;
  }

  /**
   * Returns the relative processing cost used to sort tasks from slowest to fastest.
   *
   * @return	relative processing cost or 0 if unknown
   */
  virtual float GetProcessingCost() const
  {
    return 0;
  }

  virtual void Import(class FImporter &Importer);
};

/** A mapping between world-space surfaces and static lighting cache textures. */
class FStaticLightingTextureMapping : public FStaticLightingMapping,
                                      public FStaticLightingTextureMappingData {
 public:
  FStaticLightingTextureMapping()
      : NumOutstandingCacheTasks(0)
  {
  }

  // FStaticLightingMapping interface.
  virtual FStaticLightingTextureMapping *GetTextureMapping() override
  {
    return this;
  }

  virtual const FStaticLightingTextureMapping *GetTextureMapping() const override
  {
    return this;
  }

  /**
   * Returns the relative processing cost used to sort tasks from slowest to fastest.
   *
   * @return	relative processing cost or 0 if unknown
   */
  virtual float GetProcessingCost() const
  {
    return SizeX * SizeY;
  }

  virtual void Import(class FImporter &Importer);

  /** The padded size of the mapping */
  int32 CachedSizeX;
  int32 CachedSizeY;

#if __UNREAL_NON_OFFICIAL__
  int32 iDummy;
  int32 iDummy2;
#endif

  /** The sizes that CachedIrradiancePhotons were stored with */
  int32 SurfaceCacheSizeX;
  int32 SurfaceCacheSizeY;

  /** Counts how many cache tasks this mapping needs completed. */
  volatile int32 NumOutstandingCacheTasks;
};

/**
 * A mapping that represents an object which is going to use the global volumetric lightmap.
 * Hack: currently represented as a texture lightmap to Lightmass for the purposes of surface
 * caching of light (radiosity + direct lighting).
 */
class FStaticLightingGlobalVolumeMapping : public FStaticLightingTextureMapping {
 public:
  FStaticLightingGlobalVolumeMapping() {}

  virtual class FStaticLightingGlobalVolumeMapping *GetVolumeMapping() override
  {
    return this;
  }

  virtual const FStaticLightingGlobalVolumeMapping *GetVolumeMapping() const override
  {
    return this;
  }
};
/** Represents a static mesh primitive with texture mapped static lighting. */
class FStaticMeshStaticLightingTextureMapping : public FStaticLightingTextureMapping {
 public:
  virtual void Import(class FImporter &Importer);

 private:
  /** The LOD this mapping represents. */
  int32 LODIndex;
};

/** Represents a landscape primitive with texture mapped static lighting. */
class FLandscapeStaticLightingTextureMapping : public FStaticLightingTextureMapping {
 public:
  virtual void Import(class FImporter &Importer);
};

/** Represents a fluid surface primitive with texture mapped static lighting. */
class FFluidSurfaceStaticLightingTextureMapping : public FStaticLightingTextureMapping {
 public:
  virtual void Import(class FImporter &Importer);
};

class FBSPSurfaceStaticLighting : public FStaticLightingMesh,
                                  public FBSPSurfaceStaticLightingData {
 public:
  FBSPSurfaceStaticLighting() : bComplete(false) {}

  virtual void GetTriangleIndices(int32 TriangleIndex,
                                  int32 &OutI0,
                                  int32 &OutI1,
                                  int32 &OutI2) const;

  virtual void Import(class FImporter &Importer);

  /** true if the surface has complete static lighting. */
  bool bComplete;

  /** Texture mapping for the BSP */
  FStaticLightingTextureMapping Mapping;

  /** The surface's vertices. */
  ccl::vector<FStaticLightingVertexData> Vertices;

  /** The vertex indices of the surface's triangles. */
  ccl::vector<int32> TriangleVertexIndices;

  /** Array for each triangle to the lightmass settings (boost, etc) */
  ccl::vector<int32> TriangleLightmassSettings;
};
}