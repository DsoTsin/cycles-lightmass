/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <cstdio>

#include <algorithm>

#include "light.h"
#include "interchange.h"

CCL_NAMESPACE_BEGIN

CCL_NAMESPACE_END

namespace Lightmass {
//----------------------------------------------------------------------------
//	Light base class
//----------------------------------------------------------------------------
void FLight::Import(FImporter &Importer)
{
  Importer.ImportData((FLightData *)this);
  Importer.ImportArray(LightTextureProfileData, FLightData::LightProfileTextureDataSize);
  // Precalculate the light's indirect color
  IndirectColor = FLinearColorUtils::AdjustSaturation(FLinearColor(Color),
                                                      IndirectLightingSaturation) *
                  IndirectLightingScale;
}

/**
 * Tests whether the light affects the given bounding volume.
 * @param Bounds - The bounding volume to test.
 * @return True if the light affects the bounding volume
 */
bool FLight::AffectsBounds(const FBoxSphereBounds &Bounds) const
{
  return true;
}

inline FVector operator-(const FVector& a, const FVector4& b)
{
    return (ccl::float3)a-ccl::make_float3(b);
}

FSphere FLight::GetBoundingSphere() const
{
  FSphere sphere;
  sphere.Center.x = 0;
  sphere.Center.y = 0;
  sphere.Center.z = 0;
  sphere.W = WORLD_MAX;
  // Directional lights will have a radius of WORLD_MAX
  return sphere;
}

FLinearColor FLight::GetDirectIntensity(const FVector4& Point, bool bCalculateForIndirectLighting) const
{
	// light profile (IES)
	float LightProfileAttenuation = 0.f;
	{
		//todo: LightProfileAttenuation = ComputeLightProfileMultiplier(LightTextureProfileData, Point, Position, Direction, GetLightTangent());
	}

	if (bCalculateForIndirectLighting)
	{
		return IndirectColor * (LightProfileAttenuation * Brightness);
	}
	else
	{
		return FLinearColor(Color) * (LightProfileAttenuation * Brightness);
	}
}

//----------------------------------------------------------------------------
//	Directional light class
//----------------------------------------------------------------------------
void FDirectionalLight::Import(FImporter &Importer)
{
  FLight::Import(Importer);

  Importer.ImportData((FDirectionalLightData *)this);
}

/** Returns the light's radiant power. */
float FDirectionalLight::Power() const
{
  const float EffectiveRadius = ImportanceBounds.SphereRadius > DELTA ?
                                    ImportanceBounds.SphereRadius :
                                    SceneBounds.SphereRadius;
  const FLinearColor LightPower = GetDirectIntensity(FVector4{0, 0, 0, 0}, false) *
                                  IndirectLightingScale * (float)PI * EffectiveRadius *
                                  EffectiveRadius;
  return FLinearColorUtils::LinearRGBToXYZ(LightPower).G;
}

//----------------------------------------------------------------------------
//	Point light class
//----------------------------------------------------------------------------
void FPointLight::Import(FImporter &Importer)
{
  FLight::Import(Importer);

  Importer.ImportData((FPointLightData *)this);
}

/**
 * Tests whether the light affects the given bounding volume.
 * @param Bounds - The bounding volume to test.
 * @return True if the light affects the bounding volume
 */
bool FPointLight::AffectsBounds(const FBoxSphereBounds &Bounds) const
{
  if (ccl::len_squared(Bounds.Origin - Position) > ccl::sqr(Radius + Bounds.SphereRadius)) {
    return false;
  }

  if (!FLight::AffectsBounds(Bounds)) {
    return false;
  }

  return true;
}

FSphere FPointLight::GetBoundingSphere() const
{
    FSphere sphere;
    sphere.Center.x=Position.x;
    sphere.Center.y=Position.y;
    sphere.Center.z=Position.z;
    sphere.W=Radius;
    return sphere;
}

// Fudge factor to get point light photon intensities to match direct lighting more closely.
static const float PointLightIntensityScale = 1.0f;

/** Returns the light's radiant power. */
float FPointLight::Power() const
{
  FLinearColor IncidentPower = FLinearColor(Color) * Brightness * IndirectLightingScale;
  // Approximate power of the light by the total amount of light passing through a sphere at half
  // the light's radius
  const float RadiusFraction = .5f;
  const float DistanceToEvaluate = RadiusFraction * Radius;

  if (LightFlags & GI_LIGHT_INVERSE_SQUARED) {
    IncidentPower = IncidentPower / (DistanceToEvaluate * DistanceToEvaluate);
  }
  else {
    float UnrealAttenuation = std::pow(std::max(1.0f - RadiusFraction * RadiusFraction, 0.0f),
                                         FalloffExponent);
    // Point light power is proportional to its radius squared
    IncidentPower = IncidentPower * UnrealAttenuation;
  }

  const FLinearColor LightPower = IncidentPower * 4.f * PI * DistanceToEvaluate *
                                  DistanceToEvaluate;
  return FLinearColorUtils::LinearRGBToXYZ(LightPower).G;
}

//----------------------------------------------------------------------------
//	Spot light class
//----------------------------------------------------------------------------
void FSpotLight::Import(FImporter &Importer)
{
  FPointLight::Import(Importer);

  Importer.ImportData((FSpotLightData *)this);
}

/**
 * Tests whether the light affects the given bounding volume.
 * @param Bounds - The bounding volume to test.
 * @return True if the light affects the bounding volume
 */
bool FSpotLight::AffectsBounds(const FBoxSphereBounds &Bounds) const
{
  if (!FLight::AffectsBounds(Bounds)) {
    return false;
  }

  // Radial check
  if (ccl::len_squared(Bounds.Origin - Position) > ccl::sqr(Radius + Bounds.SphereRadius)) {
    return false;
  }

  // Cone check

  //FVector4 U = Position - (Bounds.SphereRadius / SinOuterConeAngle) * Direction,
  //         D = Bounds.Origin - U;
  //float dsqr = ccl::dot(ccl::make_float3(D), ccl::make_float3(D)), E = ccl::dot(ccl::make_float3(Direction), ccl::make_float3(D));
  //if (E > 0.0f && E * E >= dsqr * ccl::sqr(CosOuterConeAngle)) {
  //  D = Bounds.Origin - Position;
  //  dsqr = ccl::dot(ccl::make_float3(D), ccl::make_float3(D));
  //  E = -ccl::dot(ccl::make_float3(Direction), ccl::make_float3(D));
  //  if (E > 0.0f && E * E >= dsqr * ccl::sqr(SinOuterConeAngle))
  //    return dsqr <= ccl::sqr(Bounds.SphereRadius);
  //  else
  //    return true;
  //}

  return false;
}

FSphere FSpotLight::GetBoundingSphere() const
{
  return FSphere();
}

//FSphere FSpotLight::GetBoundingSphere() const
//{
//  return FMath::ComputeBoundingSphereForCone(
//      Position, Direction, Radius, CosOuterConeAngle, SinOuterConeAngle);
//}

//----------------------------------------------------------------------------
//	Rect light class
//----------------------------------------------------------------------------
void FRectLight::Import(FImporter &Importer)
{
  FPointLight::Import(Importer);

  Importer.ImportData((FRectLightData *)this);
}

//bool FRectLight::AffectsBounds(const FBoxSphereBounds &Bounds) const
//{
  //todo: FPlane Plane(Position, Direction);
  //float DistanceToPlane = (Bounds.Origin - Position) | Direction;
  //if (DistanceToPlane < -Bounds.SphereRadius) {
  //  return false;
  //}

//  if (!FPointLight::AffectsBounds(Bounds)) {
//    return false;
//  }

//  return true;
//}
//}

//----------------------------------------------------------------------------
//	Sky light class
//----------------------------------------------------------------------------
void FSkyLight::Import(FImporter &Importer)
{
  FLight::Import(Importer);

  Importer.ImportData((FSkyLightData *)this);

  ccl::vector<FFloat16Color> RadianceEnvironmentMap;
  Importer.ImportArray(RadianceEnvironmentMap, RadianceEnvironmentMapDataSize);
#if 0
  CubemapSize = FMath::Sqrt(RadianceEnvironmentMapDataSize / 6);
  NumMips = FMath::CeilLogTwo(CubemapSize) + 1;

  check(FMath::IsPowerOfTwo(CubemapSize));
  check(NumMips > 0);
  check(RadianceEnvironmentMapDataSize == CubemapSize * CubemapSize * 6);

  if (bUseFilteredCubemap && CubemapSize > 0) {

    PrefilteredRadiance.resize(NumMips);
    //PrefilteredRadiance.AddZeroed(NumMips);

    PrefilteredRadiance[0].resize(CubemapSize * CubemapSize * 6);
    //PrefilteredRadiance[0].AddZeroed(CubemapSize * CubemapSize * 6);

    for (int32 TexelIndex = 0; TexelIndex < CubemapSize * CubemapSize * 6; TexelIndex++) {
      FLinearColor Lighting = FLinearColor(RadianceEnvironmentMap[TexelIndex]);
      PrefilteredRadiance[0][TexelIndex] = Lighting;
    }

    FIntPoint SubCellOffsets[4] = {
        FIntPoint(0, 0), FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(1, 1)};

    const float SubCellWeight = 1.0f / (float)UE_ARRAY_COUNT(SubCellOffsets);

    for (int32 MipIndex = 1; MipIndex < NumMips; MipIndex++) {
      const int32 MipSize = 1 << (NumMips - MipIndex - 1);
      const int32 ParentMipSize = MipSize * 2;
      const int32 CubeFaceSize = MipSize * MipSize;

      PrefilteredRadiance[MipIndex].resize(CubeFaceSize * 6);
      PrefilteredRadiance[MipIndex].AddZeroed(CubeFaceSize * 6);

      for (int32 FaceIndex = 0; FaceIndex < 6; FaceIndex++) {
        for (int32 Y = 0; Y < MipSize; Y++) {
          for (int32 X = 0; X < MipSize; X++) {
            FLinearColor FilteredValue(0, 0, 0, 0);

            for (int32 OffsetIndex = 0; OffsetIndex < UE_ARRAY_COUNT(SubCellOffsets);
                 OffsetIndex++)
            {
              FIntPoint ParentOffset = FIntPoint(X, Y) * 2 + SubCellOffsets[OffsetIndex];
              int32 ParentTexelIndex = FaceIndex * ParentMipSize * ParentMipSize +
                                       ParentOffset.Y * ParentMipSize + ParentOffset.X;
              FLinearColor ParentLighting = PrefilteredRadiance[MipIndex - 1][ParentTexelIndex];
              FilteredValue += ParentLighting;
            }

            FilteredValue *= SubCellWeight;

            PrefilteredRadiance[MipIndex][FaceIndex * CubeFaceSize + Y * MipSize + X] =
                FilteredValue;
          }
        }
      }
    }

    ComputePrefilteredVariance();
  }
#endif
}

void FSkyLight::ComputePrefilteredVariance()
{
#if 0
  PrefilteredVariance.resize(NumMips);
  //PrefilteredVariance.AddZeroed(NumMips);

  ccl::vector<float> TempMaxVariance;
  TempMaxVariance.resize(NumMips);
  //TempMaxVariance.AddZeroed(NumMips);

  for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++) {
    const int32 MipSize = 1 << (NumMips - MipIndex - 1);
    const int32 CubeFaceSize = MipSize * MipSize;
    const int32 BaseMipTexelSize = CubemapSize / MipSize;
    const float NormalizeFactor = 1.0f / std::max(BaseMipTexelSize * BaseMipTexelSize - 1, 1);

    PrefilteredVariance[MipIndex].resize(CubeFaceSize * 6);
    //PrefilteredVariance[MipIndex].AddZeroed(CubeFaceSize * 6);

    for (int32 FaceIndex = 0; FaceIndex < 6; FaceIndex++) {
      for (int32 Y = 0; Y < MipSize; Y++) {
        for (int32 X = 0; X < MipSize; X++) {
          int32 TexelIndex = FaceIndex * CubeFaceSize + Y * MipSize + X;

          float Mean = PrefilteredRadiance[MipIndex][TexelIndex].GetLuminance();

          int32 BaseTexelOffset = FaceIndex * CubemapSize * CubemapSize + X * BaseMipTexelSize +
                                  Y * BaseMipTexelSize * CubemapSize;
          float SumOfSquares = 0;

          //@todo - implement in terms of the previous mip level, not the bottom mip level
          for (int32 BaseY = 0; BaseY < BaseMipTexelSize; BaseY++) {
            for (int32 BaseX = 0; BaseX < BaseMipTexelSize; BaseX++) {
              int32 BaseTexelIndex = BaseTexelOffset + BaseY * CubemapSize + BaseX;
              float BaseValue = PrefilteredRadiance[0][BaseTexelIndex].GetLuminance();

              SumOfSquares += (BaseValue - Mean) * (BaseValue - Mean);
            }
          }

          PrefilteredVariance[MipIndex][TexelIndex] = SumOfSquares * NormalizeFactor;
          TempMaxVariance[MipIndex] = std::max(TempMaxVariance[MipIndex],
                                                 SumOfSquares * NormalizeFactor);
        }
      }
    }
  }
#endif
}

}
