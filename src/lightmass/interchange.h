/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "scene.h"
#include "mesh.h"
#include "material.h"
#include "texture.h"
#include "light.h"
#include "mapping.h"

#include "SwarmInterface.h"

// ccl begin
#include "scene/image.h"
#include "scene/shader_nodes.h"

namespace Lightmass {
#define LM_COMPRESS_INPUT_DATA 1

/** Whether to enable channel I/O via Swarm - disable for performance debugging */
#define SWARM_ENABLE_CHANNEL_READS 1
#define SWARM_ENABLE_CHANNEL_WRITES 1
/** Whether or not to request compression on heavyweight input file types */
#define LM_COMPRESS_INPUT_DATA 1
#define LM_NUM_SH_COEFFICIENTS 9

/** The number of coefficients that are stored for each light sample. */
static const int32 LM_NUM_STORED_LIGHTMAP_COEF = 4;
/** The number of high quality coefficients which the lightmap stores for each light sample. */
static const int32 LM_NUM_HQ_LIGHTMAP_COEF = 2;
/** The index at which low quality coefficients are stored in any array containing all
 * LM_NUM_STORED_LIGHTMAP_COEF coefficients. */
static const int32 LM_LQ_LIGHTMAP_COEF_INDEX = 2;

/** Output channel types (extensions) */
static const TCHAR LM_TEXTUREMAPPING_EXTENSION[] = L"tmap";
static const TCHAR LM_VOLUMESAMPLES_EXTENSION[] = L"vols";
static const TCHAR LM_VOLUMEDEBUGOUTPUT_EXTENSION[] = L"vold";
static const TCHAR LM_VOLUMETRICLIGHTMAP_EXTENSION[] = L"irvol";
static const TCHAR LM_PRECOMPUTEDVISIBILITY_EXTENSION[] = L"vis";
static const TCHAR LM_DOMINANTSHADOW_EXTENSION[] = L"doms";
static const TCHAR LM_MESHAREALIGHTDATA_EXTENSION[] = L"arealights";
static const TCHAR LM_DEBUGOUTPUT_EXTENSION[] = L"dbgo";

/** Input channel types (extensions) */
#if LM_COMPRESS_INPUT_DATA
static const TCHAR LM_SCENE_EXTENSION[] = L"scenegz";
static const TCHAR LM_STATICMESH_EXTENSION[] = L"meshgz";
static const TCHAR LM_MATERIAL_EXTENSION[] = L"mtrlgz";
#else
static const TCHAR LM_SCENE_EXTENSION[] = L"scene";
static const TCHAR LM_STATICMESH_EXTENSION[] = L"mesh";
static const TCHAR LM_MATERIAL_EXTENSION[] = L"mtrl";
#endif

/**
 * Channel versions
 * Adding a new guid for any of these forces re-exporting of that data type.
 * This must be done whenever modifying code that changes how each data type is exported!
 */

static const int32 LM_TEXTUREMAPPING_VERSION = 1;
static const int32 LM_VOLUMESAMPLES_VERSION = 1;
static const int32 LM_VOLUMETRICLIGHTMAP_VERSION = 3;
static const int32 LM_PRECOMPUTEDVISIBILITY_VERSION = 1;
static const int32 LM_VOLUMEDEBUGOUTPUT_VERSION = 1;
static const int32 LM_DOMINANTSHADOW_VERSION = 1;
static const int32 LM_MESHAREALIGHTDATA_VERSION = 1;
static const int32 LM_DEBUGOUTPUT_VERSION = 1;
static const int32 LM_SCENE_VERSION = 1;

static const int32 LM_STATICMESH_VERSION = 2; // todo:

static const int32 LM_MATERIAL_VERSION = 1;

/** Flags to use when opening the different kinds of output channels */
/** MUST PAIR APPROPRIATELY WITH THE SAME FLAGS IN UE4 */
static const int32 LM_TEXTUREMAPPING_CHANNEL_FLAGS = NSwarm::SWARM_JOB_CHANNEL_WRITE;
static const int32 LM_VOLUMESAMPLES_CHANNEL_FLAGS = NSwarm::SWARM_JOB_CHANNEL_WRITE;
static const int32 LM_PRECOMPUTEDVISIBILITY_CHANNEL_FLAGS = NSwarm::SWARM_JOB_CHANNEL_WRITE;
static const int32 LM_VOLUMEDEBUGOUTPUT_CHANNEL_FLAGS = NSwarm::SWARM_JOB_CHANNEL_WRITE;
static const int32 LM_DOMINANTSHADOW_CHANNEL_FLAGS = NSwarm::SWARM_JOB_CHANNEL_WRITE;
static const int32 LM_MESHAREALIGHT_CHANNEL_FLAGS = NSwarm::SWARM_JOB_CHANNEL_WRITE;
static const int32 LM_DEBUGOUTPUT_CHANNEL_FLAGS = NSwarm::SWARM_JOB_CHANNEL_WRITE;

/** Flags to use when opening the different kinds of input channels */
/** MUST PAIR APPROPRIATELY WITH THE SAME FLAGS IN UE4 */
#if LM_COMPRESS_INPUT_DATA
static const int32 LM_SCENE_CHANNEL_FLAGS = NSwarm::SWARM_JOB_CHANNEL_READ |
                                            NSwarm::SWARM_CHANNEL_MISC_ENABLE_COMPRESSION;
static const int32 LM_STATICMESH_CHANNEL_FLAGS = NSwarm::SWARM_CHANNEL_READ |
                                                 NSwarm::SWARM_CHANNEL_MISC_ENABLE_COMPRESSION;
static const int32 LM_MATERIAL_CHANNEL_FLAGS = NSwarm::SWARM_CHANNEL_READ |
                                               NSwarm::SWARM_CHANNEL_MISC_ENABLE_COMPRESSION;
#else
static const int32 LM_SCENE_CHANNEL_FLAGS = NSwarm::SWARM_JOB_CHANNEL_READ;
static const int32 LM_STATICMESH_CHANNEL_FLAGS = NSwarm::SWARM_CHANNEL_READ;
static const int32 LM_MATERIAL_CHANNEL_FLAGS = NSwarm::SWARM_CHANNEL_READ;
#endif

class FSwarm {
 public:
  /**
   * Constructs the Swarm wrapper used by Lightmass.
   * @param SwarmInterface	The global SwarmInterface to use
   * @param JobGuid			Guid that identifies the job we're working on
   * @param TaskQueueSize		Number of tasks we should try to keep in the queue
   */
  FSwarm(NSwarm::FSwarmInterface &SwarmInterface);

  /** Destructor */
  ~FSwarm();

  /**
   * @retrurn the currently active channel for reading
   */
  int32 GetChannel()
  {
//todo:    checkf(ChannelStack.Num() > 0, L"Tried to get a channel, but none exists");
    return ChannelStack[ChannelStack.size() - 1];
  }

  /**
   * Returns the current job guid.
   * @return	Current Swarm job guid
   */
  const FGuid &GetJobGuid()
  {
    return JobGuid;
  }

  /**
   * Opens a new channel and optionally pushes it on to the channel stack
   *
   * @param ChannelName Name of the channel
   * @param ChannelFlags Flags (read, write, etc) for the channel
   * @param bPushChannel If true, this new channel will be auto-pushed onto the stack
   *
   * @return The channel that was opened
   */
  int32 OpenChannel(const TCHAR *ChannelName, int32 ChannelFlags, bool bPushChannel);

  /**
   * Closes a channel previously opened with OpenSideChannel
   *
   * @param Channel The channel to close
   */
  void CloseChannel(int32 Channel);

  /**
   * Pushes a new channel on the stack as the current channel to read from
   *
   * @param Channel New channel to read from
   */
  void PushChannel(int32 Channel);

  /**
   * Pops the top channel
   *
   * @param bCloseChannel If true, the channel will be closed when it is popped off
   */
  void PopChannel(bool bCloseChannel);

  /**
   * Function that closes and pops current channel
   */
  void CloseCurrentChannel()
  {
    PopChannel(true);
  }

  /**
   * Reads data from the current channel
   *
   * @param Data Data to read from the channel
   * @param Size Size of Data
   *
   * @return Amount read
   */
  int32 Read(void *Data, int32 Size)
  {
    TotalNumReads++;
// todo:    TotalSecondsRead -= FPlatformTime::Seconds();
#if SWARM_ENABLE_CHANNEL_READS
    int32 NumRead = API.ReadChannel(GetChannel(), Data, Size);
#else
    int32 NumRead = 0;
#endif
    TotalBytesRead += NumRead;
// todo:    TotalSecondsRead += FPlatformTime::Seconds();
    return NumRead;
  }

  /**
   * Writes data to the current channel
   *
   * @param Data Data to write over the channel
   * @param Size Size of Data
   *
   * @return Amount written
   */
  int32 Write(const void *Data, int32 Size)
  {
    TotalNumWrites++;
// todo:    TotalSecondsWritten -= FPlatformTime::Seconds();
#if SWARM_ENABLE_CHANNEL_WRITES
    int32 NumWritten = API.WriteChannel(GetChannel(), Data, Size);
#else
    int32 NumWritten = 0;
#endif
    TotalBytesWritten += NumWritten;
// todo:    TotalSecondsWritten += FPlatformTime::Seconds();
    return NumWritten;
  }

  /**
   * The callback function used by Swarm to communicate to Lightmass.
   * @param CallbackMessage	Message sent from Swarm
   * @param UserParam			User-defined parameter (specified in OpenConnection). Type-casted
   * FSwarm pointer.
   */
  static void SwarmCallback(NSwarm::FMessage *CallbackMessage, void *CallbackData);

  /**
   * Whether Swarm wants us to quit.
   * @return	true if Swarm has told us to quit.
   */
  bool ReceivedQuitRequest() const
  {
    return QuitRequest;
  }

  /**
   * Whether we've received all tasks already and there are no more tasks to work on.
   * @return	true if there are no more tasks in the job to work on
   */
  bool IsDone() const
  {
    return bIsDone;
  }

  /**
   * Prefetch tasks into our work queue, which is where RequestTask gets
   * its work items from.
   */
  void PrefetchTasks();

  /**
   * Thread-safe blocking call to request a new task from the local task queue.
   * It will block until there's a task in the local queue, or the timeout period has passed.
   * If a task is returned, it will asynchronously request a new task from Swarm to keep the queue
   * full. You must call AcceptTask() or RejectTask() if a task is returned.
   *
   * @param TaskGuid	[out] When successful, contains the FGuid for the new task
   * @param WaitTime	Timeout period in milliseconds, or -1 for INFINITE
   * @return			true if a Task Guid is returned, false if the timeout period has passed or
   * IsDone()/ReceivedQuitRequest() is true
   */
  bool RequestTask(FGuid &TaskGuid, uint32 WaitTime = uint32(-1));

  /**
   * Accepts a requested task. This will also notify UE4.
   * @param TaskGuid	The task that is being accepted
   */
  void AcceptTask(const FGuid &TaskGuid);

  /**
   * Rejects a requested task. This will also notify UE4.
   * @param TaskGuid	The task that is being rejected
   */
  void RejectTask(const FGuid &TaskGuid);

  /**
   * Tells Swarm that the task is completed and all results have been fully exported. This will
   * also notify UE4.
   * @param TaskGuid	A guid that identifies the task that has been completed
   */
  void TaskCompleted(const FGuid &TaskGuid);

  /**
   * Tells Swarm that the task has failed. This will also notify UE4.
   * @param TaskGuid	A guid that identifies the task that has failed
   */
  void TaskFailed(const FGuid &TaskGuid);

 private:
//todo:  void VARARGS SendTextMessageImpl(const TCHAR *Fmt, ...);

 public:
  /**
   * Report to Swarm by sending back a file.
   * @param Filename	File to send back
   */
  void ReportFile(const TCHAR *Filename);

  /**
   * Sends a message to Swarm. Thread-safe access.
   * @param Message	Swarm message to send.
   */
  void SendMessage(const NSwarm::FMessage &Message);

  /**
   *	Sends an alert message to Swarm. Thread-safe access.
   *	@param	AlertType		The type of alert.
   *	@param	ObjectGuid		The GUID of the object associated with the alert.
   *	@param	TypeId			The type of object.
   *	@param	MessageText		The text of the message.
   */
  void SendAlertMessage(NSwarm::TAlertLevel AlertLevel,
                        const FGuid &ObjectGuid,
                        const int32 TypeId,
                        const TCHAR *MessageText);

  /**
   * @return the total number of bytes that have been read from Swarm
   */
  uint64 GetTotalBytesRead()
  {
    return TotalBytesRead;
  }

  /**
   * @return the total number of bytes that have been read from Swarm
   */
  uint64 GetTotalBytesWritten()
  {
    return TotalBytesWritten;
  }

  /**
   * @return the total number of seconds spent reading from Swarm
   */
  double GetTotalSecondsRead()
  {
    return TotalSecondsRead;
  }

  /**
   * @return the total number of seconds spent writing to Swarm
   */
  double GetTotalSecondsWritten()
  {
    return TotalSecondsWritten;
  }

  /**
   * @return the total number of reads from Swarm
   */
  uint32 GetTotalNumReads()
  {
    return TotalNumReads;
  }

  /**
   * @return the total number of writes to Swarm
   */
  uint32 GetTotalNumWrites()
  {
    return TotalNumWrites;
  }

 private:
  /**
   * Triggers the task queue enough times to release all blocked threads.
   */
  void TriggerAllThreads();

  /** The Swarm interface. */
  NSwarm::FSwarmInterface &API;

  /** The job guid (the same as the scene guid) */
  FGuid JobGuid;

  /** Critical section to synchronize any access to the Swarm API (sending messages, etc). */
  // todo: FCriticalSection SwarmAccess;

  /** true if there are no more tasks in the job. */
  bool bIsDone;

  /** Set to true when a QUIT message is received from Swarm. */
  bool QuitRequest;

  /** Tasks that have been received from Swarm but not yet handed out to a worker thread. */
//todo:  TProducerConsumerQueue<FGuid> TaskQueue;

  /** Number of outstanding task requests. */
  volatile int32 NumRequestedTasks;

  /** Stack of used channels */
  ccl::vector<int32> ChannelStack;

  /** Total bytes read/written */
  uint64 TotalBytesRead;
  uint64 TotalBytesWritten;

  /** Total number of seconds spent reading from Swarm */
  double TotalSecondsRead;
  /** Total number of seconds spent writing to Swarm */
  double TotalSecondsWritten;
  /** Total number of reads to Swarm */
  uint32 TotalNumReads;
  /** Total number of writes to Swarm */
  uint32 TotalNumWrites;
};


//@todo - need to pass to Serialization
class FImporter {
 protected:
  /**
   * Finds existing or imports new object by Guid
   *
   * @param Key Key of object
   * @param Version Version of object to load
   * @param Extension Type of object to load (@todo UE4: This could be removed if Version could
   * imply extension)
   * @return The object that was loaded or found, or NULL if the Guid failed
   */
  template<class ObjType, class LookupMapType, class KeyType>
  ObjType *ConditionalImportObjectWithKey(const KeyType &Key,
                                          const int32 Version,
                                          const TCHAR *Extension,
                                          int32 ChannelFlags,
                                          LookupMapType &LookupMap);

 public:
  FImporter(class FSwarm *InSwarm);
  ~FImporter();

  /**
   * Imports a scene and all required dependent objects
   *
   * @param Scene Scene object to fill out
   * @param SceneGuid Guid of the scene to load from a swarm channel
   */
  bool ImportScene(class FScene &Scene, const TCHAR* FileName);

  /** Imports a buffer of raw data */
  bool Read(void *Data, int32 NumBytes);

  /** Imports one object */
  template<class DataType> bool ImportData(DataType *Data);

  /** Imports a ccl::vector of simple elements in one bulk read. */
  template<class ArrayType> bool ImportArray(ArrayType &Array, int32 Count);

  /** Imports a ccl::vector of objects, while also adding them to the specified LookupMap. */
  template<class ObjType, class LookupMapType>
  bool ImportObjectArray(ccl::vector<ObjType> &Array, int32 Count, LookupMapType &LookupMap);

  /** Imports an array of GUIDs and stores the corresponding pointers into a ccl::vector */
  template<class ArrayType, class LookupMapType>
  bool ImportGuidArray(ArrayType &Array, int32 Count, const LookupMapType &LookupMap);

  /**
   * Finds existing or imports new object by Guid
   *
   * @param Guid Guid of object
   * @param Version Version of object to load
   * @param Extension Type of object to load (@todo UE4: This could be removed if Version could
   * imply extension)
   * @return The object that was loaded or found, or NULL if the Guid failed
   */
  template<class ObjType, class LookupMapType>
  ObjType *ConditionalImportObject(const FGuid &Guid,
                                   const int32 Version,
                                   const TCHAR *Extension,
                                   int32 ChannelFlags,
                                   LookupMapType &LookupMap)
  {
    return ConditionalImportObjectWithKey<ObjType, LookupMapType, FGuid>(
        Guid, Version, Extension, ChannelFlags, LookupMap);
  }

  /**
   * Finds existing or imports new object by Hash
   *
   * @param Hash Id of object
   * @param Version Version of object to load
   * @param Extension Type of object to load (@todo UE4: This could be removed if Version could
   * imply extension)
   * @return The object that was loaded or found, or NULL if the Guid failed
   */
  template<class ObjType, class LookupMapType>
  ObjType *ConditionalImportObject(const FSHAHash &Hash,
                                   const int32 Version,
                                   const TCHAR *Extension,
                                   int32 ChannelFlags,
                                   LookupMapType &LookupMap)
  {
    return ConditionalImportObjectWithKey<ObjType, LookupMapType, FSHAHash>(
        Hash, Version, Extension, ChannelFlags, LookupMap);
  }

  void SetLevelScale(float InScale)
  {
    LevelScale = InScale;
  }
  float GetLevelScale() const
  {
    // todo: checkf(LevelScale > 0.0f, L"LevelScale must be set by the scene before it can be used");
    return LevelScale;
  }

  ccl::unordered_map<FGuid, class FLight *> &GetLights()
  {
    return Lights;
  }
  ccl::unordered_map<FGuid, class FStaticMeshStaticLightingMesh *> &GetStaticMeshInstances()
  {
    return StaticMeshInstances;
  }
  ccl::unordered_map<FGuid, class FFluidSurfaceStaticLightingMesh *> &GetFluidMeshInstances()
  {
    return FluidMeshInstances;
  }
  ccl::unordered_map<FGuid, class FLandscapeStaticLightingMesh *> &GetLandscapeMeshInstances()
  {
    return LandscapeMeshInstances;
  }
  ccl::unordered_map<FGuid, class FStaticMeshStaticLightingTextureMapping *> &GetTextureMappings()
  {
    return StaticMeshTextureMappings;
  }
  ccl::unordered_map<FGuid, class FBSPSurfaceStaticLighting *> &GetBSPMappings()
  {
    return BSPTextureMappings;
  }
  ccl::unordered_map<FGuid, class FStaticMesh *> &GetStaticMeshes()
  {
    return StaticMeshes;
  }
  ccl::unordered_map<FGuid, class FFluidSurfaceStaticLightingTextureMapping *> &GetFluidMappings()
  {
    return FluidMappings;
  }
  ccl::unordered_map<FGuid, class FLandscapeStaticLightingTextureMapping *> &GetLandscapeMappings()
  {
    return LandscapeMappings;
  }
  ccl::unordered_map<FGuid, class FStaticLightingGlobalVolumeMapping *> &GetVolumeMappings()
  {
    return VolumeMappings;
  }
  ccl::unordered_map<FSHAHash, class FMaterial *> &GetMaterials()
  {
    return Materials;
  }

 private:
  class FSwarm *Swarm;

  ccl::unordered_map<FGuid, class FLight *> Lights;
  ccl::unordered_map<FGuid, class FStaticMesh *> StaticMeshes;
  ccl::unordered_map<FGuid, class FStaticMeshStaticLightingMesh *> StaticMeshInstances;
  ccl::unordered_map<FGuid, class FFluidSurfaceStaticLightingMesh *> FluidMeshInstances;
  ccl::unordered_map<FGuid, class FLandscapeStaticLightingMesh *> LandscapeMeshInstances;
  ccl::unordered_map<FGuid, class FStaticMeshStaticLightingTextureMapping *>
      StaticMeshTextureMappings;
  ccl::unordered_map<FGuid, class FBSPSurfaceStaticLighting *> BSPTextureMappings;
  ccl::unordered_map<FGuid, class FFluidSurfaceStaticLightingTextureMapping *> FluidMappings;
  ccl::unordered_map<FGuid, class FLandscapeStaticLightingTextureMapping *> LandscapeMappings;
  ccl::unordered_map<FGuid, class FStaticLightingGlobalVolumeMapping *> VolumeMappings;
  ccl::unordered_map<FSHAHash, class FMaterial *> Materials;

  float LevelScale;
};

template<typename DataType> FORCEINLINE bool FImporter::ImportData(DataType *Data)
{
  return Read(Data, sizeof(DataType));
}

/** Imports a ccl::vector of simple elements in one bulk read. */
template<class ArrayType> bool FImporter::ImportArray(ArrayType &Array, int32 Count)
{
  Array.resize(Count);
  return Read(Array.data(), Count * sizeof(typename ArrayType::value_type));
}

/** Imports a ccl::vector of objects, while also adding them to the specified LookupMap. */
template<class ObjType, class LookupMapType>
bool FImporter::ImportObjectArray(ccl::vector<ObjType> &Array,
                                           int32 Count,
                                           LookupMapType &LookupMap)
{
  Array.resize(Count);
  for (int32 Index = 0; Index < Count; ++Index) {
    Array[Index].Import(*this);
    LookupMap.insert({Array[Index].Guid, &Array[Index]});
  }
  return true;
}

/** Imports an array of GUIDs and stores the corresponding pointers into a ccl::vector */
template<class ArrayType, class LookupMapType>
bool FImporter::ImportGuidArray(ArrayType &Array,
                                         int32 Count,
                                         const LookupMapType &LookupMap)
{
  bool bOk = true;
  Array.reserve(Count);
  for (int32 Index = 0; bOk && Index < Count; ++Index) {
    FGuid Guid;
    bOk = ImportData(&Guid);
    Array.push_back(LookupMap.find(Guid)->second);
  }
  return bOk;
}

/**
 * Finds existing or imports new object by Guid
 *
 * @param Key Key of object
 * @param Version Version of object to load
 * @param Extension Type of object to load (@todo UE4: This could be removed if Version could imply
 * extension)
 * @return The object that was loaded or found, or NULL if the Guid failed
 */
template<class ObjType, class LookupMapType, class KeyType>
ObjType *FImporter::ConditionalImportObjectWithKey(const KeyType &Key,
                                                            const int32 Version,
                                                            const TCHAR *Extension,
                                                            int32 ChannelFlags,
                                                            LookupMapType &LookupMap)
{
  // look to see if it exists already
  auto iter = LookupMap.find(Key);
  ObjType *Obj = NULL;
  if (iter == LookupMap.end()) {
    // open a new channel and make it current
    auto channel = CreateChannelName(Key, Version, Extension);
    if (Swarm->OpenChannel(channel.c_str(), ChannelFlags, true) >= 0) {
      Obj = new ObjType;

      // import the object from its own channel
      Obj->Import(*this);

      // close the object channel
      Swarm->CloseCurrentChannel();

      // cache this object so it can be found later by another call to this function
      LookupMap.insert({Key, Obj});
    }
  }
  else {
    Obj = iter->second;
  }

  return Obj;
}

class FScene : public FSceneFileHeader {
 public:
  FScene();
  virtual ~FScene();

  virtual void Import(class FImporter &Importer);
  FBoxSphereBounds GetImportanceBounds() const;

  FBox ImportanceBoundingBox;
  ccl::vector<FBox> ImportanceVolumes;

#if __UNREAL_NON_OFFICIAL__
  ccl::vector<FBox> DummyVolumes;
#endif

  ccl::vector<FBox> CharacterIndirectDetailVolumes;
  ccl::vector<FVolumetricLightmapDensityVolume> VolumetricLightmapDensityVolumes;
  ccl::vector<FSphere> Portals;
  ccl::vector<FPrecomputedVisibilityVolume> PrecomputedVisibilityVolumes;
  ccl::vector<FPrecomputedVisibilityOverrideVolume> PrecomputedVisibilityOverrideVolumes;
  ccl::vector<FVector4> CameraTrackPositions;

  ccl::vector<FDirectionalLight> DirectionalLights;
  ccl::vector<FPointLight> PointLights;
  ccl::vector<FSpotLight> SpotLights;
  ccl::vector<FRectLight> RectLights;
  ccl::vector<FSkyLight> SkyLights;

  ccl::vector<FStaticMeshStaticLightingMesh> StaticMeshInstances;
  ccl::vector<FFluidSurfaceStaticLightingMesh> FluidMeshInstances;
  ccl::vector<FLandscapeStaticLightingMesh> LandscapeMeshInstances;
  ccl::vector<FBSPSurfaceStaticLighting> BspMappings;
  ccl::vector<FStaticMeshStaticLightingTextureMapping> TextureLightingMappings;
  ccl::vector<FFluidSurfaceStaticLightingTextureMapping> FluidMappings;
  ccl::vector<FLandscapeStaticLightingTextureMapping> LandscapeMappings;
  ccl::vector<FStaticLightingGlobalVolumeMapping> VolumeMappings;

  ccl::vector<FGuid> VisibilityBucketGuids;
  ccl::vector<FGuid> VolumetricLightmapTaskGuids;

  bool bVerifyEmbree;

  /** The mapping whose texel is selected in Unreal and is being debugged. */
  const class FStaticLightingMapping *DebugMapping;

  const FLight *FindLightByGuid(const FGuid &Guid) const;

  /** Returns true if the specified position is inside any of the importance volumes. */
  bool IsPointInImportanceVolume(const FVector4 &Position, float Tolerance) const;

  bool IsBoxInImportanceVolume(const FBox &QueryBox) const;

  /** Returns true if the specified position is inside any of the visibility volumes. */
  bool IsPointInVisibilityVolume(const FVector4 &Position) const;

  bool DoesBoxIntersectVisibilityVolume(const FBox &TestBounds) const;

  /** Returns accumulated bounds from all the visibility volumes. */
  FBox GetVisibilityVolumeBounds() const;

  bool GetVolumetricLightmapAllowedMipRange(const FVector4 &Position, FIntPoint &OutRange) const;

 private:
  /** Searches through all mapping arrays for the mapping matching FindGuid. */
  const FStaticLightingMapping *FindMappingByGuid(FGuid FindGuid) const;

  /** Applies GeneralSettings.StaticLightingLevelScale to all scale dependent settings. */
  void ApplyStaticLightingScale();
};

class Texture2DLoader : public ccl::ImageLoader {
 public:
  Texture2DLoader(ccl::ustring const& name, const FTexture2D &texture);
  ~Texture2DLoader() override;

  bool load_metadata(const ccl::ImageDeviceFeatures &features,
                     ccl::ImageMetaData &metadata) override;

  bool load_pixels(const ccl::ImageMetaData &metadata,
                   void *pixels,
                   const size_t pixels_size,
                   const bool associate_alpha) override;

  ccl::string name() const override;

  bool equals(const ImageLoader &other) const override;

  void cleanup() override;

  bool is_vdb_loader() const override;

 private:
  const FTexture2D &texture;
  ccl::ustring tex_name;
};

}  // namespace Lightmass

CCL_NAMESPACE_BEGIN

class LightmassTexture2DNode : public ImageSlotTextureNode {
 public:
  SHADER_NODE_NO_CLONE_CLASS(LightmassTexture2DNode)
  ShaderNode *clone(ShaderGraph *graph) const override;
  void attributes(Shader *shader, AttributeRequestSet *attributes) override;
  bool has_attribute_dependency() override
  {
    return true;
  }

  bool equals(const ShaderNode &other) override
  {
    const LightmassTexture2DNode &other_node = (const LightmassTexture2DNode &)other;
    return ImageSlotTextureNode::equals(other) && animated == other_node.animated;
  }

  ImageParams image_params() const;

  /* Parameters. */
  const Lightmass::FTexture2D *texture;
  NODE_SOCKET_API(ustring, tex_name)
  NODE_SOCKET_API(ustring, colorspace)
  NODE_SOCKET_API(ImageAlphaType, alpha_type)
  NODE_SOCKET_API(InterpolationType, interpolation)
  NODE_SOCKET_API(bool, animated)
  NODE_SOCKET_API(float3, vector)
};

CCL_NAMESPACE_END
