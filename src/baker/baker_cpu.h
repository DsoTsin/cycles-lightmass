#pragma once

CCL_NAMESPACE_BEGIN

class baker {
 public:
  virtual ~baker() {}
};

class baker_cpu: public baker {
 public:
  virtual ~baker_cpu() override;
};

CCL_NAMESPACE_END
