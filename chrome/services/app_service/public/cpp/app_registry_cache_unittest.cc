// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "chrome/services/app_service/public/cpp/app_registry_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

class AppRegistryCacheTest : public testing::Test,
                             public apps::AppRegistryCache::Observer {
 protected:
  apps::mojom::AppPtr MakeApp(
      const char* app_id,
      const char* name,
      apps::mojom::Readiness readiness = apps::mojom::Readiness::kUnknown) {
    apps::mojom::AppPtr app = apps::mojom::App::New();
    app->app_type = apps::mojom::AppType::kArc;
    app->app_id = app_id;
    app->readiness = readiness;
    app->name = name;
    return app;
  }

  void CallForEachApp(apps::AppRegistryCache& cache) {
    cache.ForEachApp(
        [this](const apps::AppUpdate& update) { OnAppUpdate(update); });
  }

  // apps::AppRegistryCache::Observer overrides.
  void OnAppUpdate(const apps::AppUpdate& update) override {
    EXPECT_NE("", update.Name());
    if (update.ReadinessChanged() &&
        (update.Readiness() == apps::mojom::Readiness::kReady)) {
      num_freshly_installed_++;
    }
    updated_ids_.insert(update.AppId());
    updated_names_.insert(update.Name());
  }

  int num_freshly_installed_ = 0;
  std::set<std::string> updated_ids_;
  std::set<std::string> updated_names_;
};

// Responds to a cache's OnAppUpdate to call back into the cache, checking that
// the cache presents a self-consistent snapshot. For example, the app names
// should match for the outer and inner AppUpdate.
class RecursiveObserver : public apps::AppRegistryCache::Observer {
 public:
  explicit RecursiveObserver(apps::AppRegistryCache* cache) : cache_(cache) {
    cache_->AddObserver(this);
  }

  ~RecursiveObserver() override { cache_->RemoveObserver(this); }

  void PrepareForOnApps(int expected_num_apps,
                        const std::string& expected_name_for_p) {
    expected_name_for_p_ = expected_name_for_p;
    expected_num_apps_ = expected_num_apps;
    num_apps_seen_on_app_update_ = 0;
    names_.clear();
  }

  int NumAppsSeenOnAppUpdate() { return num_apps_seen_on_app_update_; }

 protected:
  // apps::AppRegistryCache::Observer overrides.
  void OnAppUpdate(const apps::AppUpdate& outer) override {
    int num_apps = 0;
    cache_->ForEachApp([this, &outer, &num_apps](const apps::AppUpdate& inner) {
      if (num_apps_seen_on_app_update_ == 0) {
        // If this is the first time that OnAppUpdate is called, after a
        // PrepareForOnApps call, then just populate the names_ map.
        names_[inner.AppId()] = inner.Name();
      } else {
        // Otherwise, check that the names found during this OnAppUpdate call
        // match those during the first OnAppUpdate call.
        auto iter = names_.find(inner.AppId());
        EXPECT_EQ(inner.Name(), (iter != names_.end()) ? iter->second : "");
      }

      if (outer.AppId() == inner.AppId()) {
        ExpectEq(outer, inner);
      }

      if (inner.AppId() == "p") {
        EXPECT_EQ(expected_name_for_p_, inner.Name());
      }

      num_apps++;
    });
    EXPECT_EQ(expected_num_apps_, num_apps);

    EXPECT_FALSE(cache_->ForOneApp(
        "no_such_app_id",
        [&outer](const apps::AppUpdate& inner) { ExpectEq(outer, inner); }));

    EXPECT_TRUE(cache_->ForOneApp(
        outer.AppId(),
        [&outer](const apps::AppUpdate& inner) { ExpectEq(outer, inner); }));

    num_apps_seen_on_app_update_++;
  }

  static void ExpectEq(const apps::AppUpdate& outer,
                       const apps::AppUpdate& inner) {
    EXPECT_EQ(outer.AppType(), inner.AppType());
    EXPECT_EQ(outer.AppId(), inner.AppId());
    EXPECT_EQ(outer.StateIsNull(), inner.StateIsNull());
    EXPECT_EQ(outer.Readiness(), inner.Readiness());
    EXPECT_EQ(outer.Name(), inner.Name());
  }

  apps::AppRegistryCache* cache_;
  std::string expected_name_for_p_;
  int expected_num_apps_;
  std::map<std::string, std::string> names_;
  int num_apps_seen_on_app_update_;
};

TEST_F(AppRegistryCacheTest, ForEachApp) {
  std::vector<apps::mojom::AppPtr> deltas;
  apps::AppRegistryCache cache;

  updated_names_.clear();
  CallForEachApp(cache);

  EXPECT_EQ(0u, updated_names_.size());

  deltas.clear();
  deltas.push_back(MakeApp("a", "apple"));
  deltas.push_back(MakeApp("b", "banana"));
  deltas.push_back(MakeApp("c", "cherry"));
  cache.OnApps(std::move(deltas));

  updated_names_.clear();
  CallForEachApp(cache);

  EXPECT_EQ(3u, updated_names_.size());
  EXPECT_NE(updated_names_.end(), updated_names_.find("apple"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("banana"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("cherry"));

  deltas.clear();
  deltas.push_back(MakeApp("a", "apricot"));
  deltas.push_back(MakeApp("d", "durian"));
  cache.OnApps(std::move(deltas));

  updated_names_.clear();
  CallForEachApp(cache);

  EXPECT_EQ(4u, updated_names_.size());
  EXPECT_NE(updated_names_.end(), updated_names_.find("apricot"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("banana"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("cherry"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("durian"));

  // Test that ForOneApp succeeds for "c" and fails for "e".

  bool found_c = false;
  EXPECT_TRUE(cache.ForOneApp("c", [&found_c](const apps::AppUpdate& update) {
    found_c = true;
    EXPECT_EQ("c", update.AppId());
  }));
  EXPECT_TRUE(found_c);

  bool found_e = false;
  EXPECT_FALSE(cache.ForOneApp("e", [&found_e](const apps::AppUpdate& update) {
    found_e = true;
    EXPECT_EQ("e", update.AppId());
  }));
  EXPECT_FALSE(found_e);
}

TEST_F(AppRegistryCacheTest, Observer) {
  std::vector<apps::mojom::AppPtr> deltas;
  apps::AppRegistryCache cache;

  cache.AddObserver(this);

  num_freshly_installed_ = 0;
  updated_ids_.clear();
  deltas.clear();
  deltas.push_back(MakeApp("a", "avocado"));
  deltas.push_back(MakeApp("c", "cucumber"));
  deltas.push_back(MakeApp("e", "eggfruit"));
  cache.OnApps(std::move(deltas));

  EXPECT_EQ(0, num_freshly_installed_);
  EXPECT_EQ(3u, updated_ids_.size());
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("a"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("c"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("e"));

  num_freshly_installed_ = 0;
  updated_ids_.clear();
  deltas.clear();
  deltas.push_back(MakeApp("b", "blueberry"));
  deltas.push_back(MakeApp("c", "cucumber", apps::mojom::Readiness::kReady));
  cache.OnApps(std::move(deltas));

  EXPECT_EQ(1, num_freshly_installed_);
  EXPECT_EQ(2u, updated_ids_.size());
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("b"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("c"));

  cache.RemoveObserver(this);

  num_freshly_installed_ = 0;
  updated_ids_.clear();
  deltas.clear();
  deltas.push_back(MakeApp("f", "fig"));
  cache.OnApps(std::move(deltas));

  EXPECT_EQ(0, num_freshly_installed_);
  EXPECT_EQ(0u, updated_ids_.size());
}

TEST_F(AppRegistryCacheTest, Recursive) {
  std::vector<apps::mojom::AppPtr> deltas;
  apps::AppRegistryCache cache;
  RecursiveObserver observer(&cache);

  observer.PrepareForOnApps(2, "peach");
  deltas.clear();
  deltas.push_back(MakeApp("o", "orange"));
  deltas.push_back(MakeApp("p", "peach"));
  cache.OnApps(std::move(deltas));
  EXPECT_EQ(2, observer.NumAppsSeenOnAppUpdate());

  observer.PrepareForOnApps(3, "pear");
  deltas.clear();
  deltas.push_back(MakeApp("p", "pear", apps::mojom::Readiness::kReady));
  deltas.push_back(MakeApp("q", "quince"));
  cache.OnApps(std::move(deltas));
  EXPECT_EQ(2, observer.NumAppsSeenOnAppUpdate());

  observer.PrepareForOnApps(3, "plum");
  deltas.clear();
  deltas.push_back(MakeApp("p", "pear"));
  deltas.push_back(MakeApp("p", "pear"));
  deltas.push_back(MakeApp("p", "plum"));
  cache.OnApps(std::move(deltas));
  EXPECT_EQ(1, observer.NumAppsSeenOnAppUpdate());
}
