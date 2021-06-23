//
// Created by Marc Rousavy on 11.06.21.
//

#include "FrameProcessorRuntimeManager.h"
#include <android/log.h>
#include <jni.h>

#include "RuntimeDecorator.h"
#include "AndroidErrorHandler.h"

#include "MakeJSIRuntime.h"
#include "CameraView.h"
#include "JImageProxy.h"
#include "JImageProxyHostObject.h"
#include "JSIJNIConversion.h"

namespace vision {

// type aliases
using TSelf = local_ref<HybridClass<vision::FrameProcessorRuntimeManager>::jhybriddata>;
using JSCallInvokerHolder = jni::alias_ref<facebook::react::CallInvokerHolder::javaobject>;
using AndroidScheduler = jni::alias_ref<reanimated::AndroidScheduler::javaobject>;

// JNI binding
void vision::FrameProcessorRuntimeManager::registerNatives() {
  registerHybrid({
    makeNativeMethod("initHybrid",
                     FrameProcessorRuntimeManager::initHybrid),
    makeNativeMethod("installJSIBindings",
                     FrameProcessorRuntimeManager::installJSIBindings),
    makeNativeMethod("initializeRuntime",
                     FrameProcessorRuntimeManager::initializeRuntime),
    makeNativeMethod("registerPlugin",
                     FrameProcessorRuntimeManager::registerPlugin),
  });
}

// JNI init
TSelf vision::FrameProcessorRuntimeManager::initHybrid(
    alias_ref<jhybridobject> jThis,
    jlong jsContext,
    JSCallInvokerHolder jsCallInvokerHolder,
    AndroidScheduler androidScheduler) {
  __android_log_write(ANDROID_LOG_INFO, TAG,
                      "Initializing FrameProcessorRuntimeManager...");

  // cast from JNI hybrid objects to C++ instances
  auto jsCallInvoker = jsCallInvokerHolder->cthis()->getCallInvoker();
  auto scheduler = androidScheduler->cthis()->getScheduler();
  scheduler->setJSCallInvoker(jsCallInvoker);

  return makeCxxInstance(jThis, (jsi::Runtime *) jsContext, jsCallInvoker, scheduler);
}

void vision::FrameProcessorRuntimeManager::initializeRuntime() {
  __android_log_write(ANDROID_LOG_INFO, TAG,
                      "Initializing Vision JS-Runtime...");

  // create JSI runtime and decorate it
  auto runtime = makeJSIRuntime();
  reanimated::RuntimeDecorator::decorateRuntime(*runtime, "FRAME_PROCESSOR");
  runtime->global().setProperty(*runtime, "_FRAME_PROCESSOR",
                                jsi::Value(true));

  // create REA runtime manager
  auto errorHandler = std::make_shared<reanimated::AndroidErrorHandler>(scheduler_);
  _runtimeManager = std::make_unique<reanimated::RuntimeManager>(std::move(runtime),
                                                                 errorHandler,
                                                                 scheduler_);

  __android_log_write(ANDROID_LOG_INFO, TAG,
                      "Initialized Vision JS-Runtime!");
}

CameraView* FrameProcessorRuntimeManager::findCameraViewById(int viewId) {
  static const auto func = javaPart_->getClass()->getMethod<CameraView*(jint)>("findCameraViewById");
  auto result = func(javaPart_.get(), viewId);
  return result->cthis();
}

// actual JSI installer
void FrameProcessorRuntimeManager::installJSIBindings() {
  __android_log_write(ANDROID_LOG_INFO, TAG, "Installing JSI bindings...");

  if (runtime_ == nullptr) {
    __android_log_write(ANDROID_LOG_ERROR, TAG,
                        "JS-Runtime was null, Frame Processor JSI bindings could not be installed!");
    return;
  }

  auto &jsiRuntime = *runtime_;

  auto setFrameProcessor = [this](jsi::Runtime &runtime,
                                  const jsi::Value &thisValue,
                                  const jsi::Value *arguments,
                                  size_t count) -> jsi::Value {
    __android_log_write(ANDROID_LOG_INFO, TAG,
                        "Setting new Frame Processor...");

    if (!arguments[0].isNumber()) {
      throw jsi::JSError(runtime,
                         "Camera::setFrameProcessor: First argument ('viewTag') must be a number!");
    }
    if (!arguments[1].isObject()) {
      throw jsi::JSError(runtime,
                         "Camera::setFrameProcessor: Second argument ('frameProcessor') must be a function!");
    }
    if (!_runtimeManager || !_runtimeManager->runtime) {
      throw jsi::JSError(runtime,
                         "Camera::setFrameProcessor: The RuntimeManager is not yet initialized!");
    }

    // find camera view
    auto viewTag = arguments[0].asNumber();
    auto cameraView = findCameraViewById((int) viewTag);
    __android_log_write(ANDROID_LOG_INFO, TAG, "Found CameraView!");

    // TODO: does this have to be called on the separate VisionCamera Frame Processor Thread?

    // convert jsi::Function to a ShareableValue (can be shared across runtimes)
    __android_log_write(ANDROID_LOG_INFO, TAG, "Adapting Shareable value from function (conversion to worklet)...");
    auto worklet = reanimated::ShareableValue::adapt(runtime, arguments[1],
                                                     _runtimeManager.get());
    __android_log_write(ANDROID_LOG_INFO, TAG, "Successfully created worklet!");

    // cast worklet to a jsi::Function for the new runtime
    auto &rt = *this->_runtimeManager->runtime;
    auto function = std::make_shared<jsi::Function>(worklet->getValue(rt).asObject(rt).asFunction(rt));

    // assign lambda to frame processor
    cameraView->setFrameProcessor([&rt, function](jni::local_ref<JImageProxy::javaobject> frame) {
      __android_log_write(ANDROID_LOG_INFO, TAG, "Frame Processor called!");

      // create HostObject which holds the Frame (JImageProxy)
      auto hostObject = std::make_shared<JImageProxyHostObject>(frame);
      function->call(rt, jsi::Object::createFromHostObject(rt, hostObject));
    });

    __android_log_write(ANDROID_LOG_INFO, TAG, "Frame Processor set!");

    return jsi::Value::undefined();
  };
  jsiRuntime.global().setProperty(jsiRuntime,
                                  "setFrameProcessor",
                                  jsi::Function::createFromHostFunction(
                                      jsiRuntime,
                                      jsi::PropNameID::forAscii(jsiRuntime,
                                                                "setFrameProcessor"),
                                      2,  // viewTag, frameProcessor
                                      setFrameProcessor));


  auto unsetFrameProcessor = [this](jsi::Runtime &runtime,
                                    const jsi::Value &thisValue,
                                    const jsi::Value *arguments,
                                    size_t count) -> jsi::Value {
    __android_log_write(ANDROID_LOG_INFO, TAG, "Removing Frame Processor...");
    if (!arguments[0].isNumber()) {
      throw jsi::JSError(runtime,
                         "Camera::unsetFrameProcessor: First argument ('viewTag') must be a number!");
    }

    // find camera view
    auto viewTag = arguments[0].asNumber();
    auto cameraView = findCameraViewById((int) viewTag);

    // call Java method to unset frame processor
    cameraView->unsetFrameProcessor();

    __android_log_write(ANDROID_LOG_INFO, TAG, "Frame Processor removed!");

    return jsi::Value::undefined();
  };
  jsiRuntime.global().setProperty(jsiRuntime,
                                  "unsetFrameProcessor",
                                  jsi::Function::createFromHostFunction(
                                      jsiRuntime,
                                      jsi::PropNameID::forAscii(jsiRuntime,
                                                                "unsetFrameProcessor"),
                                      1, // viewTag
                                      unsetFrameProcessor));

  __android_log_write(ANDROID_LOG_INFO, TAG, "Finished installing JSI bindings!");
}

void FrameProcessorRuntimeManager::registerPlugin(alias_ref<FrameProcessorPlugin::javaobject> plugin) {
  // _runtimeManager might never be null, but we can never be too sure.
  if (!_runtimeManager || !_runtimeManager->runtime) {
    throw std::runtime_error("Tried to register plugin before initializing JS runtime! Call `initializeRuntime()` first.");
  }

  auto& runtime = *_runtimeManager->runtime;

  // we need a strong reference on the plugin, make_global does that.
  auto pluginGlobal = make_global(plugin);
  auto pluginCxx = pluginGlobal->cthis();
  // name is always prefixed with two underscores (__)
  auto name = "__" + pluginCxx->getName();

  auto message = "Installing Frame Processor Plugin \"" + name + "\"...";
  __android_log_write(ANDROID_LOG_INFO, TAG, message.c_str());

  auto callback = [pluginCxx](jsi::Runtime& runtime,
                              const jsi::Value& thisValue,
                              const jsi::Value* arguments,
                              size_t count) -> jsi::Value {
    // Unbox object and get typed HostObject
    auto boxedHostObject = arguments[0].asObject(runtime).asHostObject(runtime);
    auto frameHostObject = dynamic_cast<JImageProxyHostObject*>(boxedHostObject.get());

    // parse params
    auto paramsCount = count - 1;
    auto params = JArrayClass<jobject>::newArray(paramsCount);
    for (size_t i = 1; i < paramsCount; i++) {
      params->setElement(i, JSIJNIConversion::convertJSIValueToJNIObject(runtime, arguments[i]));
    }

    // call implemented virtual method
    auto result = pluginCxx->callback(frameHostObject->frame, params);

    // convert result from JNI to JSI value
    return JSIJNIConversion::convertJNIObjectToJSIValue(runtime, result);
  };

  runtime.global().setProperty(runtime, name.c_str(), jsi::Function::createFromHostFunction(runtime,
                                                                                            jsi::PropNameID::forAscii(runtime, name),
                                                                                            1, // frame
                                                                                            callback));
}

} // namespace vision
