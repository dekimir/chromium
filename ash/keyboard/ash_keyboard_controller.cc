// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ash_keyboard_controller.h"

#include "ash/keyboard/ash_keyboard_ui.h"
#include "ash/keyboard/virtual_keyboard_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_ui_factory.h"
#include "ui/wm/core/coordinate_conversion.h"

using keyboard::mojom::KeyboardConfig;
using keyboard::mojom::KeyboardConfigPtr;
using keyboard::mojom::KeyboardEnableFlag;

namespace ash {

AshKeyboardController::AshKeyboardController(
    SessionController* session_controller)
    : session_controller_(session_controller),
      keyboard_controller_(std::make_unique<keyboard::KeyboardController>()) {
  if (session_controller_)  // May be null in tests.
    session_controller_->AddObserver(this);
  keyboard_controller_->AddObserver(this);
}

AshKeyboardController::~AshKeyboardController() {
  keyboard_controller_->RemoveObserver(this);
  if (session_controller_)  // May be null in tests.
    session_controller_->RemoveObserver(this);
}

void AshKeyboardController::BindRequest(
    mojom::KeyboardControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void AshKeyboardController::EnableKeyboard() {
  if (!keyboard_controller_->IsKeyboardEnableRequested())
    return;

  // De-activate the keyboard, as some callers expect the keyboard to be
  // reloaded. TODO(https://crbug.com/731537): Add a separate function for
  // reloading the keyboard.
  DeactivateKeyboard();

  keyboard_controller_->EnableKeyboard(
      keyboard_ui_factory_ ? keyboard_ui_factory_->CreateKeyboardUI()
                           : std::make_unique<AshKeyboardUI>(this),
      virtual_keyboard_controller_.get());
  ActivateKeyboard();
}

void AshKeyboardController::DisableKeyboard() {
  DeactivateKeyboard();
  keyboard_controller_->DisableKeyboard();
}

void AshKeyboardController::CreateVirtualKeyboard(
    std::unique_ptr<keyboard::KeyboardUIFactory> keyboard_ui_factory) {
  DCHECK(keyboard_ui_factory || features::IsUsingWindowService())
      << "keyboard_ui_factory can be null only when window service is used.";
  keyboard_ui_factory_ = std::move(keyboard_ui_factory);
  virtual_keyboard_controller_ = std::make_unique<VirtualKeyboardController>();
}

void AshKeyboardController::DestroyVirtualKeyboard() {
  virtual_keyboard_controller_.reset();
}

void AshKeyboardController::SendOnKeyboardVisibleBoundsChanged(
    const gfx::Rect& bounds) {
  DVLOG(1) << "OnKeyboardVisibleBoundsChanged: " << bounds.ToString();
  // Pass the bounds in screen coordinates over mojo.
  gfx::Rect screen_bounds = BoundsToScreen(bounds);
  observers_.ForAllPtrs(
      [&screen_bounds](mojom::KeyboardControllerObserver* observer) {
        observer->OnKeyboardVisibleBoundsChanged(screen_bounds);
      });
}

void AshKeyboardController::SendOnLoadKeyboardContentsRequested() {
  observers_.ForAllPtrs([](mojom::KeyboardControllerObserver* observer) {
    observer->OnLoadKeyboardContentsRequested();
  });
}

void AshKeyboardController::SendOnKeyboardUIDestroyed() {
  observers_.ForAllPtrs([](mojom::KeyboardControllerObserver* observer) {
    observer->OnKeyboardUIDestroyed();
  });
}

// mojom::KeyboardController

void AshKeyboardController::KeyboardContentsLoaded(
    const base::UnguessableToken& token,
    const gfx::Size& size) {
  keyboard_controller()->KeyboardContentsLoaded(token, size);
}

void AshKeyboardController::GetKeyboardConfig(
    GetKeyboardConfigCallback callback) {
  std::move(callback).Run(
      KeyboardConfig::New(keyboard_controller_->keyboard_config()));
}

void AshKeyboardController::SetKeyboardConfig(
    KeyboardConfigPtr keyboard_config) {
  keyboard_controller_->UpdateKeyboardConfig(*keyboard_config);
}

void AshKeyboardController::IsKeyboardEnabled(
    IsKeyboardEnabledCallback callback) {
  std::move(callback).Run(keyboard_controller_->IsEnabled());
}

void AshKeyboardController::SetEnableFlag(KeyboardEnableFlag flag) {
  bool was_enabled = keyboard_controller_->IsEnabled();
  keyboard_controller_->SetEnableFlag(flag);
  UpdateEnableFlag(was_enabled);
}

void AshKeyboardController::ClearEnableFlag(KeyboardEnableFlag flag) {
  bool was_enabled = keyboard_controller_->IsEnabled();
  keyboard_controller_->ClearEnableFlag(flag);
  UpdateEnableFlag(was_enabled);
}

void AshKeyboardController::GetEnableFlags(GetEnableFlagsCallback callback) {
  const std::set<keyboard::mojom::KeyboardEnableFlag>& keyboard_enable_flags =
      keyboard_controller_->keyboard_enable_flags();
  std::vector<keyboard::mojom::KeyboardEnableFlag> flags(
      keyboard_enable_flags.begin(), keyboard_enable_flags.end());
  std::move(callback).Run(std::move(flags));
}

void AshKeyboardController::ReloadKeyboardIfNeeded() {
  keyboard_controller_->Reload();
}

void AshKeyboardController::RebuildKeyboardIfEnabled() {
  // Test IsKeyboardEnableRequested in case of an unlikely edge case where this
  // is called while after the enable state changed to disabled (in which case
  // we do not want to override the requested state).
  if (keyboard_controller_->IsKeyboardEnableRequested())
    EnableKeyboard();
}

void AshKeyboardController::IsKeyboardVisible(
    IsKeyboardVisibleCallback callback) {
  std::move(callback).Run(keyboard_controller_->IsKeyboardVisible());
}

void AshKeyboardController::ShowKeyboard() {
  if (keyboard_controller_->IsEnabled())
    keyboard_controller_->ShowKeyboard(false /* lock */);
}

void AshKeyboardController::HideKeyboard(mojom::HideReason reason) {
  if (!keyboard_controller_->IsEnabled())
    return;
  switch (reason) {
    case mojom::HideReason::kUser:
      keyboard_controller_->HideKeyboardByUser();
      break;
    case mojom::HideReason::kSystem:
      keyboard_controller_->HideKeyboardExplicitlyBySystem();
      break;
  }
}

void AshKeyboardController::SetContainerType(
    keyboard::mojom::ContainerType container_type,
    const base::Optional<gfx::Rect>& target_bounds,
    SetContainerTypeCallback callback) {
  keyboard_controller_->SetContainerType(container_type, target_bounds,
                                         std::move(callback));
}

void AshKeyboardController::SetKeyboardLocked(bool locked) {
  keyboard_controller_->set_keyboard_locked(locked);
}

void AshKeyboardController::SetOccludedBounds(
    const std::vector<gfx::Rect>& bounds) {
  // TODO(https://crbug.com/826617): Support occluded bounds with multiple
  // rectangles.
  keyboard_controller_->SetOccludedBounds(bounds.empty() ? gfx::Rect()
                                                         : bounds[0]);
}

void AshKeyboardController::SetHitTestBounds(
    const std::vector<gfx::Rect>& bounds) {
  keyboard_controller_->SetHitTestBounds(bounds);
}

void AshKeyboardController::SetDraggableArea(const gfx::Rect& bounds) {
  keyboard_controller_->SetDraggableArea(bounds);
}

void AshKeyboardController::AddObserver(
    mojom::KeyboardControllerObserverAssociatedPtrInfo observer) {
  mojom::KeyboardControllerObserverAssociatedPtr observer_ptr;
  observer_ptr.Bind(std::move(observer));
  observers_.AddPtr(std::move(observer_ptr));
}

// SessionObserver
void AshKeyboardController::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (!keyboard_controller_->IsKeyboardEnableRequested())
    return;

  switch (state) {
    case session_manager::SessionState::OOBE:
    case session_manager::SessionState::LOGIN_PRIMARY:
      ActivateKeyboard();
      break;
    case session_manager::SessionState::LOGGED_IN_NOT_ACTIVE:
    case session_manager::SessionState::ACTIVE:
      // Reload the keyboard on user profile change to refresh keyboard
      // extensions with the new profile and ensure the extensions call the
      // proper IME. |LOGGED_IN_NOT_ACTIVE| is needed so that the virtual
      // keyboard works on supervised user creation, http://crbug.com/712873.
      // |ACTIVE| is also needed for guest user workflow.
      EnableKeyboard();
      break;
    default:
      break;
  }
}

// private methods

void AshKeyboardController::ActivateKeyboard() {
  ActivateKeyboardForRoot(Shell::Get()->GetPrimaryRootWindowController());
}

void AshKeyboardController::ActivateKeyboardForRoot(
    RootWindowController* controller) {
  DCHECK(controller);
  if (!keyboard_controller_->IsEnabled())
    return;

  // If the keyboard is already activated for |controller|, do nothing.
  if (controller->GetRootWindow() == keyboard_controller_->GetRootWindow())
    return;

  aura::Window* container =
      controller->GetContainer(kShellWindowId_VirtualKeyboardContainer);
  DCHECK(container);
  keyboard_controller_->ActivateKeyboardInContainer(container);
  keyboard_controller_->LoadKeyboardWindowInBackground();
}

void AshKeyboardController::DeactivateKeyboard() {
  if (!keyboard_controller_->IsEnabled() ||
      !keyboard_controller_->GetRootWindow()) {
    return;
  }
  keyboard_controller_->DeactivateKeyboard();
}

void AshKeyboardController::UpdateEnableFlag(bool was_enabled) {
  bool is_enabled = keyboard_controller_->IsKeyboardEnableRequested();
  if (is_enabled && !was_enabled) {
    EnableKeyboard();
  } else if (!is_enabled && was_enabled) {
    DisableKeyboard();
  }
}

void AshKeyboardController::OnKeyboardConfigChanged() {
  KeyboardConfigPtr config =
      KeyboardConfig::New(keyboard_controller_->keyboard_config());
  observers_.ForAllPtrs([&config](mojom::KeyboardControllerObserver* observer) {
    observer->OnKeyboardConfigChanged(config.Clone());
  });
}

void AshKeyboardController::OnKeyboardVisibilityStateChanged(bool is_visible) {
  observers_.ForAllPtrs(
      [is_visible](mojom::KeyboardControllerObserver* observer) {
        observer->OnKeyboardVisibilityChanged(is_visible);
      });
}

void AshKeyboardController::OnKeyboardVisibleBoundsChanged(
    const gfx::Rect& bounds) {
  SendOnKeyboardVisibleBoundsChanged(bounds);
}

void AshKeyboardController::OnKeyboardWorkspaceOccludedBoundsChanged(
    const gfx::Rect& bounds) {
  DVLOG(1) << "OnKeyboardOccludedBoundsChanged: " << bounds.ToString();
  // Pass the bounds in screen coordinates over mojo.
  gfx::Rect screen_bounds = BoundsToScreen(bounds);
  observers_.ForAllPtrs(
      [&screen_bounds](mojom::KeyboardControllerObserver* observer) {
        observer->OnKeyboardOccludedBoundsChanged(screen_bounds);
      });
}

void AshKeyboardController::OnKeyboardEnableFlagsChanged(
    std::set<keyboard::mojom::KeyboardEnableFlag>& keyboard_enable_flags) {
  std::vector<keyboard::mojom::KeyboardEnableFlag> flags(
      keyboard_enable_flags.begin(), keyboard_enable_flags.end());
  observers_.ForAllPtrs([&flags](mojom::KeyboardControllerObserver* observer) {
    observer->OnKeyboardEnableFlagsChanged(flags);
  });
}

void AshKeyboardController::OnKeyboardEnabledChanged(bool is_enabled) {
  observers_.ForAllPtrs(
      [is_enabled](mojom::KeyboardControllerObserver* observer) {
        observer->OnKeyboardEnabledChanged(is_enabled);
      });
}

gfx::Rect AshKeyboardController::BoundsToScreen(const gfx::Rect& bounds) {
  DCHECK(keyboard_controller_->GetKeyboardWindow());
  gfx::Rect screen_bounds(bounds);
  ::wm::ConvertRectToScreen(keyboard_controller_->GetKeyboardWindow(),
                            &screen_bounds);
  return screen_bounds;
}

}  // namespace ash
