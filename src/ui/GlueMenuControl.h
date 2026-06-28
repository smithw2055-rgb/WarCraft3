#pragma once

#include "MainUI.h"
#include "GlueMenuTypes.h"

#include <NsCore/Noesis.h>
#include <NsGui/UserControl.h>
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Noesis {
class Border;
class Brush;
class FrameworkElement;
class TextBlock;
}

namespace War3UI {

class GlueMenuControl final : public Noesis::UserControl {
public:
    static constexpr int kActionCount = 75;

    using RectResolver = std::function<ScreenRect(Noesis::FrameworkElement*)>;

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
        SinglePlayer,
        Campaign,
        MissionSelect,
        CustomGame,
    };

    const char* PageLabel() const;
    void SelectCampaign(int index);
    void ApplyCampaignData();

    Noesis::FrameworkElement* nightElfBackdropLayer_ = nullptr;
    Noesis::FrameworkElement* mainMenuFrame_ = nullptr;
    Noesis::FrameworkElement* singlePlayerMenu_ = nullptr;
    Noesis::FrameworkElement* campaignMenu_ = nullptr;
    Noesis::FrameworkElement* campaignSelectFrame_ = nullptr;
    Noesis::FrameworkElement* campaignRootMenuUI_ = nullptr;
    Noesis::FrameworkElement* campaignMissionSelectUI_ = nullptr;
    std::array<Noesis::FrameworkElement*, 4> campaignBackdropLayers_{};
    Noesis::TextBlock* campaignSummaryHeaderText_ = nullptr;
    Noesis::TextBlock* campaignSummaryNameText_ = nullptr;
    Noesis::TextBlock* campaignSummaryMissionCountText_ = nullptr;
    Noesis::TextBlock* campaignSummaryMapText_ = nullptr;
    Noesis::TextBlock* campaignMissionHeaderText_ = nullptr;
    Noesis::TextBlock* campaignMissionNameText_ = nullptr;
    Noesis::TextBlock* campaignMissionMapText_ = nullptr;
    std::array<Noesis::TextBlock*, 5> campaignButtonTexts_{};
    std::array<Noesis::TextBlock*, 5> campaignButtonNameTexts_{};
    std::array<Noesis::FrameworkElement*, 5> campaignButtonSelected_{};
    std::array<Noesis::TextBlock*, 12> missionButtonTexts_{};
    std::array<Noesis::FrameworkElement*, 12> missionButtonSelected_{};
    Noesis::FrameworkElement* customGameMenu_ = nullptr;
    Noesis::FrameworkElement* customGameTeamSetupPane_ = nullptr;
    Noesis::FrameworkElement* customGameMapInfoPane_ = nullptr;
    Noesis::FrameworkElement* customGameAdvancedOptionsPane_ = nullptr;
    Noesis::TextBlock* selectedMapText_ = nullptr;
    Noesis::TextBlock* selectedMapPlayersText_ = nullptr;
    Noesis::TextBlock* selectedMapMaxPlayersText_ = nullptr;
    Noesis::TextBlock* selectedMapSizeText_ = nullptr;
    Noesis::TextBlock* selectedMapTilesetText_ = nullptr;
    Noesis::TextBlock* selectedMapAuthorText_ = nullptr;
    Noesis::TextBlock* selectedMapPageText_ = nullptr;
    Noesis::TextBlock* selectedMapPathText_ = nullptr;
    Noesis::TextBlock* selectedMapDescriptionText_ = nullptr;
    std::array<Noesis::FrameworkElement*, 10> selectedMapPreviewImages_{};
    Noesis::TextBlock* teamSetupSummaryText_ = nullptr;
    Noesis::TextBlock* teamsTogetherToggleText_ = nullptr;
    Noesis::TextBlock* fixedTeamsToggleText_ = nullptr;
    Noesis::TextBlock* fullSharedControlToggleText_ = nullptr;
    static constexpr int kPlayerSlotCount = 12;
    std::array<Noesis::TextBlock*, 10> mapItemTexts_{};
    std::array<Noesis::FrameworkElement*, 10> mapItemSelected_{};
    std::array<Noesis::FrameworkElement*, kPlayerSlotCount> playerSlotRows_{};
    std::array<Noesis::TextBlock*, kPlayerSlotCount> playerSlotNameTexts_{};
    std::array<Noesis::TextBlock*, kPlayerSlotCount> playerSlotRaceTexts_{};
    std::array<Noesis::TextBlock*, kPlayerSlotCount> playerSlotTeamTexts_{};
    std::array<Noesis::Border*, kPlayerSlotCount> playerSlotColorButtonValues_{};
    Noesis::TextBlock* stateText_ = nullptr;
    std::array<Noesis::FrameworkElement*, kActionCount> buttons_{};
    std::array<Noesis::FrameworkElement*, kActionCount> buttonHighlights_{};
    std::array<Noesis::FrameworkElement*, kActionCount> buttonPressed_{};
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
        "Small island duel map. Map preview and W3I details pending.",
        "Classic four-player melee layout. Map preview and slots pending.",
        "Large forest melee layout. Map preview and slots pending.",
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
    bool customGameMapInfoOpen_ = false;
    bool customGameAdvancedOptionsOpen_ = false;
    bool advancedTeamsTogether_ = true;
    bool advancedFixedTeams_ = false;
    bool advancedFullSharedControl_ = false;
    int selectedMapIndex_ = 0;
    int mapListOffset_ = 0;
    int selectedCampaignIndex_ = 0;
    int selectedCampaignMissionIndex_ = 0;
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

