/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <cstdio>

#include <algorithm>

#include "scene/alembic.h"
#include "scene/background.h"
#include "scene/camera.h"
#include "scene/film.h"
#include "scene/integrator.h"
#include "scene/light.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/osl.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "scene/bake.h"

#include "util/path.h"
#include "util/projection.h"
#include "util/transform.h"

#include "util/string.h"
#include "util/path.h"

#include "lightmass/mapping.h"
#include "lightmass/scene.h"


#ifdef SendMessage
#undef SendMessage
#endif

#include "lightmass/swarm_impl.h"
#include "lightmass/interchange.h"

CCL_NAMESPACE_BEGIN

static Mesh *swarm_add_mesh(Scene *scene, const Transform &tfm, Object *object);
//static void swarm_read_mesh(Scene *scene, Lightmass::FStaticMesh& mesh);
static void swarm_read_mesh_instance(Scene *scene,
                                     Lightmass::FStaticMeshStaticLightingMesh &mesh_instance);
static void swarm_read_light(Scene *scene, Lightmass::FLight &light);
static void bake_lightmass_scene(Scene *scene, const char *filepath);
#pragma optimize("",off)
void import_lightmass_scene(Scene *scene, const char *filepath)
{
  auto base_dir = path_dirname(filepath);
  auto file_name = path_filename(filepath);
  auto wbase_dir = string_to_wstring(base_dir);
  auto wfile_name = string_to_wstring(file_name);
  NSwarm::FSwarmInterface* swarm_api = new swarm_importer(wbase_dir.c_str());
  FSwarm swarm(*swarm_api);
  FImporter importer(&swarm);
  FScene gscene;
  importer.ImportScene(gscene, wfile_name.c_str());

  for (auto &[guid, light] : importer.GetLights()) {
    swarm_read_light(scene, *light);
  }

  for (auto& [guid, mesh_instance]: importer.GetStaticMeshInstances()) {
    swarm_read_mesh_instance(scene, *mesh_instance);
  }
  /* add mesh */
  //Mesh *mesh = swarm_add_mesh(scene, state.tfm, state.object);
  // array<Node *> used_shaders = mesh->get_used_shaders();
  // used_shaders.push_back_slow(state.shader);
  // mesh->set_used_shaders(used_shaders);
  
  // Add baking step
  bake_lightmass_scene(scene, filepath);
}
#pragma optimize("", on)
void swarm_read_mesh_instance(Scene *scene,
                              Lightmass::FStaticMeshStaticLightingMesh &mesh_instance)
{
  auto lmesh = mesh_instance.GetInstanceableStaticMesh();
  FStaticLightingTextureMapping *mapping = nullptr;
  if (lmesh->Mapping && lmesh->Mapping->GetTextureMapping()) {
    mapping = lmesh->Mapping->GetTextureMapping();
  }

  Object *object = scene->create_node<Object>();
  Transform tfm = make_transform(lmesh->LocalToWorld);
  Mesh *mesh = scene->create_node<Mesh>();
  Attribute *uvmain = mesh->attributes.add(ATTR_STD_UV);
  // Attribute *uvlightmap = mesh->attributes.add(ATTR_STD_GENERATED);
  Attribute *normal = mesh->attributes.add(ATTR_STD_VERTEX_NORMAL);
  Attribute *tangent = mesh->attributes.add(ATTR_STD_UV_TANGENT);
  Attribute *tangent_sign = mesh->attributes.add(ATTR_STD_UV_TANGENT_SIGN);
  mesh->resize_mesh(lmesh->NumVertices, lmesh->NumTriangles);

  auto collect_materials = [lmesh, mesh, uvmain, normal, tangent, tangent_sign, mapping]() {
    auto &verts = mesh->get_verts();
    auto uvmain_data = uvmain->data_float2();
    auto normal_data = normal->data_float3();
    auto tangent_data = tangent->data_float3();
    auto sign_data = tangent_sign->data_float();
    for (int32 TriangleIndex = 0; TriangleIndex < lmesh->NumTriangles; TriangleIndex++) {
      int32 I0 = 0, I1 = 0, I2 = 0;
      Lightmass::FStaticLightingVertexData V0, V1, V2;
      int32 ElementIndex;
      lmesh->GetInstanceableStaticMesh()->GetNonTransformedTriangleIndices(
          TriangleIndex, I0, I1, I2);
      lmesh->GetInstanceableStaticMesh()->GetNonTransformedTriangle(
          TriangleIndex, V0, V1, V2, ElementIndex);
      auto &mat_elem = lmesh->GetInstanceableStaticMesh()->GetElement(ElementIndex);
      verts[I0] = ccl::make_float3(V0.WorldPosition);
      verts[I1] = ccl::make_float3(V1.WorldPosition);
      verts[I2] = ccl::make_float3(V2.WorldPosition);

      uvmain_data[I0] = V0.TextureCoordinates[lmesh->TextureCoordinateIndex];
      uvmain_data[I1] = V1.TextureCoordinates[lmesh->TextureCoordinateIndex];
      uvmain_data[I2] = V2.TextureCoordinates[lmesh->TextureCoordinateIndex];

      // Convert normal and tangent data for each vertex
      normal_data[I0] = make_float3(V0.WorldTangentZ);
      normal_data[I1] = make_float3(V1.WorldTangentZ);
      normal_data[I2] = make_float3(V2.WorldTangentZ);

      // Convert tangent data
      tangent_data[I0] = make_float3(V0.WorldTangentX);
      tangent_data[I1] = make_float3(V1.WorldTangentX);
      tangent_data[I2] = make_float3(V2.WorldTangentX);

      // Calculate tangent sign (bitangent = sign * cross(normal, tangent))
      sign_data[I0] = (dot(cross(make_float3(V0.WorldTangentZ), make_float3(V0.WorldTangentX)),
                           make_float3(V0.WorldTangentY)) < 0.0f) ?
                          -1.0f :
                          1.0f;
      sign_data[I1] = (dot(cross(make_float3(V1.WorldTangentZ), make_float3(V1.WorldTangentX)),
                           make_float3(V1.WorldTangentY)) < 0.0f) ?
                          -1.0f :
                          1.0f;
      sign_data[I2] = (dot(cross(make_float3(V2.WorldTangentZ), make_float3(V2.WorldTangentX)),
                           make_float3(V2.WorldTangentY)) < 0.0f) ?
                          -1.0f :
                          1.0f;
      if (mapping) {
        // uvmain_data[I0] = V0.TextureCoordinates[mapping->LightmapTextureCoordinateIndex];
        //   uvmain_data[I1] = V1.TextureCoordinates[mapping->LightmapTextureCoordinateIndex];
        //   uvmain_data[I2] = V2.TextureCoordinates[mapping->LightmapTextureCoordinateIndex];
      }
    }
  };

  collect_materials();

  for (int32 TriangleIndex = 0; TriangleIndex < lmesh->NumTriangles; TriangleIndex++) {
    int32 I0 = 0, I1 = 0, I2 = 0;
    Lightmass::FStaticLightingVertexData V0, V1, V2;
    int32 ElementIndex;
    lmesh->GetInstanceableStaticMesh()->GetNonTransformedTriangleIndices(
        TriangleIndex, I0, I1, I2);
    lmesh->GetInstanceableStaticMesh()->GetNonTransformedTriangle(
        TriangleIndex, V0, V1, V2, ElementIndex);

    // mesh->add_triangle(v0, v1, v2, shader, smooth);
    auto &mat_elem = lmesh->GetInstanceableStaticMesh()->GetElement(ElementIndex);
    mat_elem.Material->Guid;
    mat_elem.Material->BlendMode;
    mat_elem.Material->DiffuseBoost;
    mat_elem.Material->DiffuseSize;  // Resolution
    mat_elem.Material->bTwoSided;
    mat_elem.Material->bCastShadowAsMasked;
    mat_elem.Material->bSurfaceDomain;
    mat_elem.Material->EmissiveBoost;
    mat_elem.Material->OpacityMaskClipValue;

    mat_elem.Material->Diffuse();
    mat_elem.Material->Emissive();
    mat_elem.Material->Transmission();
    mat_elem.Material->Normal();

    Shader *shader = scene->create_node<Shader>();

    unique_ptr<ShaderGraph> graph = make_unique<ShaderGraph>();
#ifdef WITH_OSL
    ShaderManager *manager = scene->shader_manager.get();
    string filepath;  // todo:
    ShaderNode *snode = OSLShaderManager::osl_node(graph.get(), manager, filepath, "");
#endif
    if (mat_elem.Material->Diffuse().GetMemorySize() > 0) {
      auto tex_diffuse = graph->create_node<ccl::LightmassTexture2DNode>();
      tex_diffuse->texture = &mat_elem.Material->Diffuse();
    }
    if (mat_elem.Material->Emissive().GetMemorySize() > 0) {
      auto tex_emissive = graph->create_node<ccl::LightmassTexture2DNode>();
      tex_emissive->texture = &mat_elem.Material->Emissive();
    }
    if (mat_elem.Material->Transmission().GetMemorySize() > 0) {
      auto tex_transmission = graph->create_node<ccl::LightmassTexture2DNode>();
      tex_transmission->texture = &mat_elem.Material->Transmission();  // nullable
    }
    if (mat_elem.Material->Normal().GetMemorySize() > 0) {
      auto tex_normal = graph->create_node<ccl::LightmassTexture2DNode>();
      tex_normal->texture = &mat_elem.Material->Normal();  // nullable
    }

    DiffuseBsdfNode *bsdf = graph->create_node<DiffuseBsdfNode>();
    TransparentBsdfNode *transparent = graph->create_node<TransparentBsdfNode>();
    MixClosureNode *mix = graph->create_node<MixClosureNode>();
    EmissionNode *emission = graph->create_node<EmissionNode>();
    NormalMapNode *normal_map = graph->create_node<NormalMapNode>();
    MixClosureNode *emission_mix = graph->create_node<MixClosureNode>();

    if (mat_elem.Material->Diffuse().GetMemorySize() > 0) {
      auto tex_diffuse = graph->create_node<LightmassTexture2DNode>();
      tex_diffuse->texture = &mat_elem.Material->Diffuse();
      graph->connect(tex_diffuse->output("Color"), bsdf->input("Color"));
    }
    else {
      bsdf->set_color(make_float3(0.8f, 0.8f, 0.8f));
    }

    if (mat_elem.Material->Emissive().GetMemorySize() > 0) {
      auto tex_emissive = graph->create_node<LightmassTexture2DNode>();
      tex_emissive->texture = &mat_elem.Material->Emissive();
      graph->connect(tex_emissive->output("Color"), emission->input("Color"));
      emission->set_strength(mat_elem.Material->EmissiveBoost);
    }

    if (mat_elem.Material->Transmission().GetMemorySize() > 0) {
      auto tex_transmission = graph->create_node<LightmassTexture2DNode>();
      tex_transmission->texture = &mat_elem.Material->Transmission();
      graph->connect(tex_transmission->output("Color"), mix->input("Fac"));
      graph->connect(bsdf->output("BSDF"), mix->input(0));
      graph->connect(transparent->output("BSDF"), mix->input("Closure1"));
    }

    if (mat_elem.Material->Normal().GetMemorySize() > 0) {
      auto tex_normal = graph->create_node<LightmassTexture2DNode>();
      tex_normal->texture = &mat_elem.Material->Normal();
      graph->connect(tex_normal->output("Color"), normal_map->input("Color"));
      graph->connect(normal_map->output("Normal"), bsdf->input("Normal"));
      if (mat_elem.Material->Transmission().GetMemorySize() > 0) {
        graph->connect(normal_map->output("Normal"), transparent->input("Normal"));
      }
    }

    ShaderNode *output = graph->output();
    if (mat_elem.Material->Emissive().GetMemorySize() > 0) {
      graph->connect(emission->output("Emission"), emission_mix->input("Closure1"));
      if (mat_elem.Material->Transmission().GetMemorySize() > 0) {
        graph->connect(mix->output("Closure"), emission_mix->input("Closure0"));
      }
      else {
        graph->connect(bsdf->output("BSDF"), emission_mix->input("Closure0"));
      }
      emission_mix->set_fac(1.0f);
      graph->connect(emission_mix->output("Closure"), output->input("Surface"));
    }
    else if (mat_elem.Material->Transmission().GetMemorySize() > 0) {
      graph->connect(mix->output("Closure"), output->input("Surface"));
    }
    else {
      graph->connect(bsdf->output("BSDF"), output->input("Surface"));
    }

    shader->set_graph(move(graph));
    shader->tag_update(scene);
  }

  object->set_geometry(mesh);
  object->set_tfm(tfm);
  // mesh->set_used_shaders(...);
}

static Mesh *swarm_add_mesh(Scene *scene, const Transform &tfm, Object *object)
{
  if (object && object->get_geometry()->is_mesh()) {
    /* Use existing object and mesh */
    object->set_tfm(tfm);
    Geometry *geometry = object->get_geometry();
    return static_cast<Mesh *>(geometry);
  }

  /* Create mesh */
  Mesh *mesh = scene->create_node<Mesh>();

  /* Create object. */
  object = scene->create_node<Object>();
  object->set_geometry(mesh);
  object->set_tfm(tfm);

  return mesh;
}

// @see https://github.com/blender/blender/blob/main/intern/cycles/blender/light.cpp
void swarm_read_light(Scene* scene, Lightmass::FLight& light)
{
  Light* clight = scene->create_node<Light>();

  Transform tfm = transform_identity();
  light.GetDirectionalLight();
  light.GetSpotLight();
  light.GetPointLight();  // todo: Rect light
  light.GetSkyLight();
  light.IndirectLightingSaturation;
  light.IndirectLightingScale;
  // directional, point, spot, rect, area??, skylight
  
  if (auto dir_light = light.GetDirectionalLight()) {
    clight->set_light_type(LIGHT_DISTANT);
    clight->set_angle(dir_light->LightSourceAngle); // degree or radian?
  }
  else if (auto spot_light = light.GetSpotLight()) {
    clight->set_light_type(LIGHT_SPOT);
    float3 dir = make_float3(
        spot_light->Direction.x, spot_light->Direction.y, spot_light->Direction.z);
    //tfm = transform_look_at(make_float3(light.Position.x, light.Position.y, light.Position.z),
    //                        dir,
    //                        make_float3(0, 0, 1));
    spot_light->FalloffExponent;
    spot_light->LightTangent;
    //clight->set_is_sphere(!b_spot_light.use_soft_falloff());
    clight->set_size(spot_light->Radius);
    clight->set_spot_angle(spot_light->OuterConeAngle);
    clight->set_spot_smooth(1.0f - (spot_light->OuterConeAngle - spot_light->InnerConeAngle) /
                                       spot_light->OuterConeAngle);
  }
  else if (auto point_light = light.GetPointLight()) {
    clight->set_light_type(LIGHT_POINT);
    tfm.z.w = light.Position.x;
    tfm.y.w = light.Position.y;
    tfm.x.w = light.Position.z;
    point_light->FalloffExponent;
    point_light->LightTangent;
    clight->set_size(point_light->Radius);

    //clight->set_sizeu(b_area_light.size());
    //clight->set_spread(b_area_light.spread());
    // todo: rect light
    //case BL::AreaLight::shape_RECTANGLE:
    //  light->set_sizev(b_area_light.size_y());
    //  light->set_ellipse(false);
  }
  else if (auto sky_light = light.GetSkyLight()) {
    clight->set_light_type(LIGHT_BACKGROUND);
    sky_light->RadianceEnvironmentMapDataSize;
  }
  //clight->set_strength(light.Power());
  clight->set_cast_shadow(true);

  // Set common light properties
  Shader *shader = scene->create_node<Shader>();
  unique_ptr<ShaderGraph> graph = make_unique<ShaderGraph>();
  
  EmissionNode *emission = graph->create_node<EmissionNode>();
  emission->set_color(make_float3(light.Color.R, light.Color.G, light.Color.B));
  emission->set_strength(light.Brightness);
  
  ShaderNode *output = graph->output();
  graph->connect(emission->output("Emission"), output->input("Surface"));
  
  shader->set_graph(move(graph));
  shader->tag_update(scene);
  
  //clight->set_shader(shader);
  clight->tag_update(scene);
}

// @see https://github.com/blender/blender/blob/main/intern/cycles/blender/session.cpp
void bake_lightmass_scene(Scene *scene, const char *filepath)
{
  // Configure integrator for baking
  Integrator *integrator = scene->integrator;
  integrator->set_use_direct_light(true);
  integrator->set_use_indirect_light(true);
  //integrator->set_denoise_use_gpu(true);

  const bool was_denoiser_enabled = integrator->get_use_denoise();
  //integrator->set_diffuse_samples(4);
  //integrator->set_glossy_samples(4);
  //integrator->set_transmission_samples(4);
  //integrator->set_ao_samples(4);
  //integrator->set_mesh_light_samples(4);
  scene->bake_manager->set_baking(scene, true);
  scene->bake_manager->tag_update();

  // Process each object
  for (Object *object: scene->objects) {
    Mesh *mesh = static_cast<Mesh *>(object->get_geometry());
    if (!mesh)
      continue;

    // Get or create lightmap UV coordinates
    Attribute *lightmap_uvs = mesh->attributes.find(ATTR_STD_GENERATED);
    if (!lightmap_uvs) {
      lightmap_uvs = mesh->attributes.add(ATTR_STD_GENERATED);
      // TODO: Generate lightmap UVs if needed
    }

    // TODO: Convert and store result back to Lightmass format
    // This will depend on your specific Lightmass data structures
  }

  // Reset integrator settings if needed
  //integrator->tag_update();
}
CCL_NAMESPACE_END

