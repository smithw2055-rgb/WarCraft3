#include "GluePanels.h"

#include <NsCore/ReflectionImplement.h>
#include <NsGui/IntegrationAPI.h>
#include <NsGui/Uri.h>

namespace War3UI {

MainMenuBackdropControl::MainMenuBackdropControl() {
    InitializeComponent();
}

void MainMenuBackdropControl::InitializeComponent() {
    Aero::GUI::LoadComponent(this, Aero::Uri("assets/ui/glue/MainMenuBackdrop.xaml"));
}

MainMenuPanelControl::MainMenuPanelControl() {
    InitializeComponent();
}

void MainMenuPanelControl::InitializeComponent() {
    Aero::GUI::LoadComponent(this, Aero::Uri("assets/ui/glue/MainMenuPanel.xaml"));
}

SinglePlayerPanelControl::SinglePlayerPanelControl() {
    InitializeComponent();
}

void SinglePlayerPanelControl::InitializeComponent() {
    Aero::GUI::LoadComponent(this, Aero::Uri("assets/ui/glue/SinglePlayerPanel.xaml"));
}

CampaignPanelControl::CampaignPanelControl() {
    InitializeComponent();
}

void CampaignPanelControl::InitializeComponent() {
    Aero::GUI::LoadComponent(this, Aero::Uri("assets/ui/glue/CampaignPanel.xaml"));
}

CustomGamePanelControl::CustomGamePanelControl() {
    InitializeComponent();
}

void CustomGamePanelControl::InitializeComponent() {
    Aero::GUI::LoadComponent(this, Aero::Uri("assets/ui/glue/CustomGamePanel.xaml"));
}

GlueOptionsPanelControl::GlueOptionsPanelControl() {
    InitializeComponent();
}

void GlueOptionsPanelControl::InitializeComponent() {
    Aero::GUI::LoadComponent(this, Aero::Uri("assets/ui/glue/OptionsPanel.xaml"));
}

GlueBattleNetPanelControl::GlueBattleNetPanelControl() {
    InitializeComponent();
}

void GlueBattleNetPanelControl::InitializeComponent() {
    Aero::GUI::LoadComponent(this, Aero::Uri("assets/ui/glue/BattleNetPanel.xaml"));
}

GlueLocalAreaNetworkPanelControl::GlueLocalAreaNetworkPanelControl() {
    InitializeComponent();
}

void GlueLocalAreaNetworkPanelControl::InitializeComponent() {
    Aero::GUI::LoadComponent(this, Aero::Uri("assets/ui/glue/LocalAreaNetworkPanel.xaml"));
}

NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(MainMenuPanelControl, "War3UI.MainMenuPanelControl")
{
}

NS_IMPLEMENT_REFLECTION(MainMenuBackdropControl, "War3UI.MainMenuBackdropControl")
{
}

NS_IMPLEMENT_REFLECTION(SinglePlayerPanelControl, "War3UI.SinglePlayerPanelControl")
{
}

NS_IMPLEMENT_REFLECTION(CampaignPanelControl, "War3UI.CampaignPanelControl")
{
}

NS_IMPLEMENT_REFLECTION(CustomGamePanelControl, "War3UI.CustomGamePanelControl")
{
}

NS_IMPLEMENT_REFLECTION(GlueOptionsPanelControl, "War3UI.GlueOptionsPanelControl")
{
}

NS_IMPLEMENT_REFLECTION(GlueBattleNetPanelControl, "War3UI.GlueBattleNetPanelControl")
{
}

NS_IMPLEMENT_REFLECTION(GlueLocalAreaNetworkPanelControl, "War3UI.GlueLocalAreaNetworkPanelControl")
{
}

NS_END_COLD_REGION

} // namespace War3UI
