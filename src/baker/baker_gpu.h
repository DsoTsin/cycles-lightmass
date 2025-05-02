#pragma once
#include "baker_cpu.h"

CCL_NAMESPACE_BEGIN

class baker_gpu : public baker {
 public:
  virtual ~baker_gpu() override;
};

CCL_NAMESPACE_END
