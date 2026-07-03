#pragma once

#include <NsCore/Aero.h>
#include <NsGui/UserControl.h>

namespace War3UI {

class MainMenuBackdropControl final : public Aero::UserControl {
public:
    MainMenuBackdropControl();

private:
    void InitializeComponent();
    NS_DECLARE_REFLECTION(MainMenuBackdropControl, UserControl)
};

class MainMenuPanelControl final : public Aero::UserControl {
public:
    MainMenuPanelControl();

private:
    void InitializeComponent();
    NS_DECLARE_REFLECTION(MainMenuPanelControl, UserControl)
};

class SinglePlayerPanelControl final : public Aero::UserControl {
public:
    SinglePlayerPanelControl();

private:
    void InitializeComponent();
    NS_DECLARE_REFLECTION(SinglePlayerPanelControl, UserControl)
};

class CampaignPanelControl final : public Aero::UserControl {
public:
    CampaignPanelControl();

private:
    void InitializeComponent();
    NS_DECLARE_REFLECTION(CampaignPanelControl, UserControl)
};

class CustomGamePanelControl final : public Aero::UserControl {
public:
    CustomGamePanelControl();

private:
    void InitializeComponent();
    NS_DECLARE_REFLECTION(CustomGamePanelControl, UserControl)
};

class GlueOptionsPanelControl final : public Aero::UserControl {
public:
    GlueOptionsPanelControl();

private:
    void InitializeComponent();
    NS_DECLARE_REFLECTION(GlueOptionsPanelControl, UserControl)
};

class GlueBattleNetPanelControl final : public Aero::UserControl {
public:
    GlueBattleNetPanelControl();

private:
    void InitializeComponent();
    NS_DECLARE_REFLECTION(GlueBattleNetPanelControl, UserControl)
};

class GlueLocalAreaNetworkPanelControl final : public Aero::UserControl {
public:
    GlueLocalAreaNetworkPanelControl();

private:
    void InitializeComponent();
    NS_DECLARE_REFLECTION(GlueLocalAreaNetworkPanelControl, UserControl)
};

} // namespace War3UI
