#pragma once

#include <NsCore/Noesis.h>
#include <NsGui/UserControl.h>

namespace War3UI {

class MainMenuBackdropControl final : public Noesis::UserControl {
public:
    MainMenuBackdropControl();

private:
    void InitializeComponent();
    NS_DECLARE_REFLECTION(MainMenuBackdropControl, UserControl)
};

class MainMenuPanelControl final : public Noesis::UserControl {
public:
    MainMenuPanelControl();

private:
    void InitializeComponent();
    NS_DECLARE_REFLECTION(MainMenuPanelControl, UserControl)
};

class SinglePlayerPanelControl final : public Noesis::UserControl {
public:
    SinglePlayerPanelControl();

private:
    void InitializeComponent();
    NS_DECLARE_REFLECTION(SinglePlayerPanelControl, UserControl)
};

class CampaignPanelControl final : public Noesis::UserControl {
public:
    CampaignPanelControl();

private:
    void InitializeComponent();
    NS_DECLARE_REFLECTION(CampaignPanelControl, UserControl)
};

class CustomGamePanelControl final : public Noesis::UserControl {
public:
    CustomGamePanelControl();

private:
    void InitializeComponent();
    NS_DECLARE_REFLECTION(CustomGamePanelControl, UserControl)
};

} // namespace War3UI
