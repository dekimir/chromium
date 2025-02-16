/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_FACTORY_H_

#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace WTF {
class String;
}

namespace blink {

class WebIDBCallbacks;
class WebIDBDatabaseCallbacks;

class MODULES_EXPORT WebIDBFactory {
 public:
  virtual ~WebIDBFactory() = default;

  virtual void GetDatabaseInfo(WebIDBCallbacks*,
                               scoped_refptr<base::SingleThreadTaskRunner>) = 0;
  virtual void GetDatabaseNames(
      WebIDBCallbacks*,
      scoped_refptr<base::SingleThreadTaskRunner>) = 0;
  virtual void Open(const WTF::String& name,
                    long long version,
                    long long transaction_id,
                    WebIDBCallbacks*,
                    WebIDBDatabaseCallbacks*,
                    scoped_refptr<base::SingleThreadTaskRunner>) = 0;
  virtual void DeleteDatabase(const WTF::String& name,
                              WebIDBCallbacks*,
                              bool force_close,
                              scoped_refptr<base::SingleThreadTaskRunner>) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_FACTORY_H_
