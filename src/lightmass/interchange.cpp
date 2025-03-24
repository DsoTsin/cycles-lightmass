/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <cstdio>

#include <algorithm>
#include "mapping.h"
#include "interchange.h"
#include "mesh.h"
#include "light.h"

#include "scene/osl.h"
#include "scene/scene.h"
#include "scene/svm.h"

CCL_NAMESPACE_BEGIN

CCL_NAMESPACE_END

namespace Lightmass {

static int32 SwarmConnectionDroppedExitCode = 2;

/**
 * Constructs the Swarm wrapper used by Lightmass.
 * @param SwarmInterface	The global SwarmInterface to use
 * @param JobGuid			Guid that identifies the job we're working on
 * @param TaskQueueSize		Number of tasks we should try to keep in the queue
 */
FSwarm::FSwarm(NSwarm::FSwarmInterface &SwarmInterface)
    : API(SwarmInterface),
      bIsDone(false),
      QuitRequest(false),
      //todo: TaskQueue(TaskQueueSize),
      NumRequestedTasks(0),
      TotalBytesRead(0),
      TotalBytesWritten(0),
      TotalSecondsRead(0.0),
      TotalSecondsWritten(0.0),
      TotalNumReads(0),
      TotalNumWrites(0)
{
  API.SetJobGuid(JobGuid);
}

FSwarm::~FSwarm()
{
  API.CloseConnection();
}

/**
 * Opens a new channel alongside the root channel owned by the LightmassSwarm object
 *
 * @param ChannelName Name of the channel
 * @param ChannelFlags Flags (read, write, etc) for the channel
 * @param bPushChannel If true, this new channel will be auto-pushed onto the stack
 *
 * @return The channel that was opened, without disturbing the root channel
 */
int32 FSwarm::OpenChannel(const TCHAR *ChannelName, int32 ChannelFlags, bool bPushChannel)
{
  int32 NewChannel = API.OpenChannel(ChannelName, (NSwarm::TChannelFlags)ChannelFlags);
  if ((NewChannel == NSwarm::SWARM_ERROR_CONNECTION_NOT_FOUND) ||
      (NewChannel == NSwarm::SWARM_ERROR_CONNECTION_DISCONNECTED))
  {
    // The connection has dropped, exit with a special code
    exit(SwarmConnectionDroppedExitCode);
  }

  if (bPushChannel) {
    PushChannel(NewChannel);
  }

  return NewChannel;
}

/**
 * Closes a channel previously opened with OpenChannel
 *
 * @param Channel The channel to close
 */
void FSwarm::CloseChannel(int32 Channel)
{
  int32 ReturnCode = API.CloseChannel(Channel);
  if ((ReturnCode == NSwarm::SWARM_ERROR_CONNECTION_NOT_FOUND) ||
      (ReturnCode == NSwarm::SWARM_ERROR_CONNECTION_DISCONNECTED))
  {
    // The connection has dropped, exit with a special code
    exit(SwarmConnectionDroppedExitCode);
  }
}

/**
 * Pushes a new channel on the stack as the current channel to read from
 *
 * @param Channel New channel to read from
 */
void FSwarm::PushChannel(int32 Channel)
{
//todo:  FScopeLock Lock(&SwarmAccess);
  ChannelStack.push_back(Channel);
}

/**
 * Pops the top channel
 *
 * @param bCloseChannel If true, the channel will be closed when it is popped off
 */
void FSwarm::PopChannel(bool bCloseChannel)
{
  int32 PoppedChannel;
  {
//todo:    FScopeLock Lock(&SwarmAccess);
    PoppedChannel = ChannelStack.back();
    ChannelStack.pop_back();
  }

  if (bCloseChannel) {
    int32 ReturnCode = API.CloseChannel(PoppedChannel);
    if ((ReturnCode == NSwarm::SWARM_ERROR_CONNECTION_NOT_FOUND) ||
        (ReturnCode == NSwarm::SWARM_ERROR_CONNECTION_DISCONNECTED))
    {
      // The connection has dropped, exit with a special code
      exit(SwarmConnectionDroppedExitCode);
    }
  }
}

/**
 * The callback function used by Swarm to communicate to Lightmass.
 * @param CallbackMessage	Message sent from Swarm
 * @param UserParam			User-defined parameter (specified in OpenConnection). Type-casted
 * FSwarm pointer.
 */
void FSwarm::SwarmCallback(NSwarm::FMessage *CallbackMessage, void *UserParam)
{
}


FImporter::FImporter(FSwarm *InSwarm) : Swarm(InSwarm), LevelScale(0.0f)
{
}

FImporter::~FImporter() {}

/**
 * Imports a scene and all required dependent objects
 *
 * @param Scene Scene object to fill out
 * @param SceneGuid Guid of the scene to load from a swarm channel
 */
bool FImporter::ImportScene(class FScene &Scene, const TCHAR *FileName)
{
  int32 ErrorCode = Swarm->OpenChannel(
      FileName,
      LM_SCENE_CHANNEL_FLAGS,
      true);
  if (ErrorCode >= 0) {
    Scene.Import(*this);

    Swarm->CloseCurrentChannel();
    return true;
  }
  else {
    //Swarm->SendTextMessage(
    //    TEXT("Failed to open scene channel with GUID {%08x}:{%08x}:{%08x}:{%08x}"),
    //    SceneGuid.A,
    //    SceneGuid.B,
    //    SceneGuid.C,
    //    SceneGuid.D);
  }

  return false;
}

bool FImporter::Read(void *Data, int32 NumBytes)
{
  int32 NumRead = Swarm->Read(Data, NumBytes);
  return NumRead == NumBytes;
}

void FScene::ApplyStaticLightingScale() {}

Texture2DLoader::Texture2DLoader(ccl::ustring const &name, const FTexture2D &in_texture)
    : texture(in_texture), tex_name(name)
{
}

Texture2DLoader::~Texture2DLoader() {}

bool Texture2DLoader::load_metadata(const ccl::ImageDeviceFeatures &features,
                                    ccl::ImageMetaData &metadata)
{
  metadata.channels = 4;
  metadata.width = texture.GetSizeX();
  metadata.height = texture.GetSizeY();
  metadata.byte_size = texture.GetMemorySize();
  // ARGB
  metadata.type = texture.GetFormat() == TF_ARGB16F ? ccl::ImageDataType::IMAGE_DATA_TYPE_HALF4:
      ccl::ImageDataType::IMAGE_DATA_TYPE_BYTE4;
  return true;
}

bool Texture2DLoader::load_pixels(const ccl::ImageMetaData &metadata,
                                  void *pixels,
                                  const size_t pixels_size,
                                  const bool associate_alpha)
{
  int64_t pixels_num = (int64_t)texture.GetSizeX() * texture.GetSizeY();
  switch (metadata.type) {
    case ccl::ImageDataType::IMAGE_DATA_TYPE_BYTE4: {
      ccl::uchar4 *pixels_data = reinterpret_cast<ccl::uchar4 *>(pixels);
      ccl::uchar4 *texels_data = reinterpret_cast<ccl::uchar4 *>(texture.GetData());
      for (int64_t pixel_index = 0; pixel_index < pixels_num; pixel_index++) {
        ccl::uchar4 pixel = texels_data[pixel_index];
        pixels_data[pixel_index] = ccl::uchar4{pixel.y, pixel.z, pixel.w, pixel.x};
      }
      break;
    }
    case ccl::ImageDataType::IMAGE_DATA_TYPE_HALF4: {
      ccl::half4 *pixels_data = reinterpret_cast<ccl::half4 *>(pixels);
      ccl::half4 *texels_data = reinterpret_cast<ccl::half4 *>(texture.GetData());
      for (int64_t pixel_index = 0; pixel_index < pixels_num; pixel_index++) {
        ccl::half4 pixel = texels_data[pixel_index];
        pixels_data[pixel_index] = ccl::half4{pixel.y, pixel.z, pixel.w, pixel.x};
      }
      break;
    }
  }
  memcpy(pixels, texture.GetData(), texture.GetMemorySize());
  return true;
}

ccl::string Texture2DLoader::name() const
{
  return tex_name.c_str();
}

bool Texture2DLoader::equals(const ImageLoader &) const
{
  return false;
}

void Texture2DLoader::cleanup() {}

bool Texture2DLoader::is_vdb_loader() const
{
  return false;
}

}  // namespace Lightmass

CCL_NAMESPACE_BEGIN

#define TEXTURE_MAPPING_DEFINE(TextureNode) \
  SOCKET_POINT(tex_mapping.translation, "Translation", zero_float3()); \
  SOCKET_VECTOR(tex_mapping.rotation, "Rotation", zero_float3()); \
  SOCKET_VECTOR(tex_mapping.scale, "Scale", one_float3()); \
\
  SOCKET_VECTOR(tex_mapping.min, "Min", make_float3(-FLT_MAX, -FLT_MAX, -FLT_MAX)); \
  SOCKET_VECTOR(tex_mapping.max, "Max", make_float3(FLT_MAX, FLT_MAX, FLT_MAX)); \
  SOCKET_BOOLEAN(tex_mapping.use_minmax, "Use Min Max", false); \
\
  static NodeEnum mapping_axis_enum; \
  mapping_axis_enum.insert("none", TextureMapping::NONE); \
  mapping_axis_enum.insert("x", TextureMapping::X); \
  mapping_axis_enum.insert("y", TextureMapping::Y); \
  mapping_axis_enum.insert("z", TextureMapping::Z); \
  SOCKET_ENUM(tex_mapping.x_mapping, "x_mapping", mapping_axis_enum, TextureMapping::X); \
  SOCKET_ENUM(tex_mapping.y_mapping, "y_mapping", mapping_axis_enum, TextureMapping::Y); \
  SOCKET_ENUM(tex_mapping.z_mapping, "z_mapping", mapping_axis_enum, TextureMapping::Z); \
\
  static NodeEnum mapping_type_enum; \
  mapping_type_enum.insert("point", TextureMapping::POINT); \
  mapping_type_enum.insert("texture", TextureMapping::TEXTURE); \
  mapping_type_enum.insert("vector", TextureMapping::VECTOR); \
  mapping_type_enum.insert("normal", TextureMapping::NORMAL); \
  SOCKET_ENUM(tex_mapping.type, "Type", mapping_type_enum, TextureMapping::TEXTURE); \
\
  static NodeEnum mapping_projection_enum; \
  mapping_projection_enum.insert("flat", TextureMapping::FLAT); \
  mapping_projection_enum.insert("cube", TextureMapping::CUBE); \
  mapping_projection_enum.insert("tube", TextureMapping::TUBE); \
  mapping_projection_enum.insert("sphere", TextureMapping::SPHERE); \
  SOCKET_ENUM(tex_mapping.projection, "Projection", mapping_projection_enum, TextureMapping::FLAT);

NODE_DEFINE(LightmassTexture2DNode)
{
  NodeType *type = NodeType::add("lightmass_texture", create, NodeType::SHADER);

  TEXTURE_MAPPING_DEFINE(LightmassTexture2DNode);

  SOCKET_STRING(tex_name, "Filename", ustring());
  SOCKET_STRING(colorspace, "Colorspace", u_colorspace_auto);

  static NodeEnum alpha_type_enum;
  alpha_type_enum.insert("auto", IMAGE_ALPHA_AUTO);
  alpha_type_enum.insert("unassociated", IMAGE_ALPHA_UNASSOCIATED);
  alpha_type_enum.insert("associated", IMAGE_ALPHA_ASSOCIATED);
  alpha_type_enum.insert("channel_packed", IMAGE_ALPHA_CHANNEL_PACKED);
  alpha_type_enum.insert("ignore", IMAGE_ALPHA_IGNORE);
  SOCKET_ENUM(alpha_type, "Alpha Type", alpha_type_enum, IMAGE_ALPHA_AUTO);

  static NodeEnum interpolation_enum;
  interpolation_enum.insert("closest", INTERPOLATION_CLOSEST);
  interpolation_enum.insert("linear", INTERPOLATION_LINEAR);
  interpolation_enum.insert("cubic", INTERPOLATION_CUBIC);
  interpolation_enum.insert("smart", INTERPOLATION_SMART);
  SOCKET_ENUM(interpolation, "Interpolation", interpolation_enum, INTERPOLATION_LINEAR);

  static NodeEnum extension_enum;
  extension_enum.insert("periodic", EXTENSION_REPEAT);
  extension_enum.insert("clamp", EXTENSION_EXTEND);
  extension_enum.insert("black", EXTENSION_CLIP);
  extension_enum.insert("mirror", EXTENSION_MIRROR);
//  SOCKET_ENUM(extension, "Extension", extension_enum, EXTENSION_REPEAT);

  static NodeEnum projection_enum;
  projection_enum.insert("flat", NODE_IMAGE_PROJ_FLAT);
  projection_enum.insert("box", NODE_IMAGE_PROJ_BOX);
  projection_enum.insert("sphere", NODE_IMAGE_PROJ_SPHERE);
  projection_enum.insert("tube", NODE_IMAGE_PROJ_TUBE);
  //SOCKET_ENUM(projection, "Projection", projection_enum, NODE_IMAGE_PROJ_FLAT);

  //SOCKET_FLOAT(projection_blend, "Projection Blend", 0.0f);

  //SOCKET_INT_ARRAY(tiles, "Tiles", array<int>());
  SOCKET_BOOLEAN(animated, "Animated", false);

  SOCKET_IN_POINT(vector, "Vector", zero_float3(), SocketType::LINK_TEXTURE_UV);

  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(alpha, "Alpha");

  return type;
}

LightmassTexture2DNode::LightmassTexture2DNode() : ImageSlotTextureNode(get_node_type()), texture(nullptr)
{
  colorspace = u_colorspace_raw;
  animated = false;
}

ShaderNode *LightmassTexture2DNode::clone(ShaderGraph *graph) const
{
  LightmassTexture2DNode *node = graph->create_node<LightmassTexture2DNode>(*this);
  node->handle = handle;
  return node;
}

ImageParams LightmassTexture2DNode::image_params() const
{
  ImageParams params;
  params.animated = animated;
  params.interpolation = interpolation;
  params.extension = EXTENSION_REPEAT;
  params.alpha_type = alpha_type;
  params.colorspace = colorspace;
  return params;
}

void LightmassTexture2DNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  ShaderNode::attributes(shader, attributes);
}

// todo: Texture2DLoader
void LightmassTexture2DNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");
  ShaderOutput *color_out = output("Color");
  ShaderOutput *alpha_out = output("Alpha");

  if (handle.empty()) {
    ImageManager *image_manager = compiler.scene->image_manager.get();
    handle = image_manager->add_image(make_unique<Lightmass::Texture2DLoader>(tex_name, *texture),
                                      image_params());
  }

  const ImageMetaData metadata = handle.metadata();
  const bool compress_as_srgb = metadata.compress_as_srgb;

  const int vector_offset = tex_mapping.compile_begin(compiler, vector_in);
  uint flags = 0;

  if (compress_as_srgb) {
    flags |= NODE_IMAGE_COMPRESS_AS_SRGB;
  }

  compiler.add_node(NODE_TEX_IMAGE,
                    handle.svm_slot(),
                    compiler.encode_uchar4(vector_offset,
                                           compiler.stack_assign_if_linked(color_out),
                                           compiler.stack_assign_if_linked(alpha_out),
                                           flags),
                    TextureMapping::FLAT);

  tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void LightmassTexture2DNode::compile(OSLCompiler &compiler)
{
  if (handle.empty()) {
    ImageManager *image_manager = compiler.scene->image_manager.get();
    handle = image_manager->add_image(make_unique<Lightmass::Texture2DLoader>(tex_name, *texture),
                                      image_params());
  }

  tex_mapping.compile(compiler);

  const ImageMetaData metadata = handle.metadata();
  const bool is_float = metadata.is_float();
  const bool compress_as_srgb = metadata.compress_as_srgb;
  const ustring known_colorspace = metadata.colorspace;

  if (handle.svm_slot() == -1) {
    compiler.parameter_texture(
        "filename", tex_name, compress_as_srgb ? u_colorspace_raw : known_colorspace);
  }
  else {
    compiler.parameter_texture("filename", handle);
  }

  compiler.parameter(this, "projection");
  compiler.parameter(this, "interpolation");
  compiler.parameter("compress_as_srgb", compress_as_srgb);
  compiler.parameter("ignore_alpha", alpha_type == IMAGE_ALPHA_IGNORE);
  compiler.parameter("is_float", is_float);
  compiler.add(this, "node_image_texture");
}

CCL_NAMESPACE_END
