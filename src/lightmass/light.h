/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util.h"
#include "scene.h"

CCL_NAMESPACE_BEGIN

CCL_NAMESPACE_END

namespace Lightmass {
//----------------------------------------------------------------------------
//	Light base class
//----------------------------------------------------------------------------
class FLight : public FLightData {
 public:
  virtual ~FLight() {}

  virtual void Import(class FImporter &Importer);

  /**
   * @return 'this' if the light is a skylight, NULL otherwise
   */
  virtual const class FSkyLight *GetSkyLight() const
  {
    return NULL;
  }
  virtual class FSkyLight *GetSkyLight()
  {
    return NULL;
  }

  virtual class FDirectionalLight *GetDirectionalLight()
  {
    return NULL;
  }

  virtual const class FDirectionalLight *GetDirectionalLight() const
  {
    return NULL;
  }

  virtual class FPointLight *GetPointLight()
  {
    return NULL;
  }

  virtual const class FPointLight *GetPointLight() const
  {
    return NULL;
  }

  virtual class FSpotLight *GetSpotLight()
  {
    return NULL;
  }

  virtual const class FSpotLight *GetSpotLight() const
  {
    return NULL;
  }

  virtual const class FMeshAreaLight *GetMeshAreaLight() const
  {
    return NULL;
  }

  /**
   * Tests whether the light affects the given bounding volume.
   * @param Bounds - The bounding volume to test.
   * @return True if the light affects the bounding volume
   */
  virtual bool AffectsBounds(const FBoxSphereBounds &Bounds) const;

  virtual FSphere GetBoundingSphere() const;

  /**
   * Computes the intensity of the direct lighting from this light on a specific point.
   */
  virtual FLinearColor GetDirectIntensity(const FVector4 &Point,
                                          bool bCalculateForIndirectLighting) const;

  /** Returns the light's radiant power. */
  virtual float Power() const = 0;

  /**
   * Returns whether static lighting, aka lightmaps, is being used for primitive/ light
   * interaction.
   *
   * @return true if lightmaps/ static lighting is being used, false otherwise
   */
  bool UseStaticLighting() const
  {
    return (LightFlags & GI_LIGHT_HASSTATICLIGHTING) != 0;
  }

 protected:
  /** Cached calculation of the light's indirect color, which is the base Color adjusted by the
   * IndirectLightingScale and IndirectLightingSaturation */
  FLinearColor IndirectColor;
  ccl::vector<uint8> LightTextureProfileData;
};

//----------------------------------------------------------------------------
//	Directional light class
//----------------------------------------------------------------------------
class FDirectionalLight : public FLight, public FDirectionalLightData {
 public:
  float IndirectDiskRadius;

  virtual void Import(class FImporter &Importer) override;

  virtual class FDirectionalLight *GetDirectionalLight() override
  {
    return this;
  }

  virtual const class FDirectionalLight *GetDirectionalLight() const override
  {
    return this;
  }

  /** Returns the light's radiant power. */
  virtual float Power() const override;

 protected:
  /** Extent of PathRayGrid in the [-1, 1] space of the directional light's disk. */
  float GridExtent;

  /** Center of PathRayGrid in the [-1, 1] space of the directional light's disk. */
  FVector2D GridCenter;

  /** Size of PathRayGrid in each dimension. */
  int32 GridSize;

  /** Grid of indices into IndirectPathRays that affect each cell. */
//todo:  TArray<TArray<int32>> PathRayGrid;

  /** Bounds of the scene that the directional light is affecting. */
  FBoxSphereBounds SceneBounds;

  bool bEmitPhotonsOutsideImportanceVolume;

  /** Bounds of the importance volume in the scene.  If the radius is 0, there was no importance
   * volume. */
  FBoxSphereBounds ImportanceBounds;

  /** Center of the importance volume in the [-1,1] space of the directional light's disk */
  FVector2D ImportanceDiskOrigin;

  /** Radius of the importance volume in the [-1,1] space of the directional light's disk */
  float LightSpaceImportanceDiskRadius;

  /** Density of photons to gather outside of the importance volume. */
  float OutsideImportanceVolumeDensity;

  /** Probability of generating a sample inside the importance volume. */
  float ImportanceBoundsSampleProbability;

  /** X axis of the directional light, which is unit length and orthogonal to the direction and
   * YAxis. */
  FVector4 XAxis;

  /** Y axis of the directional light, which is unit length and orthogonal to the direction and
   * XAxis. */
  FVector4 YAxis;
};

//----------------------------------------------------------------------------
//	Point light class
//----------------------------------------------------------------------------
class FPointLight : public FLight, public FPointLightData {
 public:
  virtual void Import(class FImporter &Importer) override;

  virtual class FPointLight *GetPointLight() override
  {
    return this;
  }

  virtual const class FPointLight *GetPointLight() const override
  {
    return this;
  }

  /**
   * Tests whether the light affects the given bounding volume.
   * @param Bounds - The bounding volume to test.
   * @return True if the light affects the bounding volume
   */
  virtual bool AffectsBounds(const FBoxSphereBounds &Bounds) const override;

  virtual FSphere GetBoundingSphere() const override;
  
  /** Returns the light's radiant power. */
  virtual float Power() const override;

 protected:
  float CosIndirectPhotonEmitConeAngle;
};

//----------------------------------------------------------------------------
//	Spot light class
//----------------------------------------------------------------------------
class FSpotLight : public FPointLight, public FSpotLightData {
 public:
  virtual class FSpotLight *GetSpotLight() override
  {
    return this;
  }

  virtual const class FSpotLight *GetSpotLight() const override
  {
    return this;
  }

  virtual void Import(class FImporter &Importer) override;

  /**
   * Tests whether the light affects the given bounding volume.
   * @param Bounds - The bounding volume to test.
   * @return True if the light affects the bounding volume
   */
  virtual bool AffectsBounds(const FBoxSphereBounds &Bounds) const override;

  virtual FSphere GetBoundingSphere() const override;

 protected:
  float SinOuterConeAngle;
  float CosOuterConeAngle;
  float CosInnerConeAngle;
};

//----------------------------------------------------------------------------
//	Rect light class
//----------------------------------------------------------------------------
class FRectLight : public FPointLight, public FRectLightData {
 public:
  virtual void Import(class FImporter &Importer) override;

 private:
   ccl::vector<FFloat16Color> SourceTexture;
};

//----------------------------------------------------------------------------
//	Sky light class
//----------------------------------------------------------------------------
class FSkyLight : public FLight, public FSkyLightData {
 public:
  virtual void Import(class FImporter &Importer) override;

  /**
   * @return 'this' if the light is a skylight, NULL otherwise
   */
  virtual const class FSkyLight *GetSkyLight() const override
  {
    return this;
  }
  virtual class FSkyLight *GetSkyLight() override
  {
    return this;
  }

  /** Returns the light's radiant power. */
  virtual float Power() const override
  {
    //checkf(0, TEXT("Power is not supported for skylights"));
    return 0;
  }

 protected:
  void ComputePrefilteredVariance();

  int32 CubemapSize;
  int32 NumMips;
  ccl::vector<ccl::vector<FLinearColor>> PrefilteredRadiance;
  ccl::vector<ccl::vector<float>> PrefilteredVariance;
};

}