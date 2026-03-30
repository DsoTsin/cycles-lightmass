/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_float2.h"
#include "util/types_float3.h"
#include "util/types_float4.h"
#include "util/types_int3.h"
#include "util/types_int2.h"
#include "util/math_base.h"
#include "util/math_float3.h"
#include "util/math_float4.h"
#include "util/hash.h"
#include "util/color.h"
#include "util/vector.h"
#include "util/map.h"
#include "util/string.h"
#include "util/math.h"
#include "util/transform.h"
#include "util/aligned_malloc.h"

CCL_NAMESPACE_BEGIN
void check_impl(bool result, const char *expr, int line);
void check_fmt_impl(bool result, int line, const wchar_t *fmt, ...);
void check_fmt_impl(bool result, int line, const char *fmt, ...);

struct aux_buffer {
  aux_buffer();
  ~aux_buffer();

  uint8_t *data() const
  {
    return _ptr;
  }
  size_t size() const
  {
    return _size;
  }

  void append(uint8_t *data, size_t size);
  void slice(size_t start);

  void release();

 private:
  uint8_t *_ptr;
  size_t _size;
};
CCL_NAMESPACE_END

#define FORCEINLINE __forceinline
#define check(x) ccl::check_impl(x, #x, __LINE__)
#define checkf(x, y, ...) ccl::check_fmt_impl(x, __LINE__, y, __VA_ARGS__)
#define verify(x) ccl::check_impl(x, #x, __LINE__)
#define checkSlow(x) ccl::check_impl(x, #x, __LINE__)

#ifndef TEXT
#  define TEXT(x) L##x
#endif

#ifndef __UNREAL_NON_OFFICIAL__
#define __UNREAL_NON_OFFICIAL__ 1
#endif

namespace Lightmass {
#define DELTA			(0.00001f)
#define WORLD_MAX					2097152.0				/* Maximum size of the world */
#define HALF_WORLD_MAX				(WORLD_MAX * 0.5)		/* Half the maximum size of the world */
    
#undef  PI
#define PI 					(3.1415926535897932f)	/* Extra digits if needed: 3.1415926535897932384626433832795f */
#define SMALL_NUMBER		(1.e-8f)
#define KINDA_SMALL_NUMBER	(1.e-4f)
#define INV_PI			(0.31830988618f)
#define HALF_PI			(1.57079632679f)

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef wchar_t TCHAR;

typedef ccl::float2 FVector2D;
typedef ccl::packed_float3 FVector;
typedef ccl::float4 FVector4;
//typedef ccl::float4 FLinearColor;
typedef ccl::int2 FIntPoint;
struct FIntVector
{
  int32 X, Y, Z;
};
typedef ccl::float4 FMatrix[4];

struct FBox {
  uint8 IsValid;
  FVector Min;
  FVector Max;
};

struct FLinearColor {
  float R, G, B, A;

  FLinearColor() : R(0), G(0), B(0), A(0) {}

  FLinearColor(float r, float g, float b, float a = 0) : R(r), G(g), B(b), A(a) {}
  FLinearColor(ccl::float4 v) : R(v.x), G(v.y), B(v.z), A(v.w) {}

  operator ccl::float4() const
  {
    return ccl::float4{R, G, B, A};
  }
  operator ccl::float3() const
  {
    return ccl::float3{R, G, B};
  }
  friend FORCEINLINE FLinearColor operator*(const FLinearColor& b, float a)
  {
    return FLinearColor(b.R * a, b.G * a, b.B * a, b.A * a);
  }
  friend FORCEINLINE FLinearColor operator/(const FLinearColor &b, float a)
  {
    return FLinearColor(b.R / a, b.G / a, b.B / a, b.A / a);
  }
};



struct FBoxSphereBounds {
  /** Holds the origin of the bounding box and sphere. */
  FVector Origin;

  /** Holds the extent of the bounding box. */
  FVector BoxExtent;

  /** Holds the radius of the bounding sphere. */
  float SphereRadius;
};

struct FGuid {
  uint32 A, B, C, D;

  bool operator==(const FGuid &other) const
  {
    return A == other.A && B == other.B && C == other.C && D == other.D;
  }
};

struct FSHVector3 {
  FVector4 SH[3];
};

struct FSHVectorRGB3 {
  FSHVector3 R, G, B;
};

class FSHAHash {
 public:
  uint8 Hash[20];

  bool operator==(const FSHAHash& h) const {
    return memcmp(Hash, h.Hash, 20) == 0;
  }
};

enum { MAX_TEXCOORDS = 4 };

class FFloat16 {
 public:
  union {
    struct {
//#if PLATFORM_LITTLE_ENDIAN
      uint16 Mantissa : 10;
      uint16 Exponent : 5;
      uint16 Sign : 1;
//#else
//      uint16 Sign : 1;
//      uint16 Exponent : 5;
//      uint16 Mantissa : 10;
//#endif
    } Components;

    uint16 Encoded;
  };
};

class FFloat16Color {
 public:
  FFloat16 R;
  FFloat16 G;
  FFloat16 B;
  FFloat16 A;
};

struct FColor {
 public:
  union {
    struct {
      uint8 B, G, R, A;
    };
    uint32 AlignmentDummy;
  };
};

class FSphere {
 public:
  FSphere() {}
  FSphere(const FVector& center, float radius)
  : Center(center), W(radius) 
  {}

  /** The sphere's center point. */
  FVector Center;

  /** The sphere's radius. */
  float W;
};

struct FPlane {
  FVector4 Data;
};

FORCEINLINE std::wstring CreateChannelName(const FGuid &guid, int32 ver, const TCHAR *ext)
{
  static thread_local wchar_t buffer[256];
  swprintf(buffer,
           sizeof(buffer),
           L"v%d.%08X%08X%08X%08X.%s",
           ver,
           guid.A,
           guid.B,
           guid.C,
           guid.D,
           ext);
  return buffer;
}

/** Returns Char value of Nibble */
inline TCHAR NibbleToTChar(uint8 Num)
{
  if (Num > 9) {
    return TEXT('A') + TCHAR(Num - 10);
  }
  return TEXT('0') + TCHAR(Num);
}

/**
 * Convert a byte to hex
 * @param In byte value to convert
 * @param Result out hex value output
 */
inline void ByteToHex(uint8 In, std::wstring &Result)
{
  Result += NibbleToTChar(In >> 4);
  Result += NibbleToTChar(In & 15);
}

/**
 * Convert an array of bytes to hex
 * @param In byte array values to convert
 * @param Count number of bytes to convert
 * @return Hex value in string.
 */
inline std::wstring BytesToHex(const uint8 *In, int32 Count)
{
  std::wstring Result;

  while (Count) {
    ByteToHex(*In++, Result);
    Count--;
  }
  Result.push_back(0);
  return Result;
}

FORCEINLINE std::wstring CreateChannelName(const FSHAHash &hash, const int32 ver, const TCHAR *ext)
{
  static thread_local wchar_t buffer[256];
  auto hex = BytesToHex((const uint8 *)hash.Hash, sizeof(hash));
  swprintf(buffer, sizeof(buffer), L"v%d.%s.%s", ver, hex.c_str(), ext);
  return buffer;
}

struct FLinearColorUtils {
  /** Converts a linear space RGB color to linear space XYZ. */
    FORCEINLINE static FLinearColor LinearRGBToXYZ(const FLinearColor& InColor) {
      // RGB to XYZ linear transformation used by sRGB
      float x = ccl::dot(ccl::make_float3(0.4124564f, 0.3575761f, 0.1804375f),
                         ccl::make_float3(InColor.R, InColor.G, InColor.B));
      float y = ccl::dot(ccl::make_float3(0.2126729f, 0.7151522f, 0.0721750f),
                         ccl::make_float3(InColor.R, InColor.G, InColor.B));
      float z = ccl::dot(ccl::make_float3(0.0193339f, 0.1191920f, 0.9503041f),
                         ccl::make_float3(InColor.R, InColor.G, InColor.B));
      return FLinearColor{x, y, z};
  }

  /** Converts a linear space XYZ color to linear space RGB. */
  static FLinearColor XYZToLinearRGB(const FLinearColor &InColor);

  /** Converts an XYZ color to xyzY, where xy and z are chrominance measures and Y is the
   * brightness. */
  static FLinearColor XYZToxyzY(const FLinearColor &InColor);

  /** Converts an xyzY color to XYZ. */
  static FLinearColor xyzYToXYZ(const FLinearColor &InColor);

  /** Converts a linear space RGB color to an HSV color */
  static FLinearColor LinearRGBToHSV(const FLinearColor &InColor);

  /** Converts an HSV color to a linear space RGB color */
  static FLinearColor HSVToLinearRGB(const FLinearColor &InColor);

  /**
   * Returns a color with adjusted saturation levels, with valid input from 0.0 to 2.0
   * 0.0 produces a fully desaturated color
   * 1.0 produces no change to the saturation
   * 2.0 produces a fully saturated color
   *
   * @param	SaturationFactor	Saturation factor in range [0..2]
   * @return	Desaturated color
   */
  FORCEINLINE static FLinearColor AdjustSaturation(const FLinearColor &InColor,
                                                  float SaturationFactor)
  {

    // Convert to HSV space for the saturation adjustment
    ccl::float3 HSVColor = ccl::rgb_to_hsv(ccl::make_float3(InColor.R, InColor.G, InColor.B));

    // Clamp the range to what's expected
    SaturationFactor = ccl::clamp(SaturationFactor, 0.0f, 2.0f);

    if (SaturationFactor < 1.0f) {
      HSVColor.y = ccl::mix(0.0f, HSVColor.y, SaturationFactor);
    }
    else {
      HSVColor.z = ccl::mix(HSVColor.y, 1.0f, SaturationFactor - 1.0f);
    }

    // Convert back to linear RGB
    return ccl::make_float4(ccl::hsv_to_rgb(HSVColor));
  }
};
}



namespace std {
template<> struct hash<Lightmass::FGuid> {
  size_t operator()(const Lightmass::FGuid &guid) const
  {
    return ccl::hash_uint4(guid.A, guid.B, guid.C, guid.D);
  }
};
template<> struct hash<Lightmass::FSHAHash> {
  size_t operator()(const Lightmass::FSHAHash &h) const
  {
    return ccl::hash_uint2(ccl::hash_uint4(*(uint32_t *)h.Hash,
                                           *((uint32_t *)h.Hash + 1),
                                           *((uint32_t *)h.Hash + 2),
                                           *((uint32_t *)h.Hash + 3)),
                           *((uint32_t *)h.Hash + 4));
  }
};
}  // namespace std


CCL_NAMESPACE_BEGIN

Transform make_transform(const Lightmass::FMatrix &);

CCL_NAMESPACE_END
