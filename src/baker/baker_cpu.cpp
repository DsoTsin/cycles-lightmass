#include "baker_cpu.h"

#include "device/device.h"
#include "graph/node.h"
#include "scene/bake.h"
//#include "scene/buffers.h"
#include "scene/camera.h"
#include "scene/geometry.h"
//#include "scene/graph.h"
#include "scene/integrator.h"
#include "scene/light.h"
#include "scene/mesh.h"
//#include "scene/nodes.h"
#include "scene/object.h"
#include "scene/scene.h"
#include "session/session.h"
#include "scene/shader.h"
#include "scene/volume.h"
#include "util/image.h"
#include "util/path.h"
#include "util/math.h"

#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "OpenImageIO\imagebuf.h"
#include "OpenImageIO\imagebufalgo.h"
#include "OpenImageIO\imageio.h"

CCL_NAMESPACE_BEGIN

#define DEG2RADF(_deg) ((_deg) * (float)(M_PI / 180.0))

typedef struct BakePixel {
  int primitive_id, object_id;
  int seed;
  float uv[2];
  float du_dx, du_dy;
  float dv_dx, dv_dy;
} BakePixel;

typedef struct BakeImage {
  struct Image *image;
  int width;
  int height;
  size_t offset;
} BakeImage;

/* span fill in method, is also used to localize data for zbuffering */
typedef struct ZSpan {
  int rectx, recty; /* range for clipping */

  int miny1, maxy1, miny2, maxy2;             /* actual filled in range */
  const float *minp1, *maxp1, *minp2, *maxp2; /* vertex pointers detect min/max range in */
  float *span1, *span2;
} ZSpan;

typedef struct BakeDataZSpan {
  BakePixel *pixel_array;
  int primitive_id;
  BakeImage *bk_image;
  ZSpan *zspan;
  float du_dx, du_dy;
  float dv_dx, dv_dy;
} BakeDataZSpan;

typedef struct MyBakeData {
  int width, height;
  OIIO::ImageBuf buffer_combined;
  OIIO::ImageBuf buffer_primitive_id;
  OIIO::ImageBuf buffer_differencial;
  OIIO::ImageBuf buffer_uv;

  MyBakeData(int width, int height)
      : width(width),
        height(height),
        buffer_combined(ImageSpec(width, height, 4, OIIO::TypeDesc(OIIO::TypeDesc::FLOAT))),
        buffer_primitive_id(ImageSpec(width, height, 4, OIIO::TypeDesc(OIIO::TypeDesc::FLOAT))),
        buffer_differencial(ImageSpec(width, height, 4, OIIO::TypeDesc(OIIO::TypeDesc::FLOAT))),
        buffer_uv(ImageSpec(width, height, 2, OIIO::TypeDesc(OIIO::TypeDesc::FLOAT)))
  {
  }

  void set(int x,
           int y,
           int seed,
           int primitive_id,
           float2 uv,
           float du_dx,
           float du_dy,
           float dv_dx,
           float dv_dy)
  {
    float prim[4] = {__int_as_float(seed), __int_as_float(primitive_id), uv.x, uv.y};
    buffer_primitive_id.setpixel(x, y, prim);
    float diff[4] = {du_dx, du_dy, dv_dx, dv_dy};
    buffer_differencial.setpixel(x, y, diff);
    buffer_uv.setpixel(x, y, &uv.x);
  }

} MyBakeData;


baker_cpu::~baker_cpu() {
}


static int bake_pass_filter_get(const int pass_filter)
{
  int flag = 0;

  //flag |= BAKE_FILTER_DIRECT;
  //flag |= BAKE_FILTER_INDIRECT;
  //flag |= BAKE_FILTER_COLOR;
  //flag |= BAKE_FILTER_DIFFUSE;
  //flag |= BAKE_FILTER_GLOSSY;
  //flag |= BAKE_FILTER_TRANSMISSION;
  //flag |= BAKE_FILTER_EMISSION;
  //flag |= BAKE_FILTER_AO;

  return flag;
}

inline int max_ii(int a, int b)
{
  return (b < a) ? a : b;
}
inline float min_ff(float a, float b)
{
  return (a < b) ? a : b;
}
inline int min_ii(int a, int b)
{
  return (a < b) ? a : b;
}
inline float max_ff(float a, float b)
{
  return (a > b) ? a : b;
}

static void bake_differentials(BakeDataZSpan *bd,
                               const float *uv1,
                               const float *uv2,
                               const float *uv3)
{
  float A;

  /* assumes dPdu = P1 - P3 and dPdv = P2 - P3 */
  A = (uv2[0] - uv1[0]) * (uv3[1] - uv1[1]) - (uv3[0] - uv1[0]) * (uv2[1] - uv1[1]);

  if (fabsf(A) > FLT_EPSILON) {
    A = 0.5f / A;

    bd->du_dx = (uv2[1] - uv3[1]) * A;
    bd->dv_dx = (uv3[1] - uv1[1]) * A;

    bd->du_dy = (uv3[0] - uv2[0]) * A;
    bd->dv_dy = (uv1[0] - uv3[0]) * A;
  }
  else {
    bd->du_dx = bd->du_dy = 0.0f;
    bd->dv_dx = bd->dv_dy = 0.0f;
  }
}

static void zbuf_add_to_span(ZSpan *zspan, const float v1[2], const float v2[2])
{
  const float *minv, *maxv;
  float *span;
  float xx1, dx0, xs0;
  int y, my0, my2;

  if (v1[1] < v2[1]) {
    minv = v1;
    maxv = v2;
  }
  else {
    minv = v2;
    maxv = v1;
  }

  my0 = (int)std::ceil(minv[1]);
  my2 = (int)std::floor(maxv[1]);

  if (my2 < 0 || my0 >= zspan->recty) {
    return;
  }

  /* clip top */
  if (my2 >= zspan->recty) {
    my2 = zspan->recty - 1;
  }
  /* clip bottom */
  if (my0 < 0) {
    my0 = 0;
  }

  if (my0 > my2) {
    return;
  }
  /* if (my0>my2) should still fill in, that way we get spans that skip nicely */

  xx1 = maxv[1] - minv[1];
  if (xx1 > FLT_EPSILON) {
    dx0 = (minv[0] - maxv[0]) / xx1;
    xs0 = dx0 * (minv[1] - my2) + minv[0];
  }
  else {
    dx0 = 0.0f;
    xs0 = min_ff(minv[0], maxv[0]);
  }

  /* empty span */
  if (zspan->maxp1 == NULL) {
    span = zspan->span1;
  }
  else { /* does it complete left span? */
    if (maxv == zspan->minp1 || minv == zspan->maxp1) {
      span = zspan->span1;
    }
    else {
      span = zspan->span2;
    }
  }

  if (span == zspan->span1) {
    //      printf("left span my0 %d my2 %d\n", my0, my2);
    if (zspan->minp1 == NULL || zspan->minp1[1] > minv[1]) {
      zspan->minp1 = minv;
    }
    if (zspan->maxp1 == NULL || zspan->maxp1[1] < maxv[1]) {
      zspan->maxp1 = maxv;
    }
    if (my0 < zspan->miny1) {
      zspan->miny1 = my0;
    }
    if (my2 > zspan->maxy1) {
      zspan->maxy1 = my2;
    }
  }
  else {
    //      printf("right span my0 %d my2 %d\n", my0, my2);
    if (zspan->minp2 == NULL || zspan->minp2[1] > minv[1]) {
      zspan->minp2 = minv;
    }
    if (zspan->maxp2 == NULL || zspan->maxp2[1] < maxv[1]) {
      zspan->maxp2 = maxv;
    }
    if (my0 < zspan->miny2) {
      zspan->miny2 = my0;
    }
    if (my2 > zspan->maxy2) {
      zspan->maxy2 = my2;
    }
  }

  for (y = my2; y >= my0; y--, xs0 += dx0) {
    /* xs0 is the xcoord! */
    span[y] = xs0;
  }
}

/* reset range for clipping */
static void zbuf_init_span(ZSpan *zspan)
{
  zspan->miny1 = zspan->miny2 = zspan->recty + 1;
  zspan->maxy1 = zspan->maxy2 = -1;
  zspan->minp1 = zspan->maxp1 = zspan->minp2 = zspan->maxp2 = NULL;
}

/* Scanconvert for strand triangles, calls func for each x, y coordinate
 * and gives UV barycentrics and z. */

void zspan_scanconvert(ZSpan *zspan,
                       BakeDataZSpan *handle,
                       float *v1,
                       float *v2,
                       float *v3,
                       void (*func)(BakeDataZSpan *, int, int, float, float))
{
  float x0, y0, x1, y1, x2, y2, z0, z1, z2;
  float u, v, uxd, uyd, vxd, vyd, uy0, vy0, xx1;
  const float *span1, *span2;
  int i, j, x, y, sn1, sn2, rectx = zspan->rectx, my0, my2;

  /* init */
  zbuf_init_span(zspan);

  /* set spans */
  zbuf_add_to_span(zspan, v1, v2);
  zbuf_add_to_span(zspan, v2, v3);
  zbuf_add_to_span(zspan, v3, v1);

  /* clipped */
  if (zspan->minp2 == NULL || zspan->maxp2 == NULL) {
    return;
  }

  my0 = max_ii(zspan->miny1, zspan->miny2);
  my2 = min_ii(zspan->maxy1, zspan->maxy2);

  //  printf("my %d %d\n", my0, my2);
  if (my2 < my0) {
    return;
  }

  /* ZBUF DX DY, in floats still */
  x1 = v1[0] - v2[0];
  x2 = v2[0] - v3[0];
  y1 = v1[1] - v2[1];
  y2 = v2[1] - v3[1];

  z1 = 1.0f; /* (u1 - u2) */
  z2 = 0.0f; /* (u2 - u3) */

  x0 = y1 * z2 - z1 * y2;
  y0 = z1 * x2 - x1 * z2;
  z0 = x1 * y2 - y1 * x2;

  if (z0 == 0.0f) {
    return;
  }

  xx1 = (x0 * v1[0] + y0 * v1[1]) / z0 + 1.0f;
  uxd = -(double)x0 / (double)z0;
  uyd = -(double)y0 / (double)z0;
  uy0 = ((double)my2) * uyd + (double)xx1;

  z1 = -1.0f; /* (v1 - v2) */
  z2 = 1.0f;  /* (v2 - v3) */

  x0 = y1 * z2 - z1 * y2;
  y0 = z1 * x2 - x1 * z2;

  xx1 = (x0 * v1[0] + y0 * v1[1]) / z0;
  vxd = -(double)x0 / (double)z0;
  vyd = -(double)y0 / (double)z0;
  vy0 = ((double)my2) * vyd + (double)xx1;

  /* correct span */
  span1 = zspan->span1 + my2;
  span2 = zspan->span2 + my2;

  for (i = 0, y = my2; y >= my0; i++, y--, span1--, span2--) {

    sn1 = (int)std::floor(min_ff(*span1, *span2));
    sn2 = (int)std::floor(max_ff(*span1, *span2));
    sn1++;

    if (sn2 >= rectx) {
      sn2 = rectx - 1;
    }
    if (sn1 < 0) {
      sn1 = 0;
    }

    u = (((double)sn1 * uxd) + uy0) - (i * uyd);
    v = (((double)sn1 * vxd) + vy0) - (i * vyd);

    for (j = 0, x = sn1; x <= sn2; j++, x++) {
      func(handle, x, y, u + (j * uxd), v + (j * vxd));
    }
  }
}

static void store_bake_pixel(BakeDataZSpan *handle, int x, int y, float u, float v)
{
  BakeDataZSpan *bd = (BakeDataZSpan *)handle;
  BakePixel *pixel;

  const int width = bd->bk_image->width;
  const size_t offset = bd->bk_image->offset;
  const int i = offset + y * width + x;

  pixel = &bd->pixel_array[i];
  pixel->seed = rand();
  pixel->primitive_id = bd->primitive_id;

  /* At this point object_id is always 0, since this function runs for the
   * low-poly mesh only. The object_id lookup indices are set afterwards. */

  pixel->uv[0] = u;
  pixel->uv[1] = v;

  pixel->du_dx = bd->du_dx;
  pixel->du_dy = bd->du_dy;
  pixel->dv_dx = bd->dv_dx;
  pixel->dv_dy = bd->dv_dy;
  pixel->object_id = 0;
}

/* each zbuffer has coordinates transformed to local rect coordinates, so we can simply clip */
void zbuf_alloc_span(ZSpan *zspan, int rectx, int recty)
{
  memset(zspan, 0, sizeof(ZSpan));

  zspan->rectx = rectx;
  zspan->recty = recty;

  zspan->span1 = (float *)malloc(recty * sizeof(float));
  zspan->span2 = (float *)malloc(recty * sizeof(float));
}

void populate_bake_data(const Mesh &mesh, size_t uv_map_index, MyBakeData *data)
{
  int image_width = data->width;
  int image_height = data->height;

  size_t num_pixels = image_width * image_height;

  /* initialize all pixel arrays so we know which ones are 'blank' */
  BakeDataZSpan bd;
  bd.bk_image = new BakeImage();
  bd.bk_image->width = image_width;
  bd.bk_image->height = image_height;
  bd.bk_image->offset = 0;
  bd.pixel_array = (BakePixel *)malloc(sizeof(BakePixel) * num_pixels);
  bd.zspan = new ZSpan();

  for (size_t i = 0; i < num_pixels; i++) {
    bd.pixel_array[i].primitive_id = -1;
    bd.pixel_array[i].object_id = 0;
  }
  zbuf_alloc_span(bd.zspan, image_width, image_height);

  Attribute *attribute = mesh.attributes.find(ATTR_STD_UV);
  if (attribute == nullptr) {
    return;
  }

  static_cast<void>(uv_map_index);
  float2 *fdata = attribute->data_float2();
  size_t triangles_count = mesh.num_triangles();

  for (size_t i = 0; i < triangles_count; i++) {
    bd.primitive_id = i;
    float vec[3][2];

    Mesh::Triangle triangle = mesh.get_triangle(i);
    for (size_t j = 0; j < 3; j++) {
      float2 uv = fdata[i * 3 + j];
      vec[j][0] = uv[0] * (float)bd.bk_image->width - (0.5f + 0.001f);
      vec[j][1] = uv[1] * (float)bd.bk_image->height - (0.5f + 0.002f);
    }

    bake_differentials(&bd, vec[0], vec[1], vec[2]);
    zspan_scanconvert(bd.zspan, &bd, vec[0], vec[1], vec[2], store_bake_pixel);
  }

  BakePixel *bp = bd.pixel_array;
  for (size_t y = 0; y < image_height; y++) {
    for (size_t x = 0; x < image_width; x++) {
      data->set(x,
                y,
                bp->seed,
                bp->primitive_id,
                make_float2(bp->uv[0], bp->uv[1]),
                bp->du_dx,
                bp->du_dy,
                bp->dv_dx,
                bp->dv_dy);
      bp++;
    }
  }

  free(bd.pixel_array);
  delete bd.bk_image;
  delete bd.zspan;
}


CCL_NAMESPACE_END
