#pragma once

#include "MainUI.h"
#include "GlueMenuTypes.h"

#include <NsCore/Aero.h>
#include <NsGui/UserControl.h>
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Aero {
class Border;
class Brush;
class FrameworkElement;
class TextBlock;
}

namespace War3UI {

class GlueMenuControl final : public Aero::UserControl {
public:
    static constexpr int kActionCount = 92;

    using RectResolver = std::function<ScreenRect(Aero::FrameworkElement*)>;

    void CacheElements();
    void LogMissingElements() const;
    void CacheRects(const RectResolver& resolveRect);
    void ApplyState();
    void SetMaps(const std::vector<GlueMapListItem>& maps);
    void SetCampaigns(const std::vector<GlueCampaignListItem>& campaigns);

    bool IsActionVisible(int action) const;
    int HitButton(float x, float y) const;
    void SetHoveredButton(int action);
    void SetPressedButton(int action);
    void HandleAction(int action);
    void OpenDebugPage(const std::string& pageName);
    bool InvokeActionForDebug(const std::string& actionName);

private:
    enum class Page : uint8_t {
        MainMenu,
        BattleNet,
        LocalAreaNetwork,
        SinglePlayer,
        Campaign,
        MissionSelect,
        CustomGame,
        Options,
    };

    const char* PageLabel() const;
    void SelectCampaign(int index);
    void ApplyCampaignData();

    Aero::FrameworkElement* nightElfBackdropLayer_ = nullptr;
    Aero::FrameworkElement* mainMenuBackdropFrame_ = nullptr;
    Aero::FrameworkElement* mainMenuFrame_ = nullptr;
    Aero::FrameworkElement* quitConfirmDialog_ = nullptr;
    Aero::FrameworkElement* battleNetMenu_ = nullptr;
    Aero::FrameworkElement* localAreaNetworkMenu_ = nullptr;
    Aero::FrameworkElement* singlePlayerMenu_ = nullptr;
    Aero::FrameworkElement* singlePlayerMainPanel_ = nullptr;
    Aero::FrameworkElement* profilePanel_ = nullptr;
    Aero::FrameworkElement* campaignMenu_ = nullptr;
    Aero::FrameworkElement* campaignSelectFrame_ = nullptr;
    Aero::FrameworkElement* campaignRootMenuUI_ = nullptr;
    Aero::FrameworkElement* campaignMissionSelectUI_ = nullptr;
    std::array<Aero::FrameworkElement*, 4> campaignBackdropLayers_{};
    Aero::TextBlock* campaignSummaryHeaderText_ = nullptr;
    Aero::TextBlock* campaignSummaryNameText_ = nullptr;
    Aero::TextBlock* campaignSummaryMissionCountText_ = nullptr;
    Aero::TextBlock* campaignSummaryMapText_ = nullptr;
    Aero::TextBlock* campaignMissionHeaderText_ = nullptr;
    Aero::TextBlock* campaignMissionNameText_ = nullptr;
    Aero::TextBlock* campaignMissionMapText_ = nullptr;
    std::array<Aero::TextBlock*, 5> campaignButtonTexts_{};
    std::array<Aero::TextBlock*, 5> campaignButtonNameTexts_{};
    std::array<Aero::FrameworkElement*, 5> campaignButtonSelected_{};
    std::array<Aero::TextBlock*, 12> missionButtonTexts_{};
    std::array<Aero::FrameworkElement*, 12> missionButtonSelected_{};
    Aero::FrameworkElement* customGameMenu_ = nullptr;
    Aero::FrameworkElement* optionsMenu_ = nullptr;
    Aero::FrameworkElement* optionsListPanel_ = nullptr;
    Aero::FrameworkElement* optionsListBottomPanel_ = nullptr;
    Aero::FrameworkElement* optionsDetailBottomPanel_ = nullptr;
    std::array<Aero::FrameworkElement*, 4> optionsPanels_{};
    std::array<Aero::FrameworkElement*, 4> optionsSelected_{};
    Aero::FrameworkElement* customGameTeamSetupPane_ = nullptr;
    Aero::FrameworkElement* customGameMapInfoPane_ = nullptr;
    Aero::FrameworkElement* customGameAdvancedOptionsPane_ = nullptr;
    Aero::TextBlock* selectedMapText_ = nullptr;
    Aero::TextBlock* selectedMapPlayersText_ = nullptr;
    Aero::TextBlock* selectedMapMaxPlayersText_ = nullptr;
    Aero::TextBlock* selectedMapSizeText_ = nullptr;
    Aero::TextBlock* selectedMapTilesetText_ = nullptr;
    Aero::TextBlock* selectedMapAuthorText_ = nullptr;
    Aero::TextBlock* selectedMapPageText_ = nullptr;
    Aero::TextBlock* selectedMapPathText_ = nullptr;
    Aero::TextBlock* selectedMapDescriptionText_ = nullptr;
    std::array<Aero::FrameworkElement*, 10> selectedMapPreviewImages_{};
    Aero::TextBlock* teamSetupSummaryText_ = nullptr;
    Aero::TextBlock* teamsTogetherToggleText_ = nullptr;
    Aero::TextBlock* fixedTeamsToggleText_ = nullptr;
    Aero::TextBlock* fullSharedControlToggleText_ = nullptr;
    static constexpr int kPlayerSlotCount = 12;
    std::array<Aero::TextBlock*, 10> mapItemTexts_{};
    std::array<Aero::FrameworkElement*, 10> mapItemSelected_{};
    std::array<Aero::FrameworkElement*, kPlayerSlotCount> playerSlotRows_{};
    std::array<Aero::TextBlock*, kPlayerSlotCount> playerSlotNameTexts_{};
    std::array<Aero::TextBlock*, kPlayerSlotCount> playerSlotRaceTexts_{};
    std::array<Aero::TextBlock*, kPlayerSlotCount> playerSlotTeamTexts_{};
    std::array<Aero::Border*, kPlayerSlotCount> playerSlotColorButtonValues_{};
    Aero::TextBlock* stateText_ = nullptr;
    std::array<Aero::FrameworkElement*, kActionCount> buttons_{};
    std::array<Aero::FrameworkElement*, kActionCount> buttonHighlights_{};
    std::array<Aero::FrameworkElement*, kActionCount> buttonPressed_{};
    std::array<ScreenRect, kActionCount> buttonRects_{};
    std::array<std::string, 10> mapNames_{{
        "(2)EchoIsles.w3x", "(4)LostTemple.w3m", "(6)GnollWood.w3x",
        "", "", "", "", "", "", "",
    }};
    std::array<std::string, 10> mapPaths_{{
        "Maps/FrozenThrone/(2)EchoIsles.w3x", "Maps/FrozenThrone/(4)LostTemple.w3m", "Maps/FrozenThrone/(6)GnollWood.w3x",
        "", "", "", "", "", "", "",
    }};
    std::array<std::string, 10> mapLabels_{{
        "Echo Isles", "Lost Temple", "Gnoll Wood",
        "", "", "", "", "", "", "",
    }};
    std::array<std::string, 10> mapPlayers_{{
        "Suggested Players: 2", "Suggested Players: 4", "Suggested Players: 6",
        "", "", "", "", "", "", "",
    }};
    std::array<std::string, 10> mapAuthors_{{
        "Unknown", "Unknown", "Unknown",
        "", "", "", "", "", "", "",
    }};
    std::array<std::string, 10> mapSizes_{{
        "Unknown", "Unknown", "Unknown",
        "", "", "", "", "", "", "",
    }};
    std::array<std::string, 10> mapTilesets_{{
        "Unknown", "Unknown", "Unknown",
        "", "", "", "", "", "", "",
    }};
    std::array<std::string, 10> mapDescriptions_{{
        "Small island duel map with map preview, player slots, and tileset details.",
        "Classic four-player melee layout with map preview and player slots.",
        "Large forest melee layout with map preview and player slots.",
        "", "", "", "", "", "", "",
    }};
    std::array<int, 10> mapPlayerSlotCounts_{2, 4, 6, 0, 0, 0, 0, 0, 0, 0};
    int visibleMapCount_ = 3;
    std::vector<std::string> allMapNames_;
    std::vector<std::string> allMapPaths_;
    std::vector<std::string> allMapLabels_;
    std::vector<std::string> allMapPlayers_;
    std::vector<std::string> allMapAuthors_;
    std::vector<std::string> allMapSizes_;
    std::vector<std::string> allMapTilesets_;
    std::vector<std::string> allMapDescriptions_;
    std::vector<int> allMapPlayerSlotCounts_;
    std::vector<GlueCampaignListItem> campaigns_;

    Page page_ = Page::MainMenu;
    bool quitConfirmOpen_ = false;
    bool singlePlayerProfileOpen_ = false;
    bool customGameMapInfoOpen_ = false;
    bool customGameAdvancedOptionsOpen_ = false;
    bool advancedTeamsTogether_ = true;
    bool advancedFixedTeams_ = false;
    bool advancedFullSharedControl_ = false;
    int selectedMapIndex_ = 0;
    int mapListOffset_ = 0;
    int selectedCampaignIndex_ = 0;
    int selectedCampaignMissionIndex_ = 0;
    int selectedOptionsTab_ = -1;
    int loggedCampaignBackdropIndex_ = -1;
    std::string loggedCampaignBackdropId_;
    std::array<int, kPlayerSlotCount> playerSlotTypeIndices_{0, 3, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0};
    std::array<int, kPlayerSlotCount> playerSlotRaceIndices_{1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::array<int, kPlayerSlotCount> playerSlotColorIndices_{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    int hoveredAction_ = -1;
    int pressedAction_ = -1;

    NS_DECLARE_REFLECTION(GlueMenuControl, UserControl)
};

} // namespace War3UI

