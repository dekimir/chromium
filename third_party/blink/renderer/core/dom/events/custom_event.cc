/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/dom/events/custom_event.h"

#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

CustomEvent::CustomEvent() = default;

CustomEvent::CustomEvent(ScriptState* script_state,
                         const AtomicString& type,
                         const CustomEventInit* initializer)
    : Event(type, initializer) {
  if (initializer->hasDetail()) {
    detail_.SetAcrossWorld(initializer->detail().GetIsolate(),
                           initializer->detail().V8Value());
  }
}

CustomEvent::~CustomEvent() = default;

void CustomEvent::initCustomEvent(ScriptState* script_state,
                                  const AtomicString& type,
                                  bool bubbles,
                                  bool cancelable,
                                  const ScriptValue& script_value) {
  initEvent(type, bubbles, cancelable);
  if (!IsBeingDispatched() && !script_value.IsEmpty())
    detail_.SetAcrossWorld(script_value.GetIsolate(), script_value.V8Value());
}

ScriptValue CustomEvent::detail(ScriptState* script_state) const {
  v8::Isolate* isolate = script_state->GetIsolate();
  if (detail_.IsEmpty())
    return ScriptValue(script_state, v8::Null(isolate));
  return ScriptValue(script_state, detail_.GetAcrossWorld(script_state));
}

const AtomicString& CustomEvent::InterfaceName() const {
  return event_interface_names::kCustomEvent;
}

void CustomEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(detail_);
  Event::Trace(visitor);
}

}  // namespace blink
