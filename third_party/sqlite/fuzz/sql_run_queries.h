// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "third_party/sqlite/sqlite3.h"

namespace sql_fuzzer {
/* Standalone function that wraps the three functions below. */
void RunSqlQueries(std::vector<std::string> queries);

sqlite3* InitConnectionForFuzzing();
void RunSqlQueriesOnConnection(sqlite3* db, std::vector<std::string> queries);
void CloseConnection(sqlite3* db);
}  // namespace sql_fuzzer
