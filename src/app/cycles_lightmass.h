/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "lightmass/swarm_impl.h"

CCL_NAMESPACE_BEGIN

class Scene;

void import_lightmass_scene(Scene *scene, const char *filepath);

CCL_NAMESPACE_END
