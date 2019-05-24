// Copyright (c) Facebook, Inc. and its affiliates.

// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "Instance.h"

#include "JSBigString.h"
#include "JSBundleType.h"
#include "JSExecutor.h"
#include "MessageQueueThread.h"
#include "MethodCall.h"
#include "NativeToJsBridge.h"
#include "RAMBundleRegistry.h"
#include "RecoverableError.h"
#include "SystraceSection.h"

#include <cxxreact/JSIndexedRAMBundle.h>
#include <folly/Memory.h>
#include <folly/MoveWrapper.h>
#include <folly/json.h>

#include <glog/logging.h>

#include <condition_variable>
#include <fstream>
#include <mutex>
#include <string>

namespace facebook {
namespace react {

Instance::~Instance() {
  if (bundleRegistry_) {
    bundleRegistry_->disposeExecutionEnvironments();
  }
}

void Instance::initializeBridge(
    std::unique_ptr<InstanceCallback> callback,
    std::shared_ptr<JSExecutorFactory> jsef,
    std::shared_ptr<MessageQueueThread> jsQueue,
    std::shared_ptr<ModuleRegistry> moduleRegistry) {
  callback_ = std::move(callback);
  moduleRegistry_ = std::move(moduleRegistry);
  bundleRegistry_ = std::make_unique<BundleRegistry>(
    jsef.get(),
    moduleRegistry_,
    callback_,
    [jsQueue]() { return jsQueue; } // TODO: use a factory
  );
  // jsQueue->runOnQueueSync([this, &jsef, jsQueue]() mutable {
  //   nativeToJsBridge_ = folly::make_unique<NativeToJsBridge>(
  //       jsef.get(), moduleRegistry_, jsQueue, callback_);

  //   std::lock_guard<std::mutex> lock(m_syncMutex);
  //   m_syncReady = true;
  //   m_syncCV.notify_all();
  // });

  // CHECK(nativeToJsBridge_);
}

void Instance::loadBundleAsync(std::unique_ptr<const Bundle> bundle) {
  callback_->incrementPendingJSCalls();
  SystraceSection s("Instance::loadBundleAsync", "sourceURL", bundle->getSourceURL());
  bundleRegistry_->runNewExecutionEnvironment(std::move(bundle), [this]() {
    std::lock_guard<std::mutex> lock(m_syncMutex);
    m_syncReady = true;
    m_syncCV.notify_all();
  });
}

void Instance::loadBundleSync(std::unique_ptr<const Bundle> bundle) {
  std::unique_lock<std::mutex> lock(m_syncMutex);
  m_syncCV.wait(lock, [this] { return m_syncReady; });

  SystraceSection s("Instance::loadBundleSync", "sourceURL", bundle->getSourceURL());
  bundleRegistry_->runNewExecutionEnvironment(std::move(bundle), [this]() {
    // TODO: check if this is needed at all
    std::lock_guard<std::mutex> lock(m_syncMutex);
    m_syncReady = true;
    m_syncCV.notify_all();
  });
}

void Instance::loadBundle(std::unique_ptr<const Bundle> bundle, bool loadSynchronously) {
  SystraceSection s("Instance::loadBundle", "sourceURL", bundle->getSourceURL());
  if (loadSynchronously) {
    loadBundleSync(std::move(bundle));
  } else {
    loadBundleAsync(std::move(bundle));
  }
}

bool Instance::isIndexedRAMBundle(const char *sourcePath) {
  std::ifstream bundle_stream(sourcePath, std::ios_base::in);
  BundleHeader header;

  if (!bundle_stream ||
      !bundle_stream.read(reinterpret_cast<char *>(&header), sizeof(header))) {
    return false;
  }

  return parseTypeFromHeader(header) == ScriptTag::RAMBundle;
}

bool Instance::isIndexedRAMBundle(std::unique_ptr<const JSBigString>* script) {
  BundleHeader header;
  strncpy(reinterpret_cast<char *>(&header), script->get()->c_str(), sizeof(header));

  return parseTypeFromHeader(header) == ScriptTag::RAMBundle;
}

void Instance::setGlobalVariable(std::string propName,
                                 std::unique_ptr<const JSBigString> jsonValue) {
  // nativeToJsBridge_->setGlobalVariable(std::move(propName),
  //                                      std::move(jsonValue));
}

void *Instance::getJavaScriptContext() {
  // return nativeToJsBridge_ ? nativeToJsBridge_->getJavaScriptContext()
  //                          : nullptr;
}

bool Instance::isInspectable() {
  // return nativeToJsBridge_ ? nativeToJsBridge_->isInspectable() : false;
}
  
bool Instance::isBatchActive() {
  // return nativeToJsBridge_ ? nativeToJsBridge_->isBatchActive() : false;
}

void Instance::callJSFunction(std::string &&module, std::string &&method,
                              folly::dynamic &&params) {
  // callback_->incrementPendingJSCalls();
  // nativeToJsBridge_->callFunction(std::move(module), std::move(method),
  //                                 std::move(params));
}

void Instance::callJSCallback(uint64_t callbackId, folly::dynamic &&params) {
  // SystraceSection s("Instance::callJSCallback");
  // callback_->incrementPendingJSCalls();
  // nativeToJsBridge_->invokeCallback((double)callbackId, std::move(params));
}

const ModuleRegistry &Instance::getModuleRegistry() const {
  return *moduleRegistry_;
}

ModuleRegistry &Instance::getModuleRegistry() { return *moduleRegistry_; }

void Instance::handleMemoryPressure(int pressureLevel) {
  // nativeToJsBridge_->handleMemoryPressure(pressureLevel);
}

} // namespace react
} // namespace facebook
