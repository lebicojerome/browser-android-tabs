// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_VOICE_INTERACTION_VOICE_INTERACTION_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_ARC_VOICE_INTERACTION_VOICE_INTERACTION_CONTROLLER_CLIENT_H_

#include <memory>

#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace arc {

// The client of VoiceInteractionController. It monitors various user session
// states and notifies Ash side.  It can also be used to notify some specific
// state changes that does not have an observer interface.
class VoiceInteractionControllerClient
    : public ArcSessionManager::Observer,
      public content::NotificationObserver,
      public user_manager::UserManager::UserSessionStateObserver {
 public:
  VoiceInteractionControllerClient();
  ~VoiceInteractionControllerClient() override;
  static VoiceInteractionControllerClient* Get();

  // Notify the controller about state changes.
  void NotifyStatusChanged(ash::mojom::VoiceInteractionState state);

  // Testing methods.
  void SetControllerForTesting(
      ash::mojom::VoiceInteractionControllerPtr controller);
  void FlushMojoForTesting();

 private:
  friend class VoiceInteractionControllerClientTest;

  // Notify the controller about state changes.
  void NotifySettingsEnabled();
  void NotifyContextEnabled();
  void NotifySetupCompleted();
  void NotifyFeatureAllowed();

  // user_manager::UserManager::UserSessionStateObserver overrides:
  void ActiveUserChanged(const user_manager::User* active_user) override;

  // ArcSessionManager::Observer overrides:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  void SetProfile(Profile* profile);

  void ConnectToVoiceInteractionController();

  ash::mojom::VoiceInteractionControllerPtr voice_interaction_controller_;

  content::NotificationRegistrar notification_registrar_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  std::unique_ptr<user_manager::ScopedUserSessionStateObserver>
      session_state_observer_;

  Profile* profile_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(VoiceInteractionControllerClient);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_VOICE_INTERACTION_VOICE_INTERACTION_CONTROLLER_CLIENT_H_
