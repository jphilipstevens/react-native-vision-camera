//
// Created by Marc on 19/06/2021.
//

#pragma once

#include <jsi/jsi.h>
#include <jni.h>
#include <fbjni/fbjni.h>

#include "JImageProxy.h"

namespace vision {

using namespace facebook;

class JSI_EXPORT JImageProxyHostObject : public jsi::HostObject {
public:
  explicit JImageProxyHostObject(jni::local_ref<jobject> boxedImage) : frame(boxedImage) {

  ~JImageProxyHostObject();

public:
  jsi::Value get(jsi::Runtime &, const jsi::PropNameID &name) override;

  std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &rt) override;

public:
  jni::local_ref<jobject> frame;

private:
  static auto constexpr TAG = "VisionCamera";
};

} // namespace vision