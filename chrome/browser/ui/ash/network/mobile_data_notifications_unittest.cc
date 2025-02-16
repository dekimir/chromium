// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/mobile_data_notifications.h"

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_state_handler.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using chromeos::DBusThreadManager;
using chromeos::LoginState;

namespace {

const char kCellularDevicePath[] = "/device/stub_cellular_device1";
const char kCellularServicePath[] = "/service/cellular1";
const char kCellularGuid[] = "cellular1_guid";
const char kNotificationId[] = "chrome://settings/internet/mobile_data";
const char kTestUserName[] = "test-user@example.com";

class NetworkConnectTestDelegate : public chromeos::NetworkConnect::Delegate {
 public:
  NetworkConnectTestDelegate() {}
  ~NetworkConnectTestDelegate() override {}

  void ShowNetworkConfigure(const std::string& network_id) override {}
  void ShowNetworkSettings(const std::string& network_id) override {}
  bool ShowEnrollNetwork(const std::string& network_id) override {
    return false;
  }
  void ShowMobileSetupDialog(const std::string& network_id) override {}
  void ShowNetworkConnectError(const std::string& error_name,
                               const std::string& network_id) override {}
  void ShowMobileActivationError(const std::string& network_id) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkConnectTestDelegate);
};

class MobileDataNotificationsTest : public testing::Test {
 public:
  MobileDataNotificationsTest() {}
  ~MobileDataNotificationsTest() override {}

  void SetUp() override {
    testing::Test::SetUp();
    DBusThreadManager::Initialize();
    chromeos::NetworkHandler::Initialize();
    mobile_data_notifications_.reset(new MobileDataNotifications);
    SetupUser();
    SetupNetworkShillState();
    base::RunLoop().RunUntilIdle();
    network_connect_delegate_.reset(new NetworkConnectTestDelegate);
    chromeos::NetworkConnect::Initialize(network_connect_delegate_.get());
  }

  void TearDown() override {
    chromeos::NetworkConnect::Shutdown();
    network_connect_delegate_.reset();
    LoginState::Shutdown();
    profile_manager_.reset();
    user_manager_enabler_.reset();
    mobile_data_notifications_.reset();
    chromeos::NetworkHandler::Shutdown();
    DBusThreadManager::Shutdown();
    testing::Test::TearDown();
  }

 protected:
  void SetupUser() {
    // Add a user.
    auto user_manager = std::make_unique<chromeos::FakeChromeUserManager>();
    const AccountId test_account_id(AccountId::FromUserEmail(kTestUserName));
    user_manager->AddUser(test_account_id);
    user_manager->LoginUser(test_account_id);
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    // Set up notifications.
    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    // Passing nullptr sets up |display_service_| with system notifications.
    display_service_ = std::make_unique<NotificationDisplayServiceTester>(
        nullptr /* profile */);

    // Create a valid profile for the user.
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    TestingProfile* profile =
        profile_manager_->CreateTestingProfile(test_account_id.GetUserEmail());
    profile_manager_->SetLoggedIn(true);
    ASSERT_TRUE(ProfileManager::GetActiveUserProfile() == profile);

    // Set up login state.
    LoginState::Initialize();
    LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                        LoginState::LOGGED_IN_USER_REGULAR);
  }

  void SetupNetworkShillState() {
    base::RunLoop().RunUntilIdle();

    // Create a cellular device with provider.
    chromeos::ShillDeviceClient::TestInterface* device_test =
        DBusThreadManager::Get()->GetShillDeviceClient()->GetTestInterface();
    device_test->ClearDevices();
    device_test->AddDevice(kCellularDevicePath, shill::kTypeCellular,
                           "stub_cellular_device1");
    base::DictionaryValue home_provider;
    home_provider.SetString("name", "Cellular1_Provider");
    home_provider.SetString("country", "us");
    device_test->SetDeviceProperty(kCellularDevicePath,
                                   shill::kHomeProviderProperty, home_provider,
                                   /*notify_changed=*/true);

    // Create a cellular network and activate it.
    chromeos::ShillServiceClient::TestInterface* service_test =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    service_test->ClearServices();
    service_test->AddService(kCellularServicePath, kCellularGuid,
                             "cellular1" /* name */, shill::kTypeCellular,
                             "activated", true /* visible */);
    service_test->SetServiceProperty(
        kCellularServicePath, shill::kActivationStateProperty,
        base::Value(shill::kActivationStateActivated));
    service_test->SetServiceProperty(
        kCellularServicePath, shill::kConnectableProperty, base::Value(true));
  }

  PrefService* pref_service() {
    return ProfileManager::GetActiveUserProfile()->GetPrefs();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<MobileDataNotifications> mobile_data_notifications_;
  std::unique_ptr<NetworkConnectTestDelegate> network_connect_delegate_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MobileDataNotificationsTest);
};

// Verify that basic network setup does not trigger notification.
TEST_F(MobileDataNotificationsTest, SimpleSetup) {
  pref_service()->SetBoolean(prefs::kShowMobileDataNotification, true);
  EXPECT_FALSE(display_service_->GetNotification(kNotificationId));
}

// Verify that switching to cellular whiile pref is false does not display a
// notification.
TEST_F(MobileDataNotificationsTest, NotificationAlreadyShown) {
  pref_service()->SetBoolean(prefs::kShowMobileDataNotification, false);

  chromeos::NetworkConnect::Get()->ConnectToNetworkId(kCellularGuid);
  // Wait for async ConnectToNetworkId to take effect.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(display_service_->GetNotification(kNotificationId));
}

// Verify that switching to cellular while pref is true displays notification.
TEST_F(MobileDataNotificationsTest, DisplayNotification) {
  pref_service()->SetBoolean(prefs::kShowMobileDataNotification, true);

  chromeos::NetworkConnect::Get()->ConnectToNetworkId(kCellularGuid);
  // Wait for async ConnectToNetworkId to take effect.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(display_service_->GetNotification(kNotificationId));
}

// Verify that displaying the notification toggles the profile pref.
TEST_F(MobileDataNotificationsTest, TogglesPref) {
  pref_service()->SetBoolean(prefs::kShowMobileDataNotification, true);

  chromeos::NetworkConnect::Get()->ConnectToNetworkId(kCellularGuid);
  // Wait for async ConnectToNetworkId to take effect.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(pref_service()->GetBoolean(prefs::kShowMobileDataNotification));
}

}  // namespace
