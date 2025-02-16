// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SERVICE_APP_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SERVICE_APP_RESULT_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/app_list/search/app_result.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"

class AppListControllerDelegate;
class Profile;

namespace app_list {

class AppServiceAppResult : public AppResult {
 public:
  AppServiceAppResult(Profile* profile,
                      const std::string& app_id,
                      AppListControllerDelegate* controller,
                      bool is_recommendation);
  ~AppServiceAppResult() override;

 private:
  // ChromeSearchResult overrides:
  void Open(int event_flags) override;

  // AppContextMenuDelegate overrides:
  void ExecuteLaunchCommand(int event_flags) override;

  void OnLoadIcon(bool chip, apps::mojom::IconValuePtr icon_value);

  // TODO(crbug.com/826982): implement context menus.

  base::WeakPtrFactory<AppServiceAppResult> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppServiceAppResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SERVICE_APP_RESULT_H_
