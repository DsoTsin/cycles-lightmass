/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <cstdio>
#include <algorithm>
#include "util.h"
#include "light.h"
#include "interchange.h"
#include "mesh.h"
#include "scene.h"

CCL_NAMESPACE_BEGIN

CCL_NAMESPACE_END

namespace Lightmass {
FScene::FScene() {}
FScene::~FScene() {}

void FScene::Import(FImporter& Importer) {
  FSceneFileHeader TempHeader;
  // Import into a temp header, since padding in the header overlaps with data members in FScene
  // and ImportData stomps on that padding
  Importer.ImportData(&TempHeader);
  // Copy header members without modifying padding in FSceneFileHeader
  (FSceneFileHeader &)(*this) = TempHeader;

  // The assignment above overwrites ImportanceVolumes since FSceneFileHeader has some padding
  // which coincides with ImportanceVolumes
  memset(&ImportanceVolumes, sizeof(ImportanceVolumes), 0);
#if __UNREAL_NON_OFFICIAL__
  memset(&DummyVolumes, sizeof(DummyVolumes), 0);
#endif

  Importer.SetLevelScale(SceneConstants.StaticLightingLevelScale);
  ApplyStaticLightingScale();
  
  ccl::vector<TCHAR> UserName;
  int32 UserNameLen;
  Importer.ImportData(&UserNameLen);
  Importer.ImportArray(UserName, UserNameLen);
  UserName.push_back('\0');

  ccl::vector<TCHAR> PersistentLevelNameArray;
  int32 PersistentLevelNameLen;
  Importer.ImportData(&PersistentLevelNameLen);
  Importer.ImportArray(PersistentLevelNameArray, PersistentLevelNameLen);
  PersistentLevelNameArray.push_back('\0');

  //todo:ImportanceBoundingBox.Init();
  for (int32 VolumeIndex = 0; VolumeIndex < NumImportanceVolumes; VolumeIndex++) {
    FBox LMBox;
    Importer.ImportData(&LMBox);
    // todo:ImportanceBoundingBox += LMBox;
    ImportanceVolumes.push_back(LMBox);
  }

#if __UNREAL_NON_OFFICIAL__
  for (int32 VolumeIndex = 0; VolumeIndex < iDummy; VolumeIndex++) {
    FBox LMBox;
    Importer.ImportData(&LMBox);
    DummyVolumes.push_back(LMBox);
  }
#endif

  if (NumImportanceVolumes == 0) {
    // todo:ImportanceBoundingBox = FBox(FVector4(0, 0, 0), FVector4(0, 0, 0));
  }

  for (int32 VolumeIndex = 0; VolumeIndex < NumCharacterIndirectDetailVolumes; VolumeIndex++) {
    FBox LMBox;
    Importer.ImportData(&LMBox);
    CharacterIndirectDetailVolumes.push_back(LMBox);
  }

  for (int32 PortalIndex = 0; PortalIndex < NumPortals; PortalIndex++) {
    FMatrix LMPortal;
    Importer.ImportData(&LMPortal);
    // todo:Portals.push_back(FSphere(LMPortal.GetOrigin(), FVector2D(LMPortal.GetScaleVector().Y,
    // LMPortal.GetScaleVector().Z).Size()));
  }

  Importer.ImportArray(VisibilityBucketGuids, NumPrecomputedVisibilityBuckets);

  int32 NumVisVolumes;
  Importer.ImportData(&NumVisVolumes);
  PrecomputedVisibilityVolumes.resize(NumVisVolumes);
  for (int32 VolumeIndex = 0; VolumeIndex < NumVisVolumes; VolumeIndex++) {
    FPrecomputedVisibilityVolume &CurrentVolume = PrecomputedVisibilityVolumes[VolumeIndex];
    Importer.ImportData(&CurrentVolume.Bounds);
    int32 NumPlanes;
    Importer.ImportData(&NumPlanes);
    Importer.ImportArray(CurrentVolume.Planes, NumPlanes);
  }

  int32 NumVisOverrideVolumes;
  Importer.ImportData(&NumVisOverrideVolumes);
  PrecomputedVisibilityOverrideVolumes.resize(NumVisOverrideVolumes);
  for (int32 VolumeIndex = 0; VolumeIndex < NumVisOverrideVolumes; VolumeIndex++) {
    FPrecomputedVisibilityOverrideVolume &CurrentVolume =
        PrecomputedVisibilityOverrideVolumes[VolumeIndex];
    Importer.ImportData(&CurrentVolume.Bounds);
    int32 NumVisibilityIds;
    Importer.ImportData(&NumVisibilityIds);
    Importer.ImportArray(CurrentVolume.OverrideVisibilityIds, NumVisibilityIds);
    int32 NumInvisibilityIds;
    Importer.ImportData(&NumInvisibilityIds);
    Importer.ImportArray(CurrentVolume.OverrideInvisibilityIds, NumInvisibilityIds);
  }

  int32 NumCameraTrackPositions;
  Importer.ImportData(&NumCameraTrackPositions);
  Importer.ImportArray(CameraTrackPositions, NumCameraTrackPositions);

  for (int32 VolumeIndex = 0; VolumeIndex < NumVolumetricLightmapDensityVolumes; VolumeIndex++) {
    FVolumetricLightmapDensityVolumeData LMVolumeData;
    Importer.ImportData(&LMVolumeData);
    static_assert(sizeof(FVolumetricLightmapDensityVolumeData) == 40, "Update member copy");

    FVolumetricLightmapDensityVolume LMVolume;
    LMVolume.Bounds = LMVolumeData.Bounds;
    LMVolume.AllowedMipLevelRange = LMVolumeData.AllowedMipLevelRange;
    LMVolume.NumPlanes = LMVolumeData.NumPlanes;
    Importer.ImportArray(LMVolume.Planes, LMVolume.NumPlanes);
    VolumetricLightmapDensityVolumes.push_back(LMVolume);
  }

  Importer.ImportArray(VolumetricLightmapTaskGuids, NumVolumetricLightmapTasks);

  Importer.ImportObjectArray(DirectionalLights, NumDirectionalLights, Importer.GetLights());
  Importer.ImportObjectArray(PointLights, NumPointLights, Importer.GetLights());
  Importer.ImportObjectArray(SpotLights, NumSpotLights, Importer.GetLights());
  Importer.ImportObjectArray(RectLights, NumRectLights, Importer.GetLights());
  Importer.ImportObjectArray(SkyLights, NumSkyLights, Importer.GetLights());

  Importer.ImportObjectArray(
      StaticMeshInstances, NumStaticMeshInstances, Importer.GetStaticMeshInstances());
  Importer.ImportObjectArray(
      FluidMeshInstances, NumFluidSurfaceInstances, Importer.GetFluidMeshInstances());
  Importer.ImportObjectArray(
      LandscapeMeshInstances, NumLandscapeInstances, Importer.GetLandscapeMeshInstances());
  Importer.ImportObjectArray(BspMappings, NumBSPMappings, Importer.GetBSPMappings());
  Importer.ImportObjectArray(
      TextureLightingMappings, NumStaticMeshTextureMappings, Importer.GetTextureMappings());
  Importer.ImportObjectArray(
      FluidMappings, NumFluidSurfaceTextureMappings, Importer.GetFluidMappings());
  Importer.ImportObjectArray(
      LandscapeMappings, NumLandscapeTextureMappings, Importer.GetLandscapeMappings());
  Importer.ImportObjectArray(VolumeMappings, NumVolumeMappings, Importer.GetVolumeMappings());
}

FBoxSphereBounds FScene::GetImportanceBounds() const
{
  return FBoxSphereBounds();
}

}
