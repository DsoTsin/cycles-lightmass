/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util.h"
#include "SwarmInterface.h"
#include "session/output_driver.h"
#include "util/string.h"

CCL_NAMESPACE_BEGIN

using namespace Lightmass;
using Lightmass::TCHAR;

struct IChannelStream {
  virtual ~IChannelStream() {}
  /**
   * Reads data from a channel opened for READ into the provided buffer
   *
   * @param Data Destination buffer for the read
   * @param NumBytes Size of the destination buffer
   *
   * @return The number of bytes read (< 0 is an error)
   */
  virtual int32 Read(void *Data, int32 NumBytes) = 0;
};

class swarm_importer : public NSwarm::FSwarmInterface {
 public:
  swarm_importer(const TCHAR *Dir);
  virtual ~swarm_importer(void) override;
  virtual int32 OpenConnection(NSwarm::FConnectionCallback CallbackFunc,
                               void *CallbackData,
                               NSwarm::TLogFlags LoggingFlags,
                               const TCHAR *OptionsFolder) override;
  virtual int32 CloseConnection(void) override;
  virtual int32 SendMessage(const NSwarm::FMessage &Message) override;
  virtual int32 AddChannel(const TCHAR *FullPath, const TCHAR *ChannelName) override;
  virtual int32 TestChannel(const TCHAR *ChannelName) override;
  virtual int32 OpenChannel(const TCHAR *ChannelName, NSwarm::TChannelFlags ChannelFlags) override;
  virtual int32 CloseChannel(int32 Channel) override;
  virtual int32 WriteChannel(int32 Channel, const void *Data, int32 DataSize) override;
  virtual int32 ReadChannel(int32 Channel, void *Data, int32 DataSize) override;
  virtual int32 OpenJob(const FGuid &JobGuid) override;
  virtual int32 BeginJobSpecification(const NSwarm::FJobSpecification &Specification32,
                                      const NSwarm::FJobSpecification &Specification64) override;
  virtual int32 AddTask(const NSwarm::FTaskSpecification &Specification) override;
  virtual int32 EndJobSpecification(void) override;
  virtual int32 CloseJob(void) override;
  virtual int32 Log(NSwarm::TVerbosityLevel Verbosity,
                    NSwarm::TLogColour TextColour,
                    const TCHAR *Message) override;
  virtual void SetJobGuid(const FGuid &JobGuid) override;
  virtual bool IsJobProcessRunning(int32 *OutStatus) override;

 private:
  string _dir;
  uint32 counter = 0;
  ccl::map<int32, std::shared_ptr<IChannelStream>> map;
};

class uswarm_interface : public NSwarm::FSwarmInterface {
 public:
  static bool initialize(const TCHAR *dll_path);

  uswarm_interface();
  virtual ~uswarm_interface(void) override;
  virtual int32 OpenConnection(NSwarm::FConnectionCallback CallbackFunc,
                               void *CallbackData,
                               NSwarm::TLogFlags LoggingFlags,
                               const TCHAR *OptionsFolder) override;
  virtual int32 CloseConnection(void) override;
  virtual int32 SendMessage(const NSwarm::FMessage &Message) override;
  virtual int32 AddChannel(const TCHAR *FullPath, const TCHAR *ChannelName) override;
  virtual int32 TestChannel(const TCHAR *ChannelName) override;
  virtual int32 OpenChannel(const TCHAR *ChannelName, NSwarm::TChannelFlags ChannelFlags) override;
  virtual int32 CloseChannel(int32 Channel) override;
  virtual int32 WriteChannel(int32 Channel, const void *Data, int32 DataSize) override;
  virtual int32 ReadChannel(int32 Channel, void *Data, int32 DataSize) override;
  virtual int32 OpenJob(const FGuid &JobGuid) override;
  virtual int32 BeginJobSpecification(const NSwarm::FJobSpecification &Specification32,
                                      const NSwarm::FJobSpecification &Specification64) override;
  virtual int32 AddTask(const NSwarm::FTaskSpecification &Specification) override;
  virtual int32 EndJobSpecification(void) override;
  virtual int32 CloseJob(void) override;
  virtual int32 Log(NSwarm::TVerbosityLevel Verbosity,
                    NSwarm::TLogColour TextColour,
                    const TCHAR *Message) override;
  virtual void SetJobGuid(const FGuid &JobGuid) override;
  virtual bool IsJobProcessRunning(int32 *OutStatus) override;

};

// Per Object bake driver
// input: rasterization cache,
// output: lightmap, volumetric lightmaps and shadow maps
class SwarmOutputDriver : public OutputDriver {
 public:
  SwarmOutputDriver();
  ~SwarmOutputDriver() override;

  void write_render_tile(const Tile &tile) override;
  
  /* For baking, read render pass PASS_BAKE_PRIMITIVE/SEED/DIFFERENTIAL
   * to determine which shading points to use for baking at each pixel. Return
   * true if any data was read. */
  bool read_render_tile(const Tile& tile) override;
};
CCL_NAMESPACE_END
