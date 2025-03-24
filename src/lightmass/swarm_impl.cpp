#include "swarm_impl.h"
#include "util/path.h"
#include "zstd.h"

CCL_NAMESPACE_BEGIN

struct compression_stream : public IChannelStream {
  compression_stream(const string &InFile) : filename(InFile)
  {
    bytes_read = 0;
    total_read = 0;
    bytes_remain = 0;
    file = std::make_unique<std::ifstream>(InFile, std::ios_base::binary);
    check(file->good());
    file->seekg(0, std::ios_base::end);
    file_size = file->tellg();
    file->seekg(0, std::ios_base::beg);
    stream = ZSTD_createDStream();
    check(stream);
    size_t initResult = ZSTD_initDStream(stream);
    check(!ZSTD_isError(initResult));
  }

  ~compression_stream() override {}

  /**
   * Reads data from a channel opened for READ into the provided buffer
   *
   * @param Data Destination buffer for the read
   * @param NumBytes Size of the destination buffer
   *
   * @return The number of bytes read (< 0 is an error)
   */
  virtual int32 Read(void *Data, int32 NumBytes) override
  {
    total_read += NumBytes;
    if (bytes_remain >= NumBytes) {
      memcpy(Data, remain_decomp_data.data() + bytes_read, NumBytes);
      bytes_read += NumBytes;
      bytes_remain -= NumBytes;
      return NumBytes;
    }
    else {
      if (bytes_remain > 0) {
        remain_decomp_data.slice(bytes_read);  // Shrink the buffer
      }
      bytes_read = 0;
      // RemainBytes = 0;
    }
    ccl::vector<uint8> ReadInBuf;
    ccl::vector<uint8> ReadOutBuf;

    ReadOutBuf.resize(ZSTD_DStreamOutSize());
    do {
      auto COffset = file->tellg();
      ReadInBuf.resize((int32)std::min<int64>(ZSTD_DStreamInSize(), file_size - COffset));
      file->read((char *)ReadInBuf.data(), ReadInBuf.size());
      if (COffset == file_size || file->eof()) {
        int64 Offset = file->tellg();
        check(0);
        return 0;
      }
      ZSTD_inBuffer Zinput = {ReadInBuf.data(), ReadInBuf.size(), 0};
      while (Zinput.pos < Zinput.size) {
        ZSTD_outBuffer Zoutput = {ReadOutBuf.data(), ReadOutBuf.size(), 0};
        size_t ret = ZSTD_decompressStream(stream, &Zoutput, &Zinput);
        checkf(!ZSTD_isError(ret), TEXT("%hs"), ZSTD_getErrorName(ret));
        if (Zoutput.pos > 0) {
          remain_decomp_data.append(ReadOutBuf.data(), Zoutput.pos);
        }
      }
    } while (remain_decomp_data.size() < NumBytes);

    memcpy(Data, remain_decomp_data.data(), NumBytes);

    bytes_read += NumBytes;
    bytes_remain = remain_decomp_data.size() - NumBytes;
    return NumBytes;
  }

  ZSTD_DStream *stream;
  int64 file_size;
  int32 bytes_read;
  int64 total_read;
  int32 bytes_remain;
  string filename;
  std::unique_ptr<std::ifstream> file;
  aux_buffer remain_decomp_data;

  void release()
  {
    if (stream) {
      ZSTD_freeDStream(stream);
      stream = nullptr;
    }
    remain_decomp_data.release();
  }
};

swarm_importer::swarm_importer(const TCHAR *Dir) : _dir(string_from_wstring(Dir)) {}
swarm_importer::~swarm_importer(void) {}
int32 swarm_importer::OpenConnection(NSwarm::FConnectionCallback CallbackFunc,
                                     void *CallbackData,
                                     NSwarm::TLogFlags LoggingFlags,
                                     const TCHAR *OptionsFolder)
{
  return 0;
}
int32 swarm_importer::CloseConnection(void)
{
  return 0;
}
int32 swarm_importer::SendMessage(const NSwarm::FMessage &Message)
{
  return 0;
}
int32 swarm_importer::AddChannel(const TCHAR *FullPath, const TCHAR *ChannelName)
{
  return 0;
}
int32 swarm_importer::TestChannel(const TCHAR *ChannelName)
{
  return 0;
}
int32 swarm_importer::OpenChannel(const TCHAR *ChannelName, NSwarm::TChannelFlags ChannelFlags)
{
  check(NSwarm::SWARM_CHANNEL_MISC_ENABLE_COMPRESSION & ChannelFlags);
  map.insert(
      {counter,
       std::make_shared<compression_stream>(path_join(_dir, string_from_wstring(ChannelName)))});
  int32 ChannelId = counter++;
  return ChannelId;
}
int32 swarm_importer::CloseChannel(int32 Channel)
{
  map.erase(Channel);
  return 0;
}
int32 swarm_importer::WriteChannel(int32 Channel, const void *Data, int32 DataSize)
{
  return 0;
}
int32 swarm_importer::ReadChannel(int32 Channel, void *Data, int32 DataSize)
{
  check(map.find(Channel) != map.end());
  return map[Channel]->Read(Data, DataSize);
}
int32 swarm_importer::OpenJob(const FGuid &JobGuid)
{
  return 0;
}
int32 swarm_importer::BeginJobSpecification(const NSwarm::FJobSpecification &Specification32,
                                            const NSwarm::FJobSpecification &Specification64)
{
  return 0;
}
int32 swarm_importer::AddTask(const NSwarm::FTaskSpecification &Specification)
{
  return 0;
}
int32 swarm_importer::EndJobSpecification(void)
{
  return 0;
}
int32 swarm_importer::CloseJob(void)
{
  return 0;
}
int32 swarm_importer::Log(NSwarm::TVerbosityLevel Verbosity,
                          NSwarm::TLogColour TextColour,
                          const TCHAR *Message)
{
  return 0;
}
void swarm_importer::SetJobGuid(const FGuid &JobGuid) {}
bool swarm_importer::IsJobProcessRunning(int32 *OutStatus)
{
  return false;
}

SwarmOutputDriver::SwarmOutputDriver() {}

SwarmOutputDriver::~SwarmOutputDriver() {}

void SwarmOutputDriver::write_render_tile(const Tile &tile) {

}

bool SwarmOutputDriver::read_render_tile(const Tile &tile)
{
  //tile.set_pass_pixels(b_pass.name(), b_pass.channels(), (float *)b_pass.rect());
  return OutputDriver::read_render_tile(tile);
}

CCL_NAMESPACE_END
