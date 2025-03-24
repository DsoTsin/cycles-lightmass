/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

class Scene;

void xml_read_file(Scene *scene, const char *filepath);

#ifndef M_PI
#  define M_PI 3.14159265358979323846264338327950288
#endif
/* macros for importing */
#define RAD2DEGF(_rad) ((_rad) * (float)(180.0 / M_PI))
#define DEG2RADF(_deg) ((_deg) * (float)(M_PI / 180.0))

CCL_NAMESPACE_END
