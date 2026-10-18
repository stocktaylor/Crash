// crash - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/rex_app.h>
#include <rex/system/flags.h>
#include <rexlauncher/launch_menu.h>

class CrashApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<CrashApp>(new CrashApp(ctx, "crash",
        PPCImageConfig));
  }

  // The dump's file names are upper case. The guest VFS matches names
  // case-insensitively, but the SDK's host-side existence check before loading
  // uses "game:\default.xex" as a literal path.
  void OnLoadXexImage(std::string& xex_image) override {
    xex_image = "game:\\DEFAULT.XEX";
  }

  // With no --game_data_root, use a "game" folder next to the executable, so a
  // packaged build runs with just `./crash` (see rextools `rex dist`).
  void OnConfigurePaths(rex::PathConfig& paths) override {
    if (paths.game_data_root.empty()) {
      paths.game_data_root = rex::filesystem::GetExecutableFolder() / "game";
    }
  }

  // Launch menu (../rexlauncher): shown in the game window before the game
  // starts; Play hands the window over to the game.
  void OnConfigureFonts(ImFontAtlas* atlas) override {
    rexlauncher::ConfigureFonts(atlas);
  }

  std::optional<rex::PathConfig> OnFinalizePaths(
      const rex::PathConfig& defaults,
      std::function<void(rex::PathConfig)> resume) override {
    rexlauncher::Config menu;
    menu.title = "Burnout CRASH!";
    // license_mask bit 0 = full game (see OnPostSetup below).
    menu.extra.push_back({"License", "license_mask",
                          {{"1", "Full game"}, {"0", "Trial"}},
                          /*requires_restart=*/false, /*restart_in_game=*/true});
    // src/crash_hooks.cpp: present every vblank instead of every second one.
    menu.extra.push_back({"Frame rate", "crash_60fps",
                          {{"false", "30 fps (original)"}, {"true", "60 fps (experimental)"}}});
    return rexlauncher::Show(imgui_drawer(), app_context(), defaults,
                             std::move(resume), std::move(menu));
  }

  // XBLA licensing: the game asks XamContentGetLicenseMask and treats bit 0 as
  // "full game purchased"; anything else runs the trial. The mask comes from
  // the license_mask setting (crash.toml, --license_mask, or
  // REX_LICENSE_MASK), default 0. Logged so bug reports show which mode ran.
  void OnPostSetup() override {
    uint32_t mask = REXCVAR_GET(license_mask);
    if (mask & 1) {
      REXLOG_INFO("License: full game (license_mask={:#x})", mask);
    } else {
      REXLOG_INFO(
          "License: trial (license_mask={:#x}). Own the full game? Set license_mask = 1 "
          "in crash.toml",
          mask);
    }
    // In-game menu (F1 or Back + Start), same options as the launch menu.
    rexlauncher::EnableInGameMenu(imgui_drawer(), app_context(), runtime());
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  // void OnPreSetup(rex::RuntimeConfig& config) override {}
  // void OnPostLoadXexImage() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}
};
