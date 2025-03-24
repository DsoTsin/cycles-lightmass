#include "swarm_impl.h"
#include "util/path.h"
#include "zstd.h"


#ifdef _WIN32
// Define WIN32_LEAN_AND_MEAN to exclude rarely-used services from windows headers.
#  define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <assert.h>
#include <metahost.h>
#include <windows.h>

#include <DbgHelp.h>
#include <ErrorRep.h>
#include <Werapi.h>

#pragma comment(lib, "Faultrep.lib")
#pragma comment(lib, "wer.lib")

#pragma comment(lib, "mscoree.lib")

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#endif


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

#ifdef _WIN32
#  ifdef SendMessage
#    undef SendMessage
#  endif
#endif

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

#ifdef _WIN32
#  define PLATFORM_WINDOWS 1
#else
#  define PLATFORM_WINDOWS 0
#endif

uswarm_interface::uswarm_interface() {}
uswarm_interface::~uswarm_interface() {}

typedef int32 (*SwarmOpenConnectionProc)(NSwarm::FConnectionCallback CallbackFunc,
                                         void *CallbackData,
                                         NSwarm::TLogFlags LoggingFlags,
                                         const TCHAR *OptionsFolder);
typedef int32 (*SwarmCloseConnectionProc)(void);
typedef int32 (*SwarmSendMessageProc)(const NSwarm::FMessage *Message);
typedef int32 (*SwarmAddChannelProc)(const TCHAR *FullPath, const TCHAR *ChannelName);
typedef int32 (*SwarmTestChannelProc)(const TCHAR *ChannelName);
typedef int32 (*SwarmOpenChannelProc)(const TCHAR *ChannelName,
                                      NSwarm::TChannelFlags ChannelFlags);
typedef int32 (*SwarmCloseChannelProc)(int32 Channel);
typedef int32 (*SwarmWriteChannelProc)(int32 Channel, const void *Data, int32 DataSize);
typedef int32 (*SwarmReadChannelProc)(int32 Channel, void *Data, int32 DataSize);
typedef int32 (*SwarmOpenJobProc)(const FGuid *JobGuid);
typedef int32 (*SwarmBeginJobSpecificationProc)(const NSwarm::FJobSpecification *Specification32,
                                                const NSwarm::FJobSpecification *Specification64);
typedef int32 (*SwarmAddTaskProc)(const NSwarm::FTaskSpecification *Specification);
typedef int32 (*SwarmEndJobSpecificationProc)(void);
typedef int32 (*SwarmCloseJobProc)(void);
typedef int32 (*SwarmLogProc)(NSwarm::TVerbosityLevel Verbosity,
                              NSwarm::TLogColour TextColour,
                              const TCHAR *Message);
#if __UNREAL_NON_OFFICIAL__
typedef int32 (*SwarmDummyProc0)(uint32 *);
typedef int32 (*SwarmDummyProc1)(uint32 *);
typedef int32 (*SwarmDummyProc2)(bool);
#endif

static SwarmOpenConnectionProc SwarmOpenConnection;
static SwarmCloseConnectionProc SwarmCloseConnection;
static SwarmSendMessageProc SwarmSendMessage;
static SwarmAddChannelProc SwarmAddChannel;
static SwarmTestChannelProc SwarmTestChannel;
static SwarmOpenChannelProc SwarmOpenChannel;
static SwarmCloseChannelProc SwarmCloseChannel;
static SwarmWriteChannelProc SwarmWriteChannel;
static SwarmReadChannelProc SwarmReadChannel;
static SwarmOpenJobProc SwarmOpenJob;
static SwarmBeginJobSpecificationProc SwarmBeginJobSpecification;
static SwarmAddTaskProc SwarmAddTask;
static SwarmEndJobSpecificationProc SwarmEndJobSpecification;
static SwarmCloseJobProc SwarmCloseJob;
static SwarmLogProc SwarmLog;
#if __UNREAL_NON_OFFICIAL__
static SwarmDummyProc0 SwarmDummy0;
static SwarmDummyProc1 SwarmDummy1;
static SwarmDummyProc2 SwarmDummy2;
#endif

extern "C" __declspec(dllexport) void RegisterSwarmOpenConnectionProc(SwarmOpenConnectionProc Proc)
{
  SwarmOpenConnection = Proc;
}
extern "C" __declspec(dllexport) void RegisterSwarmCloseConnectionProc(
    SwarmCloseConnectionProc Proc)
{
  SwarmCloseConnection = Proc;
}
extern "C" __declspec(dllexport) void RegisterSwarmSendMessageProc(SwarmSendMessageProc Proc)
{
  SwarmSendMessage = Proc;
}
extern "C" __declspec(dllexport) void RegisterSwarmAddChannelProc(SwarmAddChannelProc Proc)
{
  SwarmAddChannel = Proc;
}
extern "C" __declspec(dllexport) void RegisterSwarmTestChannelProc(SwarmTestChannelProc Proc)
{
  SwarmTestChannel = Proc;
}
extern "C" __declspec(dllexport) void RegisterSwarmOpenChannelProc(SwarmOpenChannelProc Proc)
{
  SwarmOpenChannel = Proc;
}
extern "C" __declspec(dllexport) void RegisterSwarmCloseChannelProc(SwarmCloseChannelProc Proc)
{
  SwarmCloseChannel = Proc;
}
extern "C" __declspec(dllexport) void RegisterSwarmWriteChannelProc(SwarmWriteChannelProc Proc)
{
  SwarmWriteChannel = Proc;
}
extern "C" __declspec(dllexport) void RegisterSwarmReadChannelProc(SwarmReadChannelProc Proc)
{
  SwarmReadChannel = Proc;
}
extern "C" __declspec(dllexport) void RegisterSwarmOpenJobProc(SwarmOpenJobProc Proc)
{
  SwarmOpenJob = Proc;
}
extern "C" __declspec(dllexport) void RegisterSwarmBeginJobSpecificationProc(
    SwarmBeginJobSpecificationProc Proc)
{
  SwarmBeginJobSpecification = Proc;
}
extern "C" __declspec(dllexport) void RegisterSwarmAddTaskProc(SwarmAddTaskProc Proc)
{
  SwarmAddTask = Proc;
}
extern "C" __declspec(dllexport) void RegisterSwarmEndJobSpecificationProc(
    SwarmEndJobSpecificationProc Proc)
{
  SwarmEndJobSpecification = Proc;
}
extern "C" __declspec(dllexport) void RegisterSwarmCloseJobProc(SwarmCloseJobProc Proc)
{
  SwarmCloseJob = Proc;
}
extern "C" __declspec(dllexport) void RegisterSwarmLogProc(SwarmLogProc Proc)
{
  SwarmLog = Proc;
}

// DECLARE_LOG_CATEGORY_EXTERN(LogSwarmInterface, Verbose, All);
// DEFINE_LOG_CATEGORY(LogSwarmInterface)

extern "C" __declspec(dllexport) void SwarmInterfaceLog(NSwarm::TVerbosityLevel Verbosity,
                                                        const TCHAR *Message)
{
  // switch (Verbosity) {
  //   case VERBOSITY_Critical:
  //     UE_LOG_CLINKAGE(LogSwarmInterface, Error, TEXT("%s"), Message);
  //     break;
  //   case VERBOSITY_Complex:
  //     UE_LOG_CLINKAGE(LogSwarmInterface, Warning, TEXT("%s"), Message);
  //     break;
  //   default:
  //     UE_LOG_CLINKAGE(LogSwarmInterface, Log, TEXT("%s"), Message);
  //     break;
  // }
}

int32 uswarm_interface::OpenConnection(NSwarm::FConnectionCallback CallbackFunc,
                                       void *CallbackData,
                                       NSwarm::TLogFlags LoggingFlags,
                                       const TCHAR *OptionsFolder)
{
  return SwarmOpenConnection(CallbackFunc, CallbackData, LoggingFlags, OptionsFolder);
}

int32 uswarm_interface::CloseConnection(void)
{
  return SwarmCloseConnection();
}

int32 uswarm_interface::SendMessage(const NSwarm::FMessage &Message)
{
  return SwarmSendMessage(&Message);
}

int32 uswarm_interface::AddChannel(const TCHAR *FullPath, const TCHAR *ChannelName)
{
  if (FullPath == NULL) {
    return NSwarm::SWARM_ERROR_INVALID_ARG1;
  }

  if (ChannelName == NULL) {
    return NSwarm::SWARM_ERROR_INVALID_ARG2;
  }

  int32 ReturnValue = SwarmAddChannel(FullPath, ChannelName);
  if (ReturnValue < 0) {
    SendMessage(NSwarm::FInfoMessage(TEXT("Error, fatal in AddChannel")));
  }

  return ReturnValue;
}

int32 uswarm_interface::TestChannel(const TCHAR *ChannelName)
{
  if (ChannelName == NULL) {
    return NSwarm::SWARM_ERROR_INVALID_ARG1;
  }

  int32 ReturnValue = SwarmTestChannel(ChannelName);
  // Check for the one, known error code (file not found)
  if ((ReturnValue < 0) && (ReturnValue != NSwarm::SWARM_ERROR_FILE_FOUND_NOT)) {
    SendMessage(NSwarm::FInfoMessage(TEXT("Error, fatal in TestChannel")));
  }

  return (ReturnValue);
}

int32 uswarm_interface::OpenChannel(const TCHAR *ChannelName, NSwarm::TChannelFlags ChannelFlags)
{
  if (ChannelName == NULL) {
    return (NSwarm::SWARM_ERROR_INVALID_ARG1);
  }

  int32 ReturnValue = SwarmOpenChannel(ChannelName, ChannelFlags);
  if (ReturnValue < 0 && (ChannelFlags & NSwarm::SWARM_CHANNEL_ACCESS_WRITE)) {
    SendMessage(NSwarm::FInfoMessage(TEXT("Error, fatal in OpenChannel")));
  }

  return (ReturnValue);
}

int32 uswarm_interface::CloseChannel(int32 Channel)
{
  if (Channel < 0) {
    return (NSwarm::SWARM_ERROR_INVALID_ARG1);
  }

  int32 ReturnValue = SwarmCloseChannel(Channel);
  if (ReturnValue < 0) {
    SendMessage(NSwarm::FInfoMessage(TEXT("Error, fatal in CloseChannel")));
  }

  return (ReturnValue);
}

int32 uswarm_interface::WriteChannel(int32 Channel, const void *Data, int32 DataSize)
{
  if (Channel < 0) {
    return (NSwarm::SWARM_ERROR_INVALID_ARG1);
  }

  if (Data == NULL) {
    return (NSwarm::SWARM_ERROR_INVALID_ARG2);
  }

  if (DataSize < 0) {
    return (NSwarm::SWARM_ERROR_INVALID_ARG3);
  }

  int32 ReturnValue = SwarmWriteChannel(Channel, Data, DataSize);
  if (ReturnValue < 0) {
    SendMessage(NSwarm::FInfoMessage(TEXT("Error, fatal in WriteChannel")));
  }

  return (ReturnValue);
}

int32 uswarm_interface::ReadChannel(int32 Channel, void *Data, int32 DataSize)
{
  if (Channel < 0) {
    return (NSwarm::SWARM_ERROR_INVALID_ARG1);
  }

  if (Data == NULL) {
    return (NSwarm::SWARM_ERROR_INVALID_ARG2);
  }

  if (DataSize < 0) {
    return (NSwarm::SWARM_ERROR_INVALID_ARG3);
  }

  int32 ReturnValue = SwarmReadChannel(Channel, Data, DataSize);
  if (ReturnValue < 0) {
    SendMessage(NSwarm::FInfoMessage(TEXT("Error, fatal in ReadChannel")));
  }

  return (ReturnValue);
}

int32 uswarm_interface::OpenJob(const FGuid &JobGuid)
{
  int32 ReturnValue = SwarmOpenJob(&JobGuid);
  if (ReturnValue < 0) {
    SendMessage(NSwarm::FInfoMessage(TEXT(" Error, fatal in OpenJob")));
  }

  return (ReturnValue);
}

int32 uswarm_interface::BeginJobSpecification(const NSwarm::FJobSpecification &Specification32,
                                              const NSwarm::FJobSpecification &Specification64)
{
  if (Specification32.ExecutableName == NULL && Specification64.ExecutableName == NULL) {
    return (NSwarm::SWARM_ERROR_INVALID_ARG);
  }

  if (Specification32.Parameters == NULL && Specification64.Parameters == NULL) {
    return (NSwarm::SWARM_ERROR_INVALID_ARG);
  }

  if ((Specification32.RequiredDependencyCount > 0 &&
       Specification32.RequiredDependencies == NULL) ||
      (Specification32.OptionalDependencyCount > 0 &&
       Specification32.OptionalDependencies == NULL) ||
      (Specification64.RequiredDependencyCount > 0 &&
       Specification64.RequiredDependencies == NULL) ||
      (Specification64.OptionalDependencyCount > 0 &&
       Specification64.OptionalDependencies == NULL))
  {
    return (NSwarm::SWARM_ERROR_INVALID_ARG);
  }

  int32 ReturnValue = SwarmBeginJobSpecification(&Specification32, &Specification64);
  if (ReturnValue < 0) {
    SendMessage(NSwarm::FInfoMessage(TEXT("Error, fatal in BeginJobSpecification")));
  }

  return (ReturnValue);
}

int32 uswarm_interface::AddTask(const NSwarm::FTaskSpecification &Specification)
{
  if (Specification.Parameters == NULL) {
    return (NSwarm::SWARM_ERROR_INVALID_ARG);
  }

  if ((Specification.DependencyCount > 0) && (Specification.Dependencies == NULL)) {
    return (NSwarm::SWARM_ERROR_INVALID_ARG);
  }

  int32 ReturnValue = SwarmAddTask(&Specification);
  if (ReturnValue < 0) {
    SendMessage(NSwarm::FInfoMessage(TEXT("Error, fatal in AddTask")));
  }

  return (ReturnValue);
}

int32 uswarm_interface::EndJobSpecification(void)
{
  int32 ReturnValue = SwarmEndJobSpecification();
  if (ReturnValue < 0) {
    SendMessage(NSwarm::FInfoMessage(TEXT("Error, fatal in EndJobSpecification")));
  }

  return (ReturnValue);
}

int32 uswarm_interface::CloseJob(void)
{
  int32 ReturnValue = SwarmCloseJob();
  if (ReturnValue < 0) {
    SendMessage(NSwarm::FInfoMessage(TEXT("Error, fatal in CloseJob")));
  }

  return (ReturnValue);
}

int32 uswarm_interface::Log(NSwarm::TVerbosityLevel Verbosity,
                            NSwarm::TLogColour TextColour,
                            const TCHAR *Message)
{
  if (Message == NULL) {
    return (NSwarm::SWARM_ERROR_NULL_POINTER);
  }

  int32 ReturnValue = SwarmLog(Verbosity, TextColour, Message);

  return (ReturnValue);
}

void uswarm_interface::SetJobGuid(const FGuid &JobGuid) {}

bool uswarm_interface::IsJobProcessRunning(int32 *OutStatus)
{
  return false;
}

bool uswarm_interface::initialize(const TCHAR *dll_path)
{
#if PLATFORM_WINDOWS
  ICLRMetaHost *MetaHost = NULL;
  ICLRRuntimeHost *RuntimeHost = NULL;

  HRESULT Result = CLRCreateInstance(CLSID_CLRMetaHost, IID_ICLRMetaHost, (LPVOID *)&MetaHost);

  if (SUCCEEDED(Result)) {
    TCHAR NetFrameworkVersion[255];
    uint32 VersionLength = 255;
    Result = MetaHost->GetVersionFromFile(
        dll_path, NetFrameworkVersion, (unsigned long *)&VersionLength);
    if (FAILED(Result)) {
      dll_path = TEXT("SwarmInterface.dll");
      Result = MetaHost->GetVersionFromFile(
          dll_path, NetFrameworkVersion, (unsigned long *)&VersionLength);
    }

    if (SUCCEEDED(Result)) {
      ICLRRuntimeInfo *RuntimeInfo = NULL;
      Result = MetaHost->GetRuntime(
          NetFrameworkVersion, IID_ICLRRuntimeInfo, (LPVOID *)&RuntimeInfo);

      if (SUCCEEDED(Result)) {
        Result = RuntimeInfo->GetInterface(
            CLSID_CLRRuntimeHost, IID_ICLRRuntimeHost, (LPVOID *)&RuntimeHost);
        // ensureMsgf(SUCCEEDED(Result), TEXT("Error requesting Swarm interface."));
      }
    }
  }

  if (SUCCEEDED(Result)) {
    Result = RuntimeHost->Start();
    // ensureMsgf(SUCCEEDED(Result), TEXT("Cannot start Swarm runtime host."));
  }

  if (FAILED(Result)) {
    // ensureMsgf(false, TEXT("Error initializing Swarm interface."));
    return false;
  }

  TCHAR SwarmInterfaceDllName[MAX_PATH];
  GetModuleFileName((HINSTANCE)&__ImageBase, SwarmInterfaceDllName, MAX_PATH);

  uint32 ReturnValue = 0;
  Result = RuntimeHost->ExecuteInDefaultAppDomain(dll_path,
                                                  TEXT("NSwarm.FSwarmInterface"),
                                                  TEXT("InitCppBridgeCallbacks"),
                                                  SwarmInterfaceDllName,
                                                  (unsigned long *)&ReturnValue);

  if (FAILED(Result)) {
    // ensureMsgf(false, TEXT("Error creating Swarm instance bridge."));
    return false;
  }
#endif  // PLATFORM_WINDOWS

  return true;
}

SwarmOutputDriver::SwarmOutputDriver() {}

SwarmOutputDriver::~SwarmOutputDriver() {}

void SwarmOutputDriver::write_render_tile(const Tile &tile) {

}

// @see blender/source/blender/render/intern/bake.cc
bool SwarmOutputDriver::read_render_tile(const Tile &tile)
{
  //tile.set_pass_pixels(b_pass.name(), b_pass.channels(), (float *)b_pass.rect());
  return OutputDriver::read_render_tile(tile);
}

CCL_NAMESPACE_END

