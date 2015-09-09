// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/renderer/distiller_native_javascript.h"

#include <string>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/dom_distiller/content/common/distiller_javascript_service.mojom.h"
#include "content/public/common/service_registry.h"
#include "content/public/renderer/render_frame.h"
#include "gin/arguments.h"
#include "gin/function_template.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "v8/include/v8.h"

using blink::WebLocalFrame;

namespace dom_distiller {

DistillerNativeJavaScript::DistillerNativeJavaScript(
    content::RenderFrame* render_frame)
    : render_frame_(render_frame) {}

DistillerNativeJavaScript::~DistillerNativeJavaScript() {}

void DistillerNativeJavaScript::AddJavaScriptObjectToFrame(
    v8::Local<v8::Context> context) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> distiller_obj =
      GetOrCreateDistillerObject(isolate, context->Global());

  distiller_obj->Set(
      gin::StringToSymbol(isolate, "echo"),
      gin::CreateFunctionTemplate(
          isolate, base::Bind(&DistillerNativeJavaScript::DistillerEcho,
                              base::Unretained(this)))
          ->GetFunction());
}

std::string DistillerNativeJavaScript::DistillerEcho(
    const std::string& message) {
  if (!distiller_js_service_) {
    render_frame_->GetServiceRegistry()->ConnectToRemoteService(
        mojo::GetProxy(&distiller_js_service_));
  }
  // TODO(mdjones): It is possible and beneficial to have information
  // returned from the browser process with these calls. The problem
  // is waiting blocks this process.
  distiller_js_service_->HandleDistillerEchoCall(message);

  return message;
}

v8::Local<v8::Object> GetOrCreateDistillerObject(v8::Isolate* isolate,
                                                 v8::Local<v8::Object> global) {
  v8::Local<v8::Object> distiller_obj;
  v8::Local<v8::Value> distiller_value =
      global->Get(gin::StringToV8(isolate, "distiller"));
  if (distiller_value.IsEmpty() || !distiller_value->IsObject()) {
    distiller_obj = v8::Object::New(isolate);
    global->Set(gin::StringToSymbol(isolate, "distiller"), distiller_obj);
  } else {
    distiller_obj = v8::Local<v8::Object>::Cast(distiller_value);
  }
  return distiller_obj;
}

}  // namespace dom_distiller
