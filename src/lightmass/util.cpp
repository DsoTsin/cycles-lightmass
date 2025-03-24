/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <cstdio>
#include <cassert>
#include "util.h"

CCL_NAMESPACE_BEGIN
void check_impl(bool result, const char *expr, int line) {
  assert(result);
}
void check_fmt_impl(bool result, int line, const wchar_t *fmt, ...) {
  assert(result);
}
void check_fmt_impl(bool result, int line, const char *fmt, ...) {
  assert(result);
}

aux_buffer::aux_buffer() : _ptr(nullptr), _size(0) {}
aux_buffer::~aux_buffer()
{
  release();
}

void aux_buffer::append(uint8_t *data, size_t size)
{
  if (_ptr) {
    auto new_ptr = (uint8_t *)util_aligned_malloc(_size + size, 16);
    memcpy(new_ptr, _ptr, _size);
    memcpy(new_ptr + _size, data, size);
    util_aligned_free(_ptr, _size);
    _ptr = new_ptr;
    _size = size + _size;
  }
  else {
    _ptr = (uint8_t *)util_aligned_malloc(size, 16);
    _size = size;
    memcpy(_ptr, data, size);
  }
}

void aux_buffer::slice(size_t start)
{
  size_t new_size = _size - start;
  if (start < _size) {
    auto new_ptr = (uint8_t *)util_aligned_malloc(new_size, 16);
    memcpy(new_ptr, _ptr + start, new_size);
    util_aligned_free(_ptr, _size);
    _ptr = new_ptr;
    _size = new_size;
  }
  else if (_size == start) {
    util_aligned_free(_ptr, _size);
    _ptr = nullptr;
    _size = 0;
  }
}

void aux_buffer::release()
{
  if (_ptr) {
    util_aligned_free(_ptr, _size);
    _ptr = 0;
    _size = 0;
  }
}

Transform make_transform(const Lightmass::FMatrix &m) {
  Transform tfm;
  tfm.x = make_float4(m[0][0],
                      m[1][0],
                      m[2][0],
                      m[3][0]);
  tfm.y = make_float4(m[0][1],
                      m[1][1],
                      m[2][1],
                      m[3][1]);
  tfm.z = make_float4(m[0][2],
                      m[1][2],
                      m[2][2],
                      m[3][2]);
  return tfm;
}
CCL_NAMESPACE_END
