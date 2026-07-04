#include "GlueMenuControl.h"

#include <NsCore/DynamicCast.h>
#include <NsCore/ReflectionImplement.h>
#include <NsGui/Border.h>
#include <NsGui/Brush.h>
#include <NsGui/Enums.h>
#include <NsGui/FrameworkElement.h>
#include <NsGui/TextBlock.h>
#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <vector>

#ifdef FindResource
#undef FindResource
#endif

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

enum GlueMenuAction {
    kGlueMenuSinglePlayer,
    kGlueMenuBattleNet,
    kGlueMenuLocalAreaNetwork,
    kGlueMenuOptions,
    kGlueMenuCredits,
    kGlueMenuExit,
    kGlueMenuQuitConfirmOK,
    kGlueMenuQuitConfirmCancel,
    kGlueMenuProfile,
    kGlueMenuLoadSaved,
    kGlueMenuViewReplay,
    kGlueMenuCustomCampaign,
    kGlueMenuCampaign,
    kGlueMenuCustomGame,
    kGlueMenuCancel,
    kGlueMenuBack,
    kGlueMenuCampaignPrologue,
    kGlueMenuCampaignHuman,
    kGlueMenuCampaignUndead,
    kGlueMenuCampaignOrc,
    kGlueMenuCampaignNightElf,
    kGlueMenuCampaignMissionOne,
    kGlueMenuCampaignMissionTwo,
    kGlueMenuCampaignMission3,
    kGlueMenuCampaignMission4,
    kGlueMenuCampaignMission5,
    kGlueMenuCampaignMission6,
    kGlueMenuCampaignMission7,
    kGlueMenuCampaignMission8,
    kGlueMenuCampaignMission9,
    kGlueMenuCampaignMission10,
    kGlueMenuCampaignMission11,
    kGlueMenuCampaignMission12,
    kGlueMenuMissionBack,
    kGlueMenuPlayGame,
    kGlueMenuCustomGameCancel,
    kGlueMenuMapInfo,
    kGlueMenuAdvancedOptions,
    kGlueMenuTeamsTogetherToggle,
    kGlueMenuFixedTeamsToggle,
    kGlueMenuFullSharedControlToggle,
    kGlueMenuMapItem0,
    kGlueMenuMapItem1,
    kGlueMenuMapItem2,
    kGlueMenuMapItem3,
    kGlueMenuMapItem4,
    kGlueMenuMapItem5,
    kGlueMenuMapItem6,
    kGlueMenuMapItem7,
    kGlueMenuMapItem8,
    kGlueMenuMapItem9,
    kGlueMenuMapPageUp,
    kGlueMenuMapPageDown,
    kGlueMenuPlayerSlot0Name,
    kGlueMenuPlayerSlot1Name,
    kGlueMenuPlayerSlot2Name,
    kGlueMenuPlayerSlot0Race,
    kGlueMenuPlayerSlot1Race,
    kGlueMenuPlayerSlot2Race,
    kGlueMenuPlayerSlot3Name,
    kGlueMenuPlayerSlot4Name,
    kGlueMenuPlayerSlot5Name,
    kGlueMenuPlayerSlot6Name,
    kGlueMenuPlayerSlot7Name,
    kGlueMenuPlayerSlot8Name,
    kGlueMenuPlayerSlot9Name,
    kGlueMenuPlayerSlot10Name,
    kGlueMenuPlayerSlot11Name,
    kGlueMenuPlayerSlot3Race,
    kGlueMenuPlayerSlot4Race,
    kGlueMenuPlayerSlot5Race,
    kGlueMenuPlayerSlot6Race,
    kGlueMenuPlayerSlot7Race,
    kGlueMenuPlayerSlot8Race,
    kGlueMenuPlayerSlot9Race,
    kGlueMenuPlayerSlot10Race,
    kGlueMenuPlayerSlot11Race,
    kGlueMenuOptionsGameplay,
    kGlueMenuOptionsVideo,
    kGlueMenuOptionsSound,
    kGlueMenuOptionsNetwork,
    kGlueMenuOptionsBack,
    kGlueMenuOptionsOK,
    kGlueMenuOptionsCancel,
    kGlueMenuBattleNetLogon,
    kGlueMenuBattleNetCreateAccount,
    kGlueMenuBattleNetRecoverPassword,
    kGlueMenuQuitBattleNet,
    kGlueMenuLanRefresh,
    kGlueMenuLanCreateGame,
    kGlueMenuLanJoinGame,
    kGlueMenuQuitLocalAreaNetwork,
};

constexpr int kGlueMenuActionElementNameCount = 92;
constexpr int kGlueMapSlotCount = 10;
constexpr int kGluePlayerSlotCount = 12;
constexpr int kGlueCampaignButtonCount = 5;
constexpr int kGlueMissionButtonCount = 12;
constexpr int kGlueOptionsTabCount = 4;

const char* GlueMenuActionElementName(int action) {
    static constexpr const char* kNames[] = {
        "SinglePlayerButton",
        "BattleNetButton",
        "LocalAreaNetworkButton",
        "OptionsButton",
        "CreditsButton",
        "ExitButton",
        "QuitConfirmOKButton",
        "QuitConfirmCancelButton",
        "ProfileButton",
        "LoadSavedButton",
        "ViewReplayButton",
        "CustomCampaignButton",
        "CampaignButton",
        "CustomGameButton",
        "CancelButton",
        "BackButton",
        "CampaignPrologueButton",
        "CampaignHumanButton",
        "CampaignUndeadButton",
        "CampaignOrcButton",
        "CampaignNightElfButton",
        "CampaignMissionOneButton",
        "CampaignMissionTwoButton",
        "CampaignMission3Button",
        "CampaignMission4Button",
        "CampaignMission5Button",
        "CampaignMission6Button",
        "CampaignMission7Button",
        "CampaignMission8Button",
        "CampaignMission9Button",
        "CampaignMission10Button",
        "CampaignMission11Button",
        "CampaignMission12Button",
        "MissionBackButton",
        "PlayGameButton",
        "CustomGameCancelButton",
        "MapInfoButton",
        "AdvancedOptionsButton",
        "TeamsTogetherToggleButton",
        "FixedTeamsToggleButton",
        "FullSharedControlToggleButton",
        "MapItem0Button",
        "MapItem1Button",
        "MapItem2Button",
        "MapItem3Button",
        "MapItem4Button",
        "MapItem5Button",
        "MapItem6Button",
        "MapItem7Button",
        "MapItem8Button",
        "MapItem9Button",
        "MapPageUpButton",
        "MapPageDownButton",
        "PlayerSlot0NameMenu",
        "PlayerSlot1NameMenu",
        "PlayerSlot2NameMenu",
        "PlayerSlot0RaceMenu",
        "PlayerSlot1RaceMenu",
        "PlayerSlot2RaceMenu",
        "PlayerSlot3NameMenu",
        "PlayerSlot4NameMenu",
        "PlayerSlot5NameMenu",
        "PlayerSlot6NameMenu",
        "PlayerSlot7NameMenu",
        "PlayerSlot8NameMenu",
        "PlayerSlot9NameMenu",
        "PlayerSlot10NameMenu",
        "PlayerSlot11NameMenu",
        "PlayerSlot3RaceMenu",
        "PlayerSlot4RaceMenu",
        "PlayerSlot5RaceMenu",
        "PlayerSlot6RaceMenu",
        "PlayerSlot7RaceMenu",
        "PlayerSlot8RaceMenu",
        "PlayerSlot9RaceMenu",
        "PlayerSlot10RaceMenu",
        "PlayerSlot11RaceMenu",
        "OptionsGameplayButton",
        "OptionsVideoButton",
        "OptionsSoundButton",
        "OptionsNetworkButton",
        "OptionsBackButton",
        "OptionsOKButton",
        "OptionsCancelButton",
        "BattleNetLogonButton",
        "BattleNetCreateAccountButton",
        "BattleNetRecoverPasswordButton",
        "QuitBattleNetButton",
        "LanRefreshButton",
        "LanCreateGameButton",
        "LanJoinGameButton",
        "QuitLocalAreaNetworkButton",
    };
    if (action < 0 || action >= (int)(sizeof(kNames) / sizeof(kNames[0]))) return "";
    return kNames[action];
}

const char* GlueMenuActionPageName(int action) {
    switch (action) {
    case kGlueMenuSinglePlayer:
    case kGlueMenuBattleNet:
    case kGlueMenuLocalAreaNetwork:
    case kGlueMenuOptions:
    case kGlueMenuCredits:
    case kGlueMenuExit:
    case kGlueMenuQuitConfirmOK:
    case kGlueMenuQuitConfirmCancel:
        return "MainMenuFrame";
    case kGlueMenuProfile:
    case kGlueMenuCampaign:
    case kGlueMenuLoadSaved:
    case kGlueMenuViewReplay:
    case kGlueMenuCustomCampaign:
    case kGlueMenuCustomGame:
    case kGlueMenuCancel:
        return "SinglePlayerMenu";
    case kGlueMenuBack:
    case kGlueMenuCampaignPrologue:
    case kGlueMenuCampaignHuman:
    case kGlueMenuCampaignUndead:
    case kGlueMenuCampaignOrc:
    case kGlueMenuCampaignNightElf:
        return "CampaignMenu";
    case kGlueMenuCampaignMissionOne:
    case kGlueMenuCampaignMissionTwo:
    case kGlueMenuCampaignMission3:
    case kGlueMenuCampaignMission4:
    case kGlueMenuCampaignMission5:
    case kGlueMenuCampaignMission6:
    case kGlueMenuCampaignMission7:
    case kGlueMenuCampaignMission8:
    case kGlueMenuCampaignMission9:
    case kGlueMenuCampaignMission10:
    case kGlueMenuCampaignMission11:
    case kGlueMenuCampaignMission12:
    case kGlueMenuMissionBack:
        return "MissionSelect";
    case kGlueMenuPlayGame:
    case kGlueMenuCustomGameCancel:
    case kGlueMenuMapInfo:
    case kGlueMenuAdvancedOptions:
    case kGlueMenuTeamsTogetherToggle:
    case kGlueMenuFixedTeamsToggle:
    case kGlueMenuFullSharedControlToggle:
    case kGlueMenuMapItem0:
    case kGlueMenuMapItem1:
    case kGlueMenuMapItem2:
    case kGlueMenuMapItem3:
    case kGlueMenuMapItem4:
    case kGlueMenuMapItem5:
    case kGlueMenuMapItem6:
    case kGlueMenuMapItem7:
    case kGlueMenuMapItem8:
    case kGlueMenuMapItem9:
    case kGlueMenuMapPageUp:
    case kGlueMenuMapPageDown:
    case kGlueMenuPlayerSlot0Name:
    case kGlueMenuPlayerSlot1Name:
    case kGlueMenuPlayerSlot2Name:
    case kGlueMenuPlayerSlot0Race:
    case kGlueMenuPlayerSlot1Race:
    case kGlueMenuPlayerSlot2Race:
    case kGlueMenuPlayerSlot3Name:
    case kGlueMenuPlayerSlot4Name:
    case kGlueMenuPlayerSlot5Name:
    case kGlueMenuPlayerSlot6Name:
    case kGlueMenuPlayerSlot7Name:
    case kGlueMenuPlayerSlot8Name:
    case kGlueMenuPlayerSlot9Name:
    case kGlueMenuPlayerSlot10Name:
    case kGlueMenuPlayerSlot11Name:
    case kGlueMenuPlayerSlot3Race:
    case kGlueMenuPlayerSlot4Race:
    case kGlueMenuPlayerSlot5Race:
    case kGlueMenuPlayerSlot6Race:
    case kGlueMenuPlayerSlot7Race:
    case kGlueMenuPlayerSlot8Race:
    case kGlueMenuPlayerSlot9Race:
    case kGlueMenuPlayerSlot10Race:
    case kGlueMenuPlayerSlot11Race:
        return "CustomGameMenu";
    case kGlueMenuOptionsGameplay:
    case kGlueMenuOptionsVideo:
    case kGlueMenuOptionsSound:
    case kGlueMenuOptionsNetwork:
    case kGlueMenuOptionsBack:
    case kGlueMenuOptionsOK:
    case kGlueMenuOptionsCancel:
        return "OptionsMenu";
    case kGlueMenuBattleNetLogon:
    case kGlueMenuBattleNetCreateAccount:
    case kGlueMenuBattleNetRecoverPassword:
    case kGlueMenuQuitBattleNet:
        return "BattleNetMenu";
    case kGlueMenuLanRefresh:
    case kGlueMenuLanCreateGame:
    case kGlueMenuLanJoinGame:
    case kGlueMenuQuitLocalAreaNetwork:
        return "LocalAreaNetworkMenu";
    default:
        return "";
    }
}

std::string NormalizeDebugKey(std::string key) {
    std::string result;
    result.reserve(key.size());
    for (unsigned char c : key) {
        if (c == '_' || c == '-' || c == ' ') continue;
        result.push_back((char)std::tolower(c));
    }
    return result;
}

int GlueMenuActionForDebugName(const std::string& name) {
    const std::string key = NormalizeDebugKey(name);
    for (int i = 0; i < kGlueMenuActionElementNameCount; ++i) {
        if (key == NormalizeDebugKey(GlueMenuActionElementName(i))) {
            return i;
        }
    }
    if (key == "singleplayer") return kGlueMenuSinglePlayer;
    if (key == "battlenet" || key == "battle.net" || key == "battle" || key == "bnet") return kGlueMenuBattleNet;
    if (key == "localareanetwork" || key == "lan" || key == "localnetwork") return kGlueMenuLocalAreaNetwork;
    if (key == "options") return kGlueMenuOptions;
    if (key == "credits") return kGlueMenuCredits;
    if (key == "quitok" || key == "exitok" || key == "confirmquit" || key == "confirmexit") return kGlueMenuQuitConfirmOK;
    if (key == "quitcancel" || key == "exitcancel" || key == "cancelquit" || key == "cancelexit") return kGlueMenuQuitConfirmCancel;
    if (key == "exit" || key == "quit") return kGlueMenuExit;
    if (key == "profile") return kGlueMenuProfile;
    if (key == "loadsaved" || key == "loadsavedgame" || key == "loadgame") return kGlueMenuLoadSaved;
    if (key == "viewreplay" || key == "replay") return kGlueMenuViewReplay;
    if (key == "customcampaign") return kGlueMenuCustomCampaign;
    if (key == "customgame" || key == "customgames" || key == "skirmish") return kGlueMenuCustomGame;
    if (key == "campaign") return kGlueMenuCampaign;
    if (key == "prologue" || key == "tutorial" || key == "campaign0") return kGlueMenuCampaignPrologue;
    if (key == "human" || key == "campaign1") return kGlueMenuCampaignHuman;
    if (key == "undead" || key == "campaign2") return kGlueMenuCampaignUndead;
    if (key == "orc" || key == "campaign3") return kGlueMenuCampaignOrc;
    if (key == "nightelf" || key == "campaign4") return kGlueMenuCampaignNightElf;
    if (key == "mission1" || key == "missionone") return kGlueMenuCampaignMissionOne;
    if (key == "mission2" || key == "missiontwo") return kGlueMenuCampaignMissionTwo;
    if (key == "mission3") return kGlueMenuCampaignMission3;
    if (key == "mission4") return kGlueMenuCampaignMission4;
    if (key == "mission5") return kGlueMenuCampaignMission5;
    if (key == "mission6") return kGlueMenuCampaignMission6;
    if (key == "mission7") return kGlueMenuCampaignMission7;
    if (key == "mission8") return kGlueMenuCampaignMission8;
    if (key == "mission9") return kGlueMenuCampaignMission9;
    if (key == "mission10") return kGlueMenuCampaignMission10;
    if (key == "mission11") return kGlueMenuCampaignMission11;
    if (key == "mission12") return kGlueMenuCampaignMission12;
    if (key == "missionback") return kGlueMenuMissionBack;
    if (key == "cancel") return kGlueMenuCancel;
    if (key == "back") return kGlueMenuBack;
    if (key == "mapinfo" || key == "info") return kGlueMenuMapInfo;
    if (key == "advanced" || key == "advancedoptions") return kGlueMenuAdvancedOptions;
    if (key == "teamstogether" || key == "teamstogethertoggle") return kGlueMenuTeamsTogetherToggle;
    if (key == "fixedteams" || key == "fixedteamstoggle") return kGlueMenuFixedTeamsToggle;
    if (key == "fullsharedcontrol" || key == "sharedcontrol" || key == "fullshared") return kGlueMenuFullSharedControlToggle;
    if (key == "play" || key == "playgame") return kGlueMenuPlayGame;
    if (key == "skirmishcancel" || key == "customgamecancel") return kGlueMenuCustomGameCancel;
    if (key == "map0" || key == "mapitem0") return kGlueMenuMapItem0;
    if (key == "map1" || key == "mapitem1") return kGlueMenuMapItem1;
    if (key == "map2" || key == "mapitem2") return kGlueMenuMapItem2;
    if (key == "map3" || key == "mapitem3") return kGlueMenuMapItem3;
    if (key == "map4" || key == "mapitem4") return kGlueMenuMapItem4;
    if (key == "map5" || key == "mapitem5") return kGlueMenuMapItem5;
    if (key == "map6" || key == "mapitem6") return kGlueMenuMapItem6;
    if (key == "map7" || key == "mapitem7") return kGlueMenuMapItem7;
    if (key == "map8" || key == "mapitem8") return kGlueMenuMapItem8;
    if (key == "map9" || key == "mapitem9") return kGlueMenuMapItem9;
    if (key == "mappageup" || key == "mapprev" || key == "mapprevious") return kGlueMenuMapPageUp;
    if (key == "mappagedown" || key == "mapnext") return kGlueMenuMapPageDown;
    if (key == "slot0" || key == "slot0name") return kGlueMenuPlayerSlot0Name;
    if (key == "slot1" || key == "slot1name") return kGlueMenuPlayerSlot1Name;
    if (key == "slot2" || key == "slot2name") return kGlueMenuPlayerSlot2Name;
    if (key == "slot0race") return kGlueMenuPlayerSlot0Race;
    if (key == "slot1race") return kGlueMenuPlayerSlot1Race;
    if (key == "slot2race") return kGlueMenuPlayerSlot2Race;
    if (key == "slot3" || key == "slot3name") return kGlueMenuPlayerSlot3Name;
    if (key == "slot4" || key == "slot4name") return kGlueMenuPlayerSlot4Name;
    if (key == "slot5" || key == "slot5name") return kGlueMenuPlayerSlot5Name;
    if (key == "slot6" || key == "slot6name") return kGlueMenuPlayerSlot6Name;
    if (key == "slot7" || key == "slot7name") return kGlueMenuPlayerSlot7Name;
    if (key == "slot8" || key == "slot8name") return kGlueMenuPlayerSlot8Name;
    if (key == "slot9" || key == "slot9name") return kGlueMenuPlayerSlot9Name;
    if (key == "slot10" || key == "slot10name") return kGlueMenuPlayerSlot10Name;
    if (key == "slot11" || key == "slot11name") return kGlueMenuPlayerSlot11Name;
    if (key == "slot3race") return kGlueMenuPlayerSlot3Race;
    if (key == "slot4race") return kGlueMenuPlayerSlot4Race;
    if (key == "slot5race") return kGlueMenuPlayerSlot5Race;
    if (key == "slot6race") return kGlueMenuPlayerSlot6Race;
    if (key == "slot7race") return kGlueMenuPlayerSlot7Race;
    if (key == "slot8race") return kGlueMenuPlayerSlot8Race;
    if (key == "slot9race") return kGlueMenuPlayerSlot9Race;
    if (key == "slot10race") return kGlueMenuPlayerSlot10Race;
    if (key == "slot11race") return kGlueMenuPlayerSlot11Race;
    if (key == "optionsgameplay" || key == "gameplay") return kGlueMenuOptionsGameplay;
    if (key == "optionsvideo" || key == "video") return kGlueMenuOptionsVideo;
    if (key == "optionssound" || key == "sound" || key == "audio") return kGlueMenuOptionsSound;
    if (key == "optionsnetwork" || key == "network") return kGlueMenuOptionsNetwork;
    if (key == "optionsback" || key == "optionsprevious" || key == "previousmenu") return kGlueMenuOptionsBack;
    if (key == "optionsok" || key == "ok") return kGlueMenuOptionsOK;
    if (key == "optionscancel") return kGlueMenuOptionsCancel;
    if (key == "battlenetlogon" || key == "logon" || key == "login") return kGlueMenuBattleNetLogon;
    if (key == "battlenetcreateaccount" || key == "createaccount") return kGlueMenuBattleNetCreateAccount;
    if (key == "battlenetrecoverpassword" || key == "recoverpassword") return kGlueMenuBattleNetRecoverPassword;
    if (key == "quitbattlenet" || key == "battlenetcancel" || key == "bnetcancel") return kGlueMenuQuitBattleNet;
    if (key == "lanrefresh" || key == "refreshlan") return kGlueMenuLanRefresh;
    if (key == "lancreategame" || key == "createlangame" || key == "lancreate") return kGlueMenuLanCreateGame;
    if (key == "lanjoingame" || key == "joinlangame" || key == "lanjoin") return kGlueMenuLanJoinGame;
    if (key == "quitlocalareanetwork" || key == "lancancel" || key == "quitlan") return kGlueMenuQuitLocalAreaNetwork;
    if (key == "main" || key == "mainmenu" || key == "mainmenuframe") return -2;
    if (key == "back") return kGlueMenuBack;
    if (key == "cancel") return kGlueMenuCancel;
    return -1;
}

constexpr const char* kGlueSlotTypeLabels[] = {
    "Open", "Closed", "Computer Easy", "Computer Normal", "Computer Insane",
};

constexpr const char* kGlueRaceLabels[] = {
    "Random", "Human", "Orc", "Undead", "Night Elf",
};

const char* ClampPick(const char* const* values, int count, int index) {
    index = std::clamp(index, 0, count - 1);
    return values[index];
}

const char* NonEmptyOr(const std::string& value, const char* fallback) {
    return value.empty() ? fallback : value.c_str();
}

int CampaignBackdropLayerIndex(const War3UI::GlueCampaignListItem* campaign, int fallbackIndex) {
    if (campaign) {
        const std::string id = NormalizeDebugKey(campaign->id);
        const std::string header = NormalizeDebugKey(campaign->header);
        const std::string name = NormalizeDebugKey(campaign->name);
        const std::string combined = id + header + name;
        if (combined.find("nightelf") != std::string::npos || combined.find("sentinel") != std::string::npos) return 0;
        if (combined.find("human") != std::string::npos || combined.find("alliance") != std::string::npos) return 1;
        if (combined.find("undead") != std::string::npos || combined.find("scourge") != std::string::npos) return 2;
        if (combined.find("orc") != std::string::npos || combined.find("durotar") != std::string::npos) return 3;
    }
    return std::clamp(fallbackIndex, 0, 3);
}

std::string CampaignButtonLabel(const War3UI::GlueCampaignListItem& campaign) {
    if (!campaign.header.empty()) return campaign.header;
    if (!campaign.name.empty()) return campaign.name;
    return campaign.id;
}

std::string MissionButtonLabel(const War3UI::GlueCampaignMission& mission, int index) {
    std::ostringstream ss;
    if (!mission.header.empty()) {
        ss << mission.header;
    } else {
        ss << (index + 1);
    }
    if (!mission.name.empty()) {
        ss << "  " << mission.name;
    }
    return ss.str();
}

std::string StemWithoutPlayerPrefix(std::string fileName) {
    const size_t dot = fileName.find_last_of('.');
    if (dot != std::string::npos) {
        fileName.resize(dot);
    }
    if (fileName.size() > 3 && fileName[0] == '(') {
        const size_t close = fileName.find(')');
        if (close != std::string::npos && close + 1 < fileName.size()) {
            fileName.erase(0, close + 1);
        }
    }
    std::string label;
    label.reserve(fileName.size() + 8);
    for (size_t i = 0; i < fileName.size(); ++i) {
        const unsigned char c = (unsigned char)fileName[i];
        if (i > 0 && std::isupper(c) && std::islower((unsigned char)fileName[i - 1])) {
            label.push_back(' ');
        }
        label.push_back((char)c);
    }
    return label.empty() ? fileName : label;
}

std::string SuggestedPlayersFromFileName(const std::string& fileName) {
    if (fileName.size() > 3 && fileName[0] == '(') {
        const size_t close = fileName.find(')');
        if (close != std::string::npos && close > 1) {
            return "Suggested Players: " + fileName.substr(1, close - 1);
        }
    }
    return "Suggested Players: Unknown";
}

int PlayerSlotsFromFileName(const std::string& fileName) {
    if (fileName.size() > 3 && fileName[0] == '(') {
        const size_t close = fileName.find(')');
        if (close != std::string::npos && close > 1) {
            return std::max(0, std::atoi(fileName.substr(1, close - 1).c_str()));
        }
    }
    return 0;
}

int VisiblePlayerSlotsForMap(int playerSlots) {
    if (playerSlots <= 0) return 3;
    return std::clamp(playerSlots, 1, kGluePlayerSlotCount);
}

std::string MapPageSummary(int selectedIndex, int totalMaps, int offset) {
    std::string pageText = "Map ";
    pageText += std::to_string(selectedIndex + 1);
    pageText += " of ";
    pageText += std::to_string(std::max(1, totalMaps));
    pageText += " Page ";
    pageText += std::to_string((offset / kGlueMapSlotCount) + 1);
    pageText += "/";
    pageText += std::to_string(std::max(1, (std::max(1, totalMaps) + kGlueMapSlotCount - 1) / kGlueMapSlotCount));
    return pageText;
}

std::string LabeledValue(const char* label, const std::string& value, const char* fallback = "Unknown") {
    std::string text = label;
    text += ": ";
    text += value.empty() ? fallback : value;
    return text;
}

std::string TeamSetupSummary(int slots, const std::string& mapSize, const std::string& tileset) {
    std::string text = "Slots: ";
    text += slots > 0 ? std::to_string(slots) : "Unknown";
    text += "  Map Size: ";
    text += mapSize.empty() ? "Unknown" : mapSize;
    text += "  Tileset: ";
    text += tileset.empty() ? "Unknown" : tileset;
    return text;
}

const char* OnOff(bool value) {
    return value ? "On" : "Off";
}

Aero::Brush* FindGlueTeamColorBrush(Aero::FrameworkElement* scope, int colorIndex) {
    if (!scope) return nullptr;
    char key[32];
    std::snprintf(key, sizeof(key), "GlueTeamColor%02dBrush", std::clamp(colorIndex, 0, 11));
    return Aero::DynamicCast<Aero::Brush*>(scope->FindResource(key));
}

int CampaignActionToIndex(int action) {
    if (action < kGlueMenuCampaignPrologue || action > kGlueMenuCampaignNightElf) return -1;
    return action - kGlueMenuCampaignPrologue;
}

int CampaignMissionActionToIndex(int action) {
    if (action < kGlueMenuCampaignMissionOne || action > kGlueMenuCampaignMission12) return -1;
    return action - kGlueMenuCampaignMissionOne;
}

int PlayerSlotNameActionToSlot(int action) {
    switch (action) {
    case kGlueMenuPlayerSlot0Name: return 0;
    case kGlueMenuPlayerSlot1Name: return 1;
    case kGlueMenuPlayerSlot2Name: return 2;
    case kGlueMenuPlayerSlot3Name: return 3;
    case kGlueMenuPlayerSlot4Name: return 4;
    case kGlueMenuPlayerSlot5Name: return 5;
    case kGlueMenuPlayerSlot6Name: return 6;
    case kGlueMenuPlayerSlot7Name: return 7;
    case kGlueMenuPlayerSlot8Name: return 8;
    case kGlueMenuPlayerSlot9Name: return 9;
    case kGlueMenuPlayerSlot10Name: return 10;
    case kGlueMenuPlayerSlot11Name: return 11;
    default: return -1;
    }
}

int PlayerSlotRaceActionToSlot(int action) {
    switch (action) {
    case kGlueMenuPlayerSlot0Race: return 0;
    case kGlueMenuPlayerSlot1Race: return 1;
    case kGlueMenuPlayerSlot2Race: return 2;
    case kGlueMenuPlayerSlot3Race: return 3;
    case kGlueMenuPlayerSlot4Race: return 4;
    case kGlueMenuPlayerSlot5Race: return 5;
    case kGlueMenuPlayerSlot6Race: return 6;
    case kGlueMenuPlayerSlot7Race: return 7;
    case kGlueMenuPlayerSlot8Race: return 8;
    case kGlueMenuPlayerSlot9Race: return 9;
    case kGlueMenuPlayerSlot10Race: return 10;
    case kGlueMenuPlayerSlot11Race: return 11;
    default: return -1;
    }
}

template<class T>
T* FindAs(Aero::FrameworkElement* root, const char* name) {
    return root ? Aero::DynamicCast<T*>(root->FindName(name)) : nullptr;
}

template<class T>
T* FindAsAny(Aero::FrameworkElement* primary,
             std::initializer_list<Aero::FrameworkElement*> secondaryRoots,
             const char* name) {
    if (T* found = FindAs<T>(primary, name)) {
        return found;
    }
    for (Aero::FrameworkElement* secondary : secondaryRoots) {
        if (T* found = FindAs<T>(secondary, name)) {
            return found;
        }
    }
    return nullptr;
}

void GlueDiagLog(const char* fmt, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    std::error_code ec;
    std::filesystem::path logDir = std::filesystem::path(PROJECT_SOURCE_DIR) / "Logs";
    std::filesystem::create_directories(logDir, ec);
    std::ofstream out(logDir / "aero-hud.log", std::ios::app);
    if (out) {
        out << buffer << '\n';
    }
}

} // namespace

namespace War3UI {

void GlueMenuControl::CacheElements() {
    nightElfBackdropLayer_ = FindAs<Aero::FrameworkElement>(this, "GlueNightElfBackdropLayer");
    mainMenuBackdropFrame_ = FindAs<Aero::FrameworkElement>(this, "MainMenuBackdropFrame");
    mainMenuFrame_ = FindAs<Aero::FrameworkElement>(this, "MainMenuFrame");
    quitConfirmDialog_ = FindAs<Aero::FrameworkElement>(mainMenuFrame_, "QuitConfirmDialog");
    battleNetMenu_ = FindAs<Aero::FrameworkElement>(this, "BattleNetMenu");
    localAreaNetworkMenu_ = FindAs<Aero::FrameworkElement>(this, "LocalAreaNetworkMenu");
    singlePlayerMenu_ = FindAs<Aero::FrameworkElement>(this, "SinglePlayerMenu");
    singlePlayerMainPanel_ = FindAs<Aero::FrameworkElement>(singlePlayerMenu_, "SinglePlayerMainPanel");
    profilePanel_ = FindAs<Aero::FrameworkElement>(singlePlayerMenu_, "ProfilePanel");
    campaignMenu_ = FindAs<Aero::FrameworkElement>(this, "CampaignMenu");
    campaignSelectFrame_ = FindAs<Aero::FrameworkElement>(campaignMenu_, "CampaignSelectFrame");
    campaignRootMenuUI_ = FindAs<Aero::FrameworkElement>(campaignMenu_, "CampaignRootMenuUI");
    campaignMissionSelectUI_ = FindAs<Aero::FrameworkElement>(campaignMenu_, "CampaignMissionSelectMenuUI");
    static constexpr const char* kCampaignBackdropLayerNames[] = {
        "CampaignBackdropNightElfLayer",
        "CampaignBackdropHumanLayer",
        "CampaignBackdropUndeadLayer",
        "CampaignBackdropOrcLayer",
    };
    for (int i = 0; i < 4; ++i) {
        campaignBackdropLayers_[i] = FindAs<Aero::FrameworkElement>(campaignMenu_, kCampaignBackdropLayerNames[i]);
    }
    campaignSummaryHeaderText_ = FindAs<Aero::TextBlock>(campaignMenu_, "CampaignSummaryHeaderText");
    campaignSummaryNameText_ = FindAs<Aero::TextBlock>(campaignMenu_, "CampaignSummaryNameText");
    campaignSummaryMissionCountText_ = FindAs<Aero::TextBlock>(campaignMenu_, "CampaignSummaryMissionCountText");
    campaignSummaryMapText_ = FindAs<Aero::TextBlock>(campaignMenu_, "CampaignSummaryMapText");
    campaignMissionHeaderText_ = FindAs<Aero::TextBlock>(campaignMenu_, "CampaignMissionHeaderText");
    campaignMissionNameText_ = FindAs<Aero::TextBlock>(campaignMenu_, "CampaignMissionNameText");
    campaignMissionMapText_ = FindAs<Aero::TextBlock>(campaignMenu_, "CampaignMissionMapText");
    static constexpr const char* kCampaignButtonTextNames[kGlueCampaignButtonCount] = {
        "CampaignPrologueButtonText",
        "CampaignHumanButtonText",
        "CampaignUndeadButtonText",
        "CampaignOrcButtonText",
        "CampaignNightElfButtonText",
    };
    static constexpr const char* kCampaignButtonNameTextNames[kGlueCampaignButtonCount] = {
        "CampaignPrologueNameText",
        "CampaignHumanNameText",
        "CampaignUndeadNameText",
        "CampaignOrcNameText",
        "CampaignNightElfNameText",
    };
    static constexpr const char* kCampaignButtonSelectedNames[kGlueCampaignButtonCount] = {
        "CampaignPrologueSelected",
        "CampaignHumanSelected",
        "CampaignUndeadSelected",
        "CampaignOrcSelected",
        "CampaignNightElfSelected",
    };
    for (int i = 0; i < kGlueCampaignButtonCount; ++i) {
        campaignButtonTexts_[i] = FindAs<Aero::TextBlock>(campaignMenu_, kCampaignButtonTextNames[i]);
        campaignButtonNameTexts_[i] = FindAs<Aero::TextBlock>(campaignMenu_, kCampaignButtonNameTextNames[i]);
        campaignButtonSelected_[i] = FindAs<Aero::FrameworkElement>(campaignMenu_, kCampaignButtonSelectedNames[i]);
    }
    static constexpr const char* kMissionButtonTextNames[kGlueMissionButtonCount] = {
        "CampaignMissionOneButtonText",
        "CampaignMissionTwoButtonText",
        "CampaignMission3ButtonText",
        "CampaignMission4ButtonText",
        "CampaignMission5ButtonText",
        "CampaignMission6ButtonText",
        "CampaignMission7ButtonText",
        "CampaignMission8ButtonText",
        "CampaignMission9ButtonText",
        "CampaignMission10ButtonText",
        "CampaignMission11ButtonText",
        "CampaignMission12ButtonText",
    };
    static constexpr const char* kMissionButtonSelectedNames[kGlueMissionButtonCount] = {
        "CampaignMissionOneSelected",
        "CampaignMissionTwoSelected",
        "CampaignMission3Selected",
        "CampaignMission4Selected",
        "CampaignMission5Selected",
        "CampaignMission6Selected",
        "CampaignMission7Selected",
        "CampaignMission8Selected",
        "CampaignMission9Selected",
        "CampaignMission10Selected",
        "CampaignMission11Selected",
        "CampaignMission12Selected",
    };
    for (int i = 0; i < kGlueMissionButtonCount; ++i) {
        missionButtonTexts_[i] = FindAs<Aero::TextBlock>(campaignMenu_, kMissionButtonTextNames[i]);
        missionButtonSelected_[i] = FindAs<Aero::FrameworkElement>(campaignMenu_, kMissionButtonSelectedNames[i]);
    }
    customGameMenu_ = FindAs<Aero::FrameworkElement>(this, "CustomGameMenu");
    optionsMenu_ = FindAs<Aero::FrameworkElement>(this, "OptionsMenu");
    optionsListPanel_ = FindAs<Aero::FrameworkElement>(optionsMenu_, "GlueOptionsListPanel");
    optionsListBottomPanel_ = FindAs<Aero::FrameworkElement>(optionsMenu_, "GlueOptionsListBottomPanel");
    optionsDetailBottomPanel_ = FindAs<Aero::FrameworkElement>(optionsMenu_, "GlueOptionsDetailBottomPanel");
    static constexpr const char* kOptionsPanelNames[kGlueOptionsTabCount] = {
        "GlueOptionsGameplayPanel",
        "GlueOptionsVideoPanel",
        "GlueOptionsSoundPanel",
        "GlueOptionsNetworkPanel",
    };
    static constexpr const char* kOptionsSelectedNames[kGlueOptionsTabCount] = {
        "GlueOptionsGameplaySelected",
        "GlueOptionsVideoSelected",
        "GlueOptionsSoundSelected",
        "GlueOptionsNetworkSelected",
    };
    for (int i = 0; i < kGlueOptionsTabCount; ++i) {
        optionsPanels_[i] = FindAs<Aero::FrameworkElement>(optionsMenu_, kOptionsPanelNames[i]);
        optionsSelected_[i] = FindAs<Aero::FrameworkElement>(optionsMenu_, kOptionsSelectedNames[i]);
    }
    customGameTeamSetupPane_ = FindAs<Aero::FrameworkElement>(customGameMenu_, "TeamSetupPane");
    customGameMapInfoPane_ = FindAs<Aero::FrameworkElement>(customGameMenu_, "MapInfoPane");
    customGameAdvancedOptionsPane_ = FindAs<Aero::FrameworkElement>(customGameMenu_, "AdvancedOptionsPane");
    selectedMapText_ = FindAs<Aero::TextBlock>(customGameMenu_, "SelectedMapText");
    selectedMapPlayersText_ = FindAs<Aero::TextBlock>(customGameMenu_, "SelectedMapPlayersText");
    selectedMapMaxPlayersText_ = FindAs<Aero::TextBlock>(customGameMenu_, "SelectedMapMaxPlayersText");
    selectedMapSizeText_ = FindAs<Aero::TextBlock>(customGameMenu_, "SelectedMapSizeText");
    selectedMapTilesetText_ = FindAs<Aero::TextBlock>(customGameMenu_, "SelectedMapTilesetText");
    selectedMapAuthorText_ = FindAs<Aero::TextBlock>(customGameMenu_, "SelectedMapAuthorText");
    selectedMapPageText_ = FindAs<Aero::TextBlock>(customGameMenu_, "SelectedMapPageText");
    selectedMapPathText_ = FindAs<Aero::TextBlock>(customGameMenu_, "SelectedMapPathText");
    selectedMapDescriptionText_ = FindAs<Aero::TextBlock>(customGameMenu_, "SelectedMapDescriptionText");
    for (int i = 0; i < kGlueMapSlotCount; ++i) {
        char name[64];
        std::snprintf(name, sizeof(name), "SelectedMapPreview%d", i);
        selectedMapPreviewImages_[i] = FindAs<Aero::FrameworkElement>(customGameMenu_, name);
    }
    teamSetupSummaryText_ = FindAs<Aero::TextBlock>(customGameMenu_, "TeamSetupSummaryText");
    teamsTogetherToggleText_ = FindAs<Aero::TextBlock>(customGameMenu_, "TeamsTogetherToggleText");
    fixedTeamsToggleText_ = FindAs<Aero::TextBlock>(customGameMenu_, "FixedTeamsToggleText");
    fullSharedControlToggleText_ = FindAs<Aero::TextBlock>(customGameMenu_, "FullSharedControlToggleText");
    for (int i = 0; i < kGlueMapSlotCount; ++i) {
        char name[64];
        std::snprintf(name, sizeof(name), "MapItem%dText", i);
        mapItemTexts_[i] = FindAs<Aero::TextBlock>(customGameMenu_, name);
        std::snprintf(name, sizeof(name), "MapItem%dSelected", i);
        mapItemSelected_[i] = FindAs<Aero::FrameworkElement>(customGameMenu_, name);
    }
    for (int i = 0; i < kGluePlayerSlotCount; ++i) {
        char name[64];
        std::snprintf(name, sizeof(name), "PlayerSlot%d", i);
        playerSlotRows_[i] = FindAs<Aero::FrameworkElement>(customGameMenu_, name);
        std::snprintf(name, sizeof(name), "PlayerSlot%dNameText", i);
        playerSlotNameTexts_[i] = FindAs<Aero::TextBlock>(customGameMenu_, name);
        std::snprintf(name, sizeof(name), "PlayerSlot%dRaceText", i);
        playerSlotRaceTexts_[i] = FindAs<Aero::TextBlock>(customGameMenu_, name);
        std::snprintf(name, sizeof(name), "PlayerSlot%dTeamText", i);
        playerSlotTeamTexts_[i] = FindAs<Aero::TextBlock>(customGameMenu_, name);
        std::snprintf(name, sizeof(name), "PlayerSlot%dColorButtonValue", i);
        playerSlotColorButtonValues_[i] = FindAs<Aero::Border>(customGameMenu_, name);
    }
    stateText_ = FindAs<Aero::TextBlock>(this, "GlueMenuStateText");
    for (int i = 0; i < kActionCount; ++i) {
        const char* elementName = GlueMenuActionElementName(i);
        buttons_[i] = FindAsAny<Aero::FrameworkElement>(
            this, {mainMenuFrame_, battleNetMenu_, localAreaNetworkMenu_, singlePlayerMenu_, campaignMenu_, customGameMenu_, optionsMenu_}, elementName);
        char name[96];
        std::snprintf(name, sizeof(name), "GlueMenuButtonHover%s", elementName);
        buttonHighlights_[i] = FindAsAny<Aero::FrameworkElement>(
            this, {mainMenuFrame_, battleNetMenu_, localAreaNetworkMenu_, singlePlayerMenu_, campaignMenu_, customGameMenu_, optionsMenu_}, name);
        std::snprintf(name, sizeof(name), "GlueMenuButtonPressed%s", elementName);
        buttonPressed_[i] = FindAsAny<Aero::FrameworkElement>(
            this, {mainMenuFrame_, battleNetMenu_, localAreaNetworkMenu_, singlePlayerMenu_, campaignMenu_, customGameMenu_, optionsMenu_}, name);
    }
}

void GlueMenuControl::LogMissingElements() const {
    std::vector<std::string> missing;
    auto check = [&missing](const void* ptr, const char* name) {
        if (!ptr) missing.push_back(name);
    };

    check(mainMenuFrame_, "MainMenuFrame");
    check(mainMenuBackdropFrame_, "MainMenuBackdropFrame");
    check(quitConfirmDialog_, "QuitConfirmDialog");
    check(battleNetMenu_, "BattleNetMenu");
    check(localAreaNetworkMenu_, "LocalAreaNetworkMenu");
    check(nightElfBackdropLayer_, "GlueNightElfBackdropLayer");
    check(singlePlayerMenu_, "SinglePlayerMenu");
    check(singlePlayerMainPanel_, "SinglePlayerMainPanel");
    check(profilePanel_, "ProfilePanel");
    check(campaignMenu_, "CampaignMenu");
    check(campaignSelectFrame_, "CampaignSelectFrame");
    check(campaignRootMenuUI_, "CampaignRootMenuUI");
    check(campaignMissionSelectUI_, "CampaignMissionSelectMenuUI");
    check(optionsMenu_, "OptionsMenu");
    check(optionsListPanel_, "GlueOptionsListPanel");
    check(optionsListBottomPanel_, "GlueOptionsListBottomPanel");
    check(optionsDetailBottomPanel_, "GlueOptionsDetailBottomPanel");
    static constexpr const char* kCampaignBackdropLayerNames[] = {
        "CampaignBackdropNightElfLayer",
        "CampaignBackdropHumanLayer",
        "CampaignBackdropUndeadLayer",
        "CampaignBackdropOrcLayer",
    };
    for (int i = 0; i < 4; ++i) {
        check(campaignBackdropLayers_[i], kCampaignBackdropLayerNames[i]);
    }
    check(campaignSummaryHeaderText_, "CampaignSummaryHeaderText");
    check(campaignSummaryNameText_, "CampaignSummaryNameText");
    check(campaignSummaryMissionCountText_, "CampaignSummaryMissionCountText");
    check(campaignSummaryMapText_, "CampaignSummaryMapText");
    check(campaignMissionHeaderText_, "CampaignMissionHeaderText");
    check(campaignMissionNameText_, "CampaignMissionNameText");
    check(campaignMissionMapText_, "CampaignMissionMapText");
    for (int i = 0; i < kGlueCampaignButtonCount; ++i) {
        char name[64];
        if (i == 0) {
            std::snprintf(name, sizeof(name), "CampaignPrologueButtonText");
        } else if (i == 1) {
            std::snprintf(name, sizeof(name), "CampaignHumanButtonText");
        } else if (i == 2) {
            std::snprintf(name, sizeof(name), "CampaignUndeadButtonText");
        } else if (i == 3) {
            std::snprintf(name, sizeof(name), "CampaignOrcButtonText");
        } else {
            std::snprintf(name, sizeof(name), "CampaignNightElfButtonText");
        }
        check(campaignButtonTexts_[i], name);
        if (i == 0) {
            std::snprintf(name, sizeof(name), "CampaignPrologueNameText");
        } else if (i == 1) {
            std::snprintf(name, sizeof(name), "CampaignHumanNameText");
        } else if (i == 2) {
            std::snprintf(name, sizeof(name), "CampaignUndeadNameText");
        } else if (i == 3) {
            std::snprintf(name, sizeof(name), "CampaignOrcNameText");
        } else {
            std::snprintf(name, sizeof(name), "CampaignNightElfNameText");
        }
        check(campaignButtonNameTexts_[i], name);
        if (i == 0) {
            std::snprintf(name, sizeof(name), "CampaignPrologueSelected");
        } else if (i == 1) {
            std::snprintf(name, sizeof(name), "CampaignHumanSelected");
        } else if (i == 2) {
            std::snprintf(name, sizeof(name), "CampaignUndeadSelected");
        } else if (i == 3) {
            std::snprintf(name, sizeof(name), "CampaignOrcSelected");
        } else {
            std::snprintf(name, sizeof(name), "CampaignNightElfSelected");
        }
        check(campaignButtonSelected_[i], name);
    }
    for (int i = 0; i < kGlueMissionButtonCount; ++i) {
        char name[64];
        if (i == 0) {
            std::snprintf(name, sizeof(name), "CampaignMissionOneButtonText");
        } else if (i == 1) {
            std::snprintf(name, sizeof(name), "CampaignMissionTwoButtonText");
        } else {
            std::snprintf(name, sizeof(name), "CampaignMission%dButtonText", i + 1);
        }
        check(missionButtonTexts_[i], name);
        if (i == 0) {
            std::snprintf(name, sizeof(name), "CampaignMissionOneSelected");
        } else if (i == 1) {
            std::snprintf(name, sizeof(name), "CampaignMissionTwoSelected");
        } else {
            std::snprintf(name, sizeof(name), "CampaignMission%dSelected", i + 1);
        }
        check(missionButtonSelected_[i], name);
    }
    check(customGameMenu_, "CustomGameMenu");
    check(customGameTeamSetupPane_, "TeamSetupPane");
    check(customGameMapInfoPane_, "MapInfoPane");
    check(customGameAdvancedOptionsPane_, "AdvancedOptionsPane");
    check(selectedMapText_, "SelectedMapText");
    check(selectedMapPlayersText_, "SelectedMapPlayersText");
    check(selectedMapMaxPlayersText_, "SelectedMapMaxPlayersText");
    check(selectedMapSizeText_, "SelectedMapSizeText");
    check(selectedMapTilesetText_, "SelectedMapTilesetText");
    check(selectedMapAuthorText_, "SelectedMapAuthorText");
    check(selectedMapPageText_, "SelectedMapPageText");
    check(selectedMapPathText_, "SelectedMapPathText");
    check(selectedMapDescriptionText_, "SelectedMapDescriptionText");
    for (int i = 0; i < kGlueMapSlotCount; ++i) {
        char name[64];
        std::snprintf(name, sizeof(name), "SelectedMapPreview%d", i);
        check(selectedMapPreviewImages_[i], name);
    }
    check(teamSetupSummaryText_, "TeamSetupSummaryText");
    check(teamsTogetherToggleText_, "TeamsTogetherToggleText");
    check(fixedTeamsToggleText_, "FixedTeamsToggleText");
    check(fullSharedControlToggleText_, "FullSharedControlToggleText");
    for (int i = 0; i < kGlueMapSlotCount; ++i) {
        char name[64];
        std::snprintf(name, sizeof(name), "MapItem%dText", i);
        check(mapItemTexts_[i], name);
        std::snprintf(name, sizeof(name), "MapItem%dSelected", i);
        check(mapItemSelected_[i], name);
    }
    for (int i = 0; i < kGluePlayerSlotCount; ++i) {
        char name[64];
        std::snprintf(name, sizeof(name), "PlayerSlot%d", i);
        check(playerSlotRows_[i], name);
        std::snprintf(name, sizeof(name), "PlayerSlot%dNameText", i);
        check(playerSlotNameTexts_[i], name);
        std::snprintf(name, sizeof(name), "PlayerSlot%dRaceText", i);
        check(playerSlotRaceTexts_[i], name);
        std::snprintf(name, sizeof(name), "PlayerSlot%dTeamText", i);
        check(playerSlotTeamTexts_[i], name);
        std::snprintf(name, sizeof(name), "PlayerSlot%dColorButtonValue", i);
        check(playerSlotColorButtonValues_[i], name);
    }
    for (int i = 0; i < kActionCount; ++i) {
        if (!buttons_[i]) missing.push_back(GlueMenuActionElementName(i));
    }

    if (missing.empty()) {
        GlueDiagLog("GlueMenuControl: all named glue menu elements found");
        return;
    }

    std::ostringstream ss;
    ss << "GlueMenuControl: missing " << missing.size() << " named glue menu elements:";
    for (const auto& name : missing) ss << " " << name;
    GlueDiagLog("%s", ss.str().c_str());
}

void GlueMenuControl::CacheRects(const RectResolver& resolveRect) {
    if (!resolveRect) return;
    for (int i = 0; i < kActionCount; ++i) {
        buttonRects_[i] = IsActionVisible(i) ? resolveRect(buttons_[i]) : ScreenRect{};
    }
}

void GlueMenuControl::SetMaps(const std::vector<GlueMapListItem>& maps) {
    allMapNames_.clear();
    allMapPaths_.clear();
    allMapLabels_.clear();
    allMapPlayers_.clear();
    allMapAuthors_.clear();
    allMapSizes_.clear();
    allMapTilesets_.clear();
    allMapDescriptions_.clear();
    allMapPlayerSlotCounts_.clear();
    allMapNames_.reserve(maps.size());
    allMapPaths_.reserve(maps.size());
    allMapLabels_.reserve(maps.size());
    allMapPlayers_.reserve(maps.size());
    allMapAuthors_.reserve(maps.size());
    allMapSizes_.reserve(maps.size());
    allMapTilesets_.reserve(maps.size());
    allMapDescriptions_.reserve(maps.size());
    allMapPlayerSlotCounts_.reserve(maps.size());

    for (size_t i = 0; i < maps.size(); ++i) {
        const std::string fileName = maps[i].fileName.empty() ? maps[i].path : maps[i].fileName;
        const std::string mapPath = maps[i].path.empty() ? fileName : maps[i].path;
        const std::string label = maps[i].displayName.empty() ? StemWithoutPlayerPrefix(fileName) : maps[i].displayName;
        const std::string players = maps[i].recommendedPlayers.empty()
            ? SuggestedPlayersFromFileName(fileName)
            : maps[i].recommendedPlayers;
        const std::string author = maps[i].author.empty() ? "Unknown" : maps[i].author;
        const std::string mapSize = maps[i].mapSize.empty() ? "Unknown" : maps[i].mapSize;
        const std::string tileset = maps[i].tileset.empty() ? "Unknown" : maps[i].tileset;
        const std::string description = maps[i].description.empty()
            ? (maps[i].path.empty() ? "Discovered map from the active map list." : "Discovered map: " + maps[i].path)
            : maps[i].description;
        const int playerSlots = maps[i].playerSlots > 0 ? maps[i].playerSlots : PlayerSlotsFromFileName(fileName);
        allMapNames_.push_back(fileName);
        allMapPaths_.push_back(mapPath);
        allMapLabels_.push_back(label);
        allMapPlayers_.push_back(players);
        allMapAuthors_.push_back(author);
        allMapSizes_.push_back(mapSize);
        allMapTilesets_.push_back(tileset);
        allMapDescriptions_.push_back(description);
        allMapPlayerSlotCounts_.push_back(playerSlots > 0 ? playerSlots : 0);
    }

    const int count = allMapNames_.empty() ? visibleMapCount_ : std::min<int>((int)allMapNames_.size(), kGlueMapSlotCount);
    visibleMapCount_ = count > 0 ? count : visibleMapCount_;
    mapListOffset_ = 0;
    selectedMapIndex_ = std::clamp(selectedMapIndex_, 0, std::max(0, (int)allMapNames_.size() - 1));
    for (int i = 0; i < count && i < (int)allMapNames_.size(); ++i) {
        mapNames_[i] = allMapNames_[i];
        mapPaths_[i] = allMapPaths_[i];
        mapLabels_[i] = allMapLabels_[i];
        mapPlayers_[i] = allMapPlayers_[i];
        mapAuthors_[i] = allMapAuthors_[i];
        mapSizes_[i] = allMapSizes_[i];
        mapTilesets_[i] = allMapTilesets_[i];
        mapDescriptions_[i] = allMapDescriptions_[i];
        mapPlayerSlotCounts_[i] = allMapPlayerSlotCounts_[i] > 0 ? allMapPlayerSlotCounts_[i] : mapPlayerSlotCounts_[i];
        GlueDiagLog("GlueMenuControl: map slot %d file='%s' label='%s' players='%s' size='%s' tileset='%s' mapSlots=%d visibleSlots=%d",
                    i, mapNames_[i].c_str(), mapLabels_[i].c_str(), mapPlayers_[i].c_str(),
                    mapSizes_[i].c_str(), mapTilesets_[i].c_str(),
                    mapPlayerSlotCounts_[i], VisiblePlayerSlotsForMap(mapPlayerSlotCounts_[i]));
    }
    GlueDiagLog("GlueMenuControl: received %d discovered custom game map entries, showing %d",
                (int)maps.size(), count);
    ApplyState();
}

void GlueMenuControl::SetCampaigns(const std::vector<GlueCampaignListItem>& campaigns) {
    campaigns_ = campaigns;
    selectedCampaignIndex_ = std::clamp(selectedCampaignIndex_, 0, std::max(0, (int)campaigns_.size() - 1));
    selectedCampaignMissionIndex_ = 0;
    for (size_t i = 0; i < campaigns_.size(); ++i) {
        GlueDiagLog("GlueMenuControl: campaign %d id='%s' header='%s' name='%s' missions=%d",
                    (int)i,
                    campaigns_[i].id.c_str(),
                    campaigns_[i].header.c_str(),
                    campaigns_[i].name.c_str(),
                    (int)campaigns_[i].missions.size());
    }
    GlueDiagLog("GlueMenuControl: received %d campaign entries, showing %d",
                (int)campaigns_.size(), std::min<int>((int)campaigns_.size(), kGlueCampaignButtonCount));
    ApplyState();
}

void GlueMenuControl::SetMapLaunchCallback(GlueMapLaunchCallback cb) {
    mapLaunchCallback_ = std::move(cb);
}

bool GlueMenuControl::IsActionVisible(int action) const {
    switch (action) {
    case kGlueMenuSinglePlayer:
    case kGlueMenuBattleNet:
    case kGlueMenuLocalAreaNetwork:
    case kGlueMenuOptions:
    case kGlueMenuCredits:
    case kGlueMenuExit:
        return page_ == Page::MainMenu && !quitConfirmOpen_;
    case kGlueMenuQuitConfirmOK:
    case kGlueMenuQuitConfirmCancel:
        return page_ == Page::MainMenu && quitConfirmOpen_;
    case kGlueMenuProfile:
    case kGlueMenuCampaign:
    case kGlueMenuLoadSaved:
    case kGlueMenuViewReplay:
    case kGlueMenuCustomCampaign:
    case kGlueMenuCustomGame:
        return page_ == Page::SinglePlayer && !singlePlayerProfileOpen_;
    case kGlueMenuCancel:
        return page_ == Page::SinglePlayer;
    case kGlueMenuBack:
        return page_ == Page::Campaign;
    case kGlueMenuCampaignPrologue:
    case kGlueMenuCampaignHuman:
    case kGlueMenuCampaignUndead:
    case kGlueMenuCampaignOrc:
    case kGlueMenuCampaignNightElf: {
        const int campaignIndex = CampaignActionToIndex(action);
        return page_ == Page::Campaign
            && campaignIndex >= 0
            && (campaigns_.empty() ? campaignIndex < 2 : campaignIndex < (int)campaigns_.size());
    }
    case kGlueMenuCampaignMissionOne:
    case kGlueMenuCampaignMissionTwo:
    case kGlueMenuCampaignMission3:
    case kGlueMenuCampaignMission4:
    case kGlueMenuCampaignMission5:
    case kGlueMenuCampaignMission6:
    case kGlueMenuCampaignMission7:
    case kGlueMenuCampaignMission8:
    case kGlueMenuCampaignMission9:
    case kGlueMenuCampaignMission10:
    case kGlueMenuCampaignMission11:
    case kGlueMenuCampaignMission12: {
        const int missionIndex = CampaignMissionActionToIndex(action);
        return page_ == Page::MissionSelect
            && missionIndex >= 0
            && (campaigns_.empty()
                ? missionIndex < 2
                : (selectedCampaignIndex_ >= 0
                   && selectedCampaignIndex_ < (int)campaigns_.size()
                   && missionIndex < (int)campaigns_[selectedCampaignIndex_].missions.size()));
    }
    case kGlueMenuMissionBack:
        return page_ == Page::MissionSelect;
    case kGlueMenuPlayGame:
    case kGlueMenuCustomGameCancel:
    case kGlueMenuMapInfo:
    case kGlueMenuAdvancedOptions:
        return page_ == Page::CustomGame;
    case kGlueMenuOptionsGameplay:
    case kGlueMenuOptionsVideo:
    case kGlueMenuOptionsSound:
    case kGlueMenuOptionsNetwork:
        return page_ == Page::Options && selectedOptionsTab_ < 0;
    case kGlueMenuOptionsBack:
        return page_ == Page::Options && selectedOptionsTab_ < 0;
    case kGlueMenuOptionsOK:
    case kGlueMenuOptionsCancel:
        return page_ == Page::Options && selectedOptionsTab_ >= 0;
    case kGlueMenuBattleNetLogon:
    case kGlueMenuBattleNetCreateAccount:
    case kGlueMenuBattleNetRecoverPassword:
    case kGlueMenuQuitBattleNet:
        return page_ == Page::BattleNet;
    case kGlueMenuLanRefresh:
    case kGlueMenuLanCreateGame:
    case kGlueMenuLanJoinGame:
    case kGlueMenuQuitLocalAreaNetwork:
        return page_ == Page::LocalAreaNetwork;
    case kGlueMenuTeamsTogetherToggle:
    case kGlueMenuFixedTeamsToggle:
    case kGlueMenuFullSharedControlToggle:
        return page_ == Page::CustomGame && customGameAdvancedOptionsOpen_;
    case kGlueMenuMapItem0:
    case kGlueMenuMapItem1:
    case kGlueMenuMapItem2:
    case kGlueMenuMapItem3:
    case kGlueMenuMapItem4:
    case kGlueMenuMapItem5:
    case kGlueMenuMapItem6:
    case kGlueMenuMapItem7:
    case kGlueMenuMapItem8:
    case kGlueMenuMapItem9: {
        const int row = action - kGlueMenuMapItem0;
        return page_ == Page::CustomGame && row >= 0 && row < visibleMapCount_;
    }
    case kGlueMenuMapPageUp:
        return page_ == Page::CustomGame && mapListOffset_ > 0;
    case kGlueMenuMapPageDown: {
        const int mapCount = allMapNames_.empty() ? visibleMapCount_ : (int)allMapNames_.size();
        return page_ == Page::CustomGame && mapListOffset_ + visibleMapCount_ < mapCount;
    }
    case kGlueMenuPlayerSlot0Name:
    case kGlueMenuPlayerSlot1Name:
    case kGlueMenuPlayerSlot2Name:
    case kGlueMenuPlayerSlot0Race:
    case kGlueMenuPlayerSlot1Race:
    case kGlueMenuPlayerSlot2Race:
    case kGlueMenuPlayerSlot3Name:
    case kGlueMenuPlayerSlot4Name:
    case kGlueMenuPlayerSlot5Name:
    case kGlueMenuPlayerSlot6Name:
    case kGlueMenuPlayerSlot7Name:
    case kGlueMenuPlayerSlot8Name:
    case kGlueMenuPlayerSlot9Name:
    case kGlueMenuPlayerSlot10Name:
    case kGlueMenuPlayerSlot11Name:
    case kGlueMenuPlayerSlot3Race:
    case kGlueMenuPlayerSlot4Race:
    case kGlueMenuPlayerSlot5Race:
    case kGlueMenuPlayerSlot6Race:
    case kGlueMenuPlayerSlot7Race:
    case kGlueMenuPlayerSlot8Race:
    case kGlueMenuPlayerSlot9Race:
    case kGlueMenuPlayerSlot10Race:
    case kGlueMenuPlayerSlot11Race: {
        const int slot = std::max(PlayerSlotNameActionToSlot(action), PlayerSlotRaceActionToSlot(action));
        const int selectedSlots = allMapPlayerSlotCounts_.empty()
            ? mapPlayerSlotCounts_[std::clamp(selectedMapIndex_, 0, kGlueMapSlotCount - 1)]
            : allMapPlayerSlotCounts_[std::clamp(selectedMapIndex_, 0, (int)allMapPlayerSlotCounts_.size() - 1)];
        return page_ == Page::CustomGame && slot >= 0 && slot < VisiblePlayerSlotsForMap(selectedSlots);
    }
    default:
        return false;
    }
}

int GlueMenuControl::HitButton(float x, float y) const {
    for (int i = 0; i < kActionCount; ++i) {
        if (IsActionVisible(i) && buttonRects_[i].Contains(x, y)) {
            return i;
        }
    }
    return -1;
}

void GlueMenuControl::SetHoveredButton(int action) {
    hoveredAction_ = (action >= 0 && action < kActionCount && IsActionVisible(action)) ? action : -1;
}

void GlueMenuControl::SetPressedButton(int action) {
    pressedAction_ = (action >= 0 && action < kActionCount && IsActionVisible(action)) ? action : -1;
}

void GlueMenuControl::HandleAction(int action) {
    if (!IsActionVisible(action)) {
        GlueDiagLog("GlueMenuControl: glue menu action '%s' ignored on %s",
                    GlueMenuActionElementName(action), GlueMenuActionPageName(action));
        return;
    }

    switch (action) {
    case kGlueMenuSinglePlayer:
        quitConfirmOpen_ = false;
        page_ = Page::SinglePlayer;
        singlePlayerProfileOpen_ = false;
        break;
    case kGlueMenuBattleNet:
        quitConfirmOpen_ = false;
        page_ = Page::BattleNet;
        singlePlayerProfileOpen_ = false;
        break;
    case kGlueMenuLocalAreaNetwork:
        quitConfirmOpen_ = false;
        page_ = Page::LocalAreaNetwork;
        singlePlayerProfileOpen_ = false;
        break;
    case kGlueMenuOptions:
        quitConfirmOpen_ = false;
        page_ = Page::Options;
        selectedOptionsTab_ = -1;
        break;
    case kGlueMenuProfile:
        singlePlayerProfileOpen_ = true;
        GlueDiagLog("GlueMenuControl: Profile panel opened");
        break;
    case kGlueMenuCampaign:
        page_ = Page::Campaign;
        singlePlayerProfileOpen_ = false;
        break;
    case kGlueMenuLoadSaved:
        GlueDiagLog("GlueMenuControl: Load Saved Game requested (UI page pending)");
        break;
    case kGlueMenuViewReplay:
        GlueDiagLog("GlueMenuControl: View Replay requested (UI page pending)");
        break;
    case kGlueMenuCustomCampaign:
        GlueDiagLog("GlueMenuControl: Custom Campaign requested (UI page pending)");
        break;
    case kGlueMenuCustomGame:
        page_ = Page::CustomGame;
        singlePlayerProfileOpen_ = false;
        customGameMapInfoOpen_ = false;
        customGameAdvancedOptionsOpen_ = false;
        break;
    case kGlueMenuCancel:
        quitConfirmOpen_ = false;
        if (singlePlayerProfileOpen_) {
            singlePlayerProfileOpen_ = false;
            GlueDiagLog("GlueMenuControl: Profile panel closed");
        } else {
            page_ = Page::MainMenu;
        }
        break;
    case kGlueMenuBack:
        page_ = Page::SinglePlayer;
        singlePlayerProfileOpen_ = false;
        break;
    case kGlueMenuMissionBack:
        page_ = Page::Campaign;
        break;
    case kGlueMenuCampaignPrologue:
    case kGlueMenuCampaignHuman:
    case kGlueMenuCampaignUndead:
    case kGlueMenuCampaignOrc:
    case kGlueMenuCampaignNightElf:
        SelectCampaign(CampaignActionToIndex(action));
        break;
    case kGlueMenuCampaignMissionOne:
    case kGlueMenuCampaignMissionTwo:
    case kGlueMenuCampaignMission3:
    case kGlueMenuCampaignMission4:
    case kGlueMenuCampaignMission5:
    case kGlueMenuCampaignMission6:
    case kGlueMenuCampaignMission7:
    case kGlueMenuCampaignMission8:
    case kGlueMenuCampaignMission9:
    case kGlueMenuCampaignMission10:
    case kGlueMenuCampaignMission11:
    case kGlueMenuCampaignMission12: {
        const int missionIndex = CampaignMissionActionToIndex(action);
        selectedCampaignMissionIndex_ = std::max(0, missionIndex);
        std::string launchMapPath;
        if (!campaigns_.empty()
            && selectedCampaignIndex_ >= 0
            && selectedCampaignIndex_ < (int)campaigns_.size()
            && missionIndex < (int)campaigns_[selectedCampaignIndex_].missions.size()) {
            const GlueCampaignListItem& campaign = campaigns_[selectedCampaignIndex_];
            const GlueCampaignMission& mission = campaign.missions[missionIndex];
            launchMapPath = NonEmptyOr(mission.mapPath, "Maps\\Campaign\\Prologue01.w3m");
            GlueDiagLog("GlueMenuControl: Campaign mission selected campaign='%s' campaignName='%s' mission='%s' missionName='%s' map='%s' localPlayer=0",
                        NonEmptyOr(campaign.header, "PROLOGUE"),
                        NonEmptyOr(campaign.name, "EXODUS OF THE HORDE"),
                        NonEmptyOr(mission.header, "01"),
                        NonEmptyOr(mission.name, "CHASING VISIONS"),
                        launchMapPath.c_str());
        } else {
            const char* fallbackMission = missionIndex == 1 ? "02 DEPARTURES" : "01 CHASING VISIONS";
            const char* fallbackMap = missionIndex == 1 ? "Maps\\Campaign\\Prologue02.w3m" : "Maps\\Campaign\\Prologue01.w3m";
            launchMapPath = fallbackMap;
            GlueDiagLog("GlueMenuControl: Campaign mission selected campaign='%s' mission='%s' map='%s' localPlayer=0",
                        campaignMissionHeaderText_ ? campaignMissionHeaderText_->GetText() : "PROLOGUE",
                        fallbackMission,
                        fallbackMap);
        }
        if (mapLaunchCallback_ && !launchMapPath.empty()) {
            GlueDiagLog("GlueMenuControl: requesting map launch path='%s' localPlayer=0 source=campaign",
                        launchMapPath.c_str());
            mapLaunchCallback_(launchMapPath, 0);
        } else {
            GlueDiagLog("GlueMenuControl: map launch callback unavailable for path='%s'",
                        launchMapPath.c_str());
        }
        break;
    }
    case kGlueMenuCustomGameCancel:
        page_ = Page::SinglePlayer;
        singlePlayerProfileOpen_ = false;
        break;
    case kGlueMenuCredits:
        GlueDiagLog("GlueMenuControl: Credits requested (UI page pending)");
        break;
    case kGlueMenuExit:
        quitConfirmOpen_ = true;
        GlueDiagLog("GlueMenuControl: Quit confirmation opened");
        break;
    case kGlueMenuQuitConfirmOK:
        quitConfirmOpen_ = false;
        GlueDiagLog("GlueMenuControl: Exit confirmed (application exit suppressed)");
        break;
    case kGlueMenuQuitConfirmCancel:
        quitConfirmOpen_ = false;
        GlueDiagLog("GlueMenuControl: Quit confirmation canceled");
        break;
    case kGlueMenuPlayGame: {
        std::ostringstream slots;
        const bool hasFullMaps = !allMapNames_.empty();
        const int selectedIndex = hasFullMaps
            ? std::clamp(selectedMapIndex_, 0, (int)allMapNames_.size() - 1)
            : std::clamp(selectedMapIndex_, 0, kGlueMapSlotCount - 1);
        const std::string& mapName = hasFullMaps ? allMapNames_[selectedIndex] : mapNames_[selectedIndex];
        const std::string& mapPath = hasFullMaps ? allMapPaths_[selectedIndex] : mapPaths_[selectedIndex];
        const std::string& mapLabel = hasFullMaps ? allMapLabels_[selectedIndex] : mapLabels_[selectedIndex];
        const int mapSlots = hasFullMaps ? allMapPlayerSlotCounts_[selectedIndex] : mapPlayerSlotCounts_[selectedIndex];
        const int visibleSlots = VisiblePlayerSlotsForMap(mapSlots);
        for (int i = 0; i < visibleSlots; ++i) {
            if (i > 0) slots << ", ";
            slots << i << ":";
            if (i == 0) {
                slots << "Player 1";
            } else {
                slots << ClampPick(kGlueSlotTypeLabels, 5, playerSlotTypeIndices_[i]);
            }
            slots << "/" << ClampPick(kGlueRaceLabels, 5, playerSlotRaceIndices_[i]);
        }
        GlueDiagLog("GlueMenuControl: Play Game selected map='%s' path='%s' index=%d label='%s' mapSlots=%d visibleSlots=%d localPlayer=0 slots=[%s] options={teamsTogether=%s fixedTeams=%s fullSharedControl=%s}",
                    mapName.c_str(),
                    mapPath.c_str(),
                    selectedIndex,
                    mapLabel.c_str(),
                    mapSlots,
                    visibleSlots,
                    slots.str().c_str(),
                    OnOff(advancedTeamsTogether_),
                    OnOff(advancedFixedTeams_),
                    OnOff(advancedFullSharedControl_));
        if (mapLaunchCallback_ && !mapPath.empty()) {
            GlueDiagLog("GlueMenuControl: requesting map launch path='%s' localPlayer=0 source=custom-game",
                        mapPath.c_str());
            mapLaunchCallback_(mapPath, 0);
        } else {
            GlueDiagLog("GlueMenuControl: map launch callback unavailable for path='%s'",
                        mapPath.c_str());
        }
        break;
    }
    case kGlueMenuMapInfo:
        customGameMapInfoOpen_ = !customGameMapInfoOpen_;
        if (customGameMapInfoOpen_) customGameAdvancedOptionsOpen_ = false;
        GlueDiagLog("GlueMenuControl: Map Info panel %s",
                    customGameMapInfoOpen_ ? "opened" : "closed");
        break;
    case kGlueMenuAdvancedOptions:
        customGameAdvancedOptionsOpen_ = !customGameAdvancedOptionsOpen_;
        if (customGameAdvancedOptionsOpen_) customGameMapInfoOpen_ = false;
        GlueDiagLog("GlueMenuControl: Advanced Options panel %s",
                    customGameAdvancedOptionsOpen_ ? "opened" : "closed");
        break;
    case kGlueMenuTeamsTogetherToggle:
        advancedTeamsTogether_ = !advancedTeamsTogether_;
        GlueDiagLog("GlueMenuControl: Advanced option Teams Together %s",
                    advancedTeamsTogether_ ? "On" : "Off");
        break;
    case kGlueMenuFixedTeamsToggle:
        advancedFixedTeams_ = !advancedFixedTeams_;
        GlueDiagLog("GlueMenuControl: Advanced option Fixed Teams %s",
                    advancedFixedTeams_ ? "On" : "Off");
        break;
    case kGlueMenuFullSharedControlToggle:
        advancedFullSharedControl_ = !advancedFullSharedControl_;
        GlueDiagLog("GlueMenuControl: Advanced option Full Shared Control %s",
                    advancedFullSharedControl_ ? "On" : "Off");
        break;
    case kGlueMenuOptionsGameplay:
    case kGlueMenuOptionsVideo:
    case kGlueMenuOptionsSound:
    case kGlueMenuOptionsNetwork:
        selectedOptionsTab_ = std::clamp(action - kGlueMenuOptionsGameplay, 0, kGlueOptionsTabCount - 1);
        GlueDiagLog("GlueMenuControl: Options tab %d selected", selectedOptionsTab_);
        break;
    case kGlueMenuOptionsBack:
        if (selectedOptionsTab_ >= 0) {
            selectedOptionsTab_ = -1;
        } else {
            page_ = Page::MainMenu;
        }
        break;
    case kGlueMenuOptionsOK:
        GlueDiagLog("GlueMenuControl: Options tab %d accepted", selectedOptionsTab_);
        selectedOptionsTab_ = -1;
        break;
    case kGlueMenuOptionsCancel:
        GlueDiagLog("GlueMenuControl: Options tab %d canceled", selectedOptionsTab_);
        selectedOptionsTab_ = -1;
        break;
    case kGlueMenuBattleNetLogon:
        GlueDiagLog("GlueMenuControl: Battle.net logon requested (network pending)");
        break;
    case kGlueMenuBattleNetCreateAccount:
        GlueDiagLog("GlueMenuControl: Battle.net create account requested (network pending)");
        break;
    case kGlueMenuBattleNetRecoverPassword:
        GlueDiagLog("GlueMenuControl: Battle.net recover password requested (network pending)");
        break;
    case kGlueMenuQuitBattleNet:
        page_ = Page::MainMenu;
        break;
    case kGlueMenuLanRefresh:
        GlueDiagLog("GlueMenuControl: Local Area Network refresh requested (discovery pending)");
        break;
    case kGlueMenuLanCreateGame:
        GlueDiagLog("GlueMenuControl: Local Area Network create game requested (host setup pending)");
        break;
    case kGlueMenuLanJoinGame:
        GlueDiagLog("GlueMenuControl: Local Area Network join game requested (no selected game)");
        break;
    case kGlueMenuQuitLocalAreaNetwork:
        page_ = Page::MainMenu;
        break;
    case kGlueMenuMapItem0:
    case kGlueMenuMapItem1:
    case kGlueMenuMapItem2:
    case kGlueMenuMapItem3:
    case kGlueMenuMapItem4:
    case kGlueMenuMapItem5:
    case kGlueMenuMapItem6:
    case kGlueMenuMapItem7:
    case kGlueMenuMapItem8:
    case kGlueMenuMapItem9:
        selectedMapIndex_ = mapListOffset_ + (action - kGlueMenuMapItem0);
        customGameMapInfoOpen_ = true;
        customGameAdvancedOptionsOpen_ = false;
        if (!allMapNames_.empty()) {
            selectedMapIndex_ = std::clamp(selectedMapIndex_, 0, (int)allMapNames_.size() - 1);
            GlueDiagLog("GlueMenuControl: selected map index %d row %d '%s'",
                        selectedMapIndex_, selectedMapIndex_ - mapListOffset_, allMapNames_[selectedMapIndex_].c_str());
            const std::string page = MapPageSummary(selectedMapIndex_, (int)allMapNames_.size(), mapListOffset_);
            GlueDiagLog("GlueMenuControl: Map Info data name='%s' path='%s' %s maxPlayers=%d suggested='%s' size='%s' tileset='%s' author='%s'",
                        allMapLabels_[selectedMapIndex_].c_str(),
                        allMapPaths_[selectedMapIndex_].c_str(),
                        page.c_str(),
                        allMapPlayerSlotCounts_[selectedMapIndex_],
                        allMapPlayers_[selectedMapIndex_].c_str(),
                        allMapSizes_[selectedMapIndex_].c_str(),
                        allMapTilesets_[selectedMapIndex_].c_str(),
                        allMapAuthors_[selectedMapIndex_].c_str());
        } else {
            selectedMapIndex_ = std::clamp(selectedMapIndex_, 0, kGlueMapSlotCount - 1);
            GlueDiagLog("GlueMenuControl: selected map slot %d '%s'",
                        selectedMapIndex_, mapNames_[selectedMapIndex_].c_str());
            const std::string page = MapPageSummary(selectedMapIndex_, visibleMapCount_, 0);
            GlueDiagLog("GlueMenuControl: Map Info data name='%s' path='%s' %s maxPlayers=%d suggested='%s' size='%s' tileset='%s' author='%s'",
                        mapLabels_[selectedMapIndex_].c_str(),
                        mapPaths_[selectedMapIndex_].c_str(),
                        page.c_str(),
                        mapPlayerSlotCounts_[selectedMapIndex_],
                        mapPlayers_[selectedMapIndex_].c_str(),
                        mapSizes_[selectedMapIndex_].c_str(),
                        mapTilesets_[selectedMapIndex_].c_str(),
                        mapAuthors_[selectedMapIndex_].c_str());
        }
        break;
    case kGlueMenuMapPageUp: {
        const int oldOffset = mapListOffset_;
        mapListOffset_ = std::max(0, mapListOffset_ - kGlueMapSlotCount);
        selectedMapIndex_ = mapListOffset_;
        customGameMapInfoOpen_ = true;
        customGameAdvancedOptionsOpen_ = false;
        GlueDiagLog("GlueMenuControl: map list page up offset %d -> %d",
                    oldOffset, mapListOffset_);
        break;
    }
    case kGlueMenuMapPageDown: {
        const int mapCount = allMapNames_.empty() ? visibleMapCount_ : (int)allMapNames_.size();
        const int oldOffset = mapListOffset_;
        mapListOffset_ = std::min(std::max(0, mapCount - 1), mapListOffset_ + kGlueMapSlotCount);
        if (mapListOffset_ + kGlueMapSlotCount > mapCount) {
            mapListOffset_ = std::max(0, mapCount - kGlueMapSlotCount);
        }
        selectedMapIndex_ = mapListOffset_;
        customGameMapInfoOpen_ = true;
        customGameAdvancedOptionsOpen_ = false;
        GlueDiagLog("GlueMenuControl: map list page down offset %d -> %d",
                    oldOffset, mapListOffset_);
        break;
    }
    case kGlueMenuPlayerSlot0Name:
    case kGlueMenuPlayerSlot1Name:
    case kGlueMenuPlayerSlot2Name:
    case kGlueMenuPlayerSlot3Name:
    case kGlueMenuPlayerSlot4Name:
    case kGlueMenuPlayerSlot5Name:
    case kGlueMenuPlayerSlot6Name:
    case kGlueMenuPlayerSlot7Name:
    case kGlueMenuPlayerSlot8Name:
    case kGlueMenuPlayerSlot9Name:
    case kGlueMenuPlayerSlot10Name:
    case kGlueMenuPlayerSlot11Name: {
        const int slot = PlayerSlotNameActionToSlot(action);
        const int selectedSlots = allMapPlayerSlotCounts_.empty()
            ? mapPlayerSlotCounts_[std::clamp(selectedMapIndex_, 0, kGlueMapSlotCount - 1)]
            : allMapPlayerSlotCounts_[std::clamp(selectedMapIndex_, 0, (int)allMapPlayerSlotCounts_.size() - 1)];
        if (slot < 0 || slot >= VisiblePlayerSlotsForMap(selectedSlots)) break;
        playerSlotTypeIndices_[slot] = (playerSlotTypeIndices_[slot] + 1) % 5;
        GlueDiagLog("GlueMenuControl: player slot %d type '%s'",
                    slot, ClampPick(kGlueSlotTypeLabels, 5, playerSlotTypeIndices_[slot]));
        break;
    }
    case kGlueMenuPlayerSlot0Race:
    case kGlueMenuPlayerSlot1Race:
    case kGlueMenuPlayerSlot2Race:
    case kGlueMenuPlayerSlot3Race:
    case kGlueMenuPlayerSlot4Race:
    case kGlueMenuPlayerSlot5Race:
    case kGlueMenuPlayerSlot6Race:
    case kGlueMenuPlayerSlot7Race:
    case kGlueMenuPlayerSlot8Race:
    case kGlueMenuPlayerSlot9Race:
    case kGlueMenuPlayerSlot10Race:
    case kGlueMenuPlayerSlot11Race: {
        const int slot = PlayerSlotRaceActionToSlot(action);
        const int selectedSlots = allMapPlayerSlotCounts_.empty()
            ? mapPlayerSlotCounts_[std::clamp(selectedMapIndex_, 0, kGlueMapSlotCount - 1)]
            : allMapPlayerSlotCounts_[std::clamp(selectedMapIndex_, 0, (int)allMapPlayerSlotCounts_.size() - 1)];
        if (slot < 0 || slot >= VisiblePlayerSlotsForMap(selectedSlots)) break;
        playerSlotRaceIndices_[slot] = (playerSlotRaceIndices_[slot] + 1) % 5;
        GlueDiagLog("GlueMenuControl: player slot %d race '%s'",
                    slot, ClampPick(kGlueRaceLabels, 5, playerSlotRaceIndices_[slot]));
        break;
    }
    default:
        break;
    }

    hoveredAction_ = -1;
    pressedAction_ = -1;
    ApplyState();
    GlueDiagLog("GlueMenuControl: action '%s' selected %s",
                GlueMenuActionElementName(action), PageLabel());
}

void GlueMenuControl::OpenDebugPage(const std::string& pageName) {
    std::string key = NormalizeDebugKey(pageName);
    quitConfirmOpen_ = false;
    if (key.empty() || key == "main" || key == "mainmenu" || key == "home" || key == "mainmenuframe") {
        page_ = Page::MainMenu;
        singlePlayerProfileOpen_ = false;
    } else if (key == "quitconfirm" || key == "exitconfirm" || key == "quitdialog" || key == "exitdialog") {
        page_ = Page::MainMenu;
        singlePlayerProfileOpen_ = false;
        quitConfirmOpen_ = true;
    } else if (key == "battlenet" || key == "battle.net" || key == "bnet" || key == "battlenetmenu") {
        page_ = Page::BattleNet;
        singlePlayerProfileOpen_ = false;
    } else if (key == "localareanetwork" || key == "lan" || key == "localnetwork" || key == "localareanetworkmenu") {
        page_ = Page::LocalAreaNetwork;
        singlePlayerProfileOpen_ = false;
    } else if (key == "singleplayer" || key == "singleplayermenu") {
        page_ = Page::SinglePlayer;
        singlePlayerProfileOpen_ = false;
    } else if (key == "profile" || key == "profiles" || key == "singleplayerprofile") {
        page_ = Page::SinglePlayer;
        singlePlayerProfileOpen_ = true;
    } else if (key == "campaign" || key == "campaignmenu") {
        page_ = Page::Campaign;
        singlePlayerProfileOpen_ = false;
    } else if (key == "missionselect" || key == "mission") {
        page_ = Page::MissionSelect;
        singlePlayerProfileOpen_ = false;
    } else if (key == "customgame" || key == "customgames" || key == "skirmish" || key == "singleplayerskirmish") {
        page_ = Page::CustomGame;
        singlePlayerProfileOpen_ = false;
        customGameMapInfoOpen_ = false;
        customGameAdvancedOptionsOpen_ = false;
    } else if (key == "options" || key == "optionsmenu") {
        page_ = Page::Options;
        singlePlayerProfileOpen_ = false;
        selectedOptionsTab_ = -1;
    } else if (key == "gameplay" || key == "optionsgameplay") {
        page_ = Page::Options;
        singlePlayerProfileOpen_ = false;
        selectedOptionsTab_ = 0;
    } else if (key == "video" || key == "optionsvideo") {
        page_ = Page::Options;
        singlePlayerProfileOpen_ = false;
        selectedOptionsTab_ = 1;
    } else if (key == "sound" || key == "audio" || key == "optionssound") {
        page_ = Page::Options;
        singlePlayerProfileOpen_ = false;
        selectedOptionsTab_ = 2;
    } else if (key == "network" || key == "optionsnetwork") {
        page_ = Page::Options;
        singlePlayerProfileOpen_ = false;
        selectedOptionsTab_ = 3;
    } else {
        GlueDiagLog("GlueMenuControl: debug page '%s' not found, using MainMenuFrame", pageName.c_str());
        page_ = Page::MainMenu;
        singlePlayerProfileOpen_ = false;
    }
    ApplyState();
    GlueDiagLog("GlueMenuControl: debug showing %s", PageLabel());
}

bool GlueMenuControl::InvokeActionForDebug(const std::string& actionName) {
    const int action = GlueMenuActionForDebugName(actionName);
    if (action == -2) {
        page_ = Page::MainMenu;
        quitConfirmOpen_ = false;
        singlePlayerProfileOpen_ = false;
        ApplyState();
        GlueDiagLog("GlueMenuControl: debug action '%s' selected MainMenuFrame", actionName.c_str());
        return true;
    }
    if (action < 0) {
        GlueDiagLog("GlueMenuControl: debug action '%s' not found", actionName.c_str());
        return false;
    }
    HandleAction(action);
    GlueDiagLog("GlueMenuControl: debug action '%s' invoked", actionName.c_str());
    return true;
}

void GlueMenuControl::SelectCampaign(int index) {
    if (!campaigns_.empty()) {
        index = std::clamp(index, 0, (int)campaigns_.size() - 1);
    }
    selectedCampaignIndex_ = index;
    selectedCampaignMissionIndex_ = 0;
    page_ = Page::MissionSelect;
    ApplyCampaignData();
}

void GlueMenuControl::ApplyCampaignData() {
    if (!campaigns_.empty()) {
        selectedCampaignIndex_ = std::clamp(selectedCampaignIndex_, 0, (int)campaigns_.size() - 1);
    } else {
        selectedCampaignIndex_ = std::clamp(selectedCampaignIndex_, 0, kGlueCampaignButtonCount - 1);
    }

    for (int i = 0; i < kGlueCampaignButtonCount; ++i) {
        const bool hasCampaign = campaigns_.empty() ? i < 2 : i < (int)campaigns_.size();
        if (buttons_[kGlueMenuCampaignPrologue + i]) {
            buttons_[kGlueMenuCampaignPrologue + i]->SetVisibility(hasCampaign
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
        if (campaignButtonSelected_[i]) {
            campaignButtonSelected_[i]->SetVisibility(hasCampaign && i == selectedCampaignIndex_
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
        if (!campaigns_.empty() && i < (int)campaigns_.size()) {
            if (campaignButtonTexts_[i]) {
                campaignButtonTexts_[i]->SetText(NonEmptyOr(campaigns_[i].header, campaigns_[i].id.c_str()));
            }
            if (campaignButtonNameTexts_[i]) {
                campaignButtonNameTexts_[i]->SetText(NonEmptyOr(campaigns_[i].name, ""));
            }
        } else if (campaigns_.empty()) {
            static constexpr const char* kFallbackCampaignHeaders[kGlueCampaignButtonCount] = {
                "PROLOGUE CAMPAIGN", "HUMAN CAMPAIGN", "UNDEAD CAMPAIGN", "ORC CAMPAIGN", "NIGHT ELF CAMPAIGN",
            };
            static constexpr const char* kFallbackCampaignNames[kGlueCampaignButtonCount] = {
                "EXODUS OF THE HORDE", "THE SCOURGE OF LORDAERON", "PATH OF THE DAMNED", "THE INVASION OF KALIMDOR", "ETERNITY'S END",
            };
            if (campaignButtonTexts_[i]) {
                campaignButtonTexts_[i]->SetText(kFallbackCampaignHeaders[i]);
            }
            if (campaignButtonNameTexts_[i]) {
                campaignButtonNameTexts_[i]->SetText(kFallbackCampaignNames[i]);
            }
        }
    }

    const GlueCampaignListItem* campaign = nullptr;
    if (!campaigns_.empty()
        && selectedCampaignIndex_ >= 0
        && selectedCampaignIndex_ < (int)campaigns_.size()) {
        campaign = &campaigns_[selectedCampaignIndex_];
    }
    const int campaignBackdropIndex = CampaignBackdropLayerIndex(campaign, selectedCampaignIndex_);
    const std::string campaignBackdropId = campaign ? campaign->id : "<fallback>";
    if (campaignBackdropIndex != loggedCampaignBackdropIndex_
        || campaignBackdropId != loggedCampaignBackdropId_) {
        GlueDiagLog("GlueMenuControl: campaign backdrop layer %d selected for campaign='%s'",
            campaignBackdropIndex, campaignBackdropId.c_str());
        loggedCampaignBackdropIndex_ = campaignBackdropIndex;
        loggedCampaignBackdropId_ = campaignBackdropId;
    }
    for (int i = 0; i < (int)campaignBackdropLayers_.size(); ++i) {
        if (campaignBackdropLayers_[i]) {
            campaignBackdropLayers_[i]->SetVisibility(i == campaignBackdropIndex
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
    }
    const int missionCount = campaign ? (int)campaign->missions.size() : 2;
    selectedCampaignMissionIndex_ = std::clamp(selectedCampaignMissionIndex_, 0, std::max(0, missionCount - 1));
    const GlueCampaignMission* selectedMission = nullptr;
    if (campaign
        && selectedCampaignMissionIndex_ >= 0
        && selectedCampaignMissionIndex_ < (int)campaign->missions.size()) {
        selectedMission = &campaign->missions[selectedCampaignMissionIndex_];
    }

    const char* fallbackHeader = selectedCampaignIndex_ == 0 ? "PROLOGUE" : "HUMAN CAMPAIGN";
    const char* fallbackName = selectedCampaignIndex_ == 0 ? "EXODUS OF THE HORDE" : "THE SCOURGE OF LORDAERON";
    const char* campaignHeader = campaign ? NonEmptyOr(campaign->header, "CAMPAIGN") : fallbackHeader;
    const char* campaignName = campaign ? NonEmptyOr(campaign->name, "") : fallbackName;

    if (campaignSummaryHeaderText_) {
        campaignSummaryHeaderText_->SetText(campaignHeader);
    }
    if (campaignSummaryNameText_) {
        campaignSummaryNameText_->SetText(campaignName);
    }
    if (campaignSummaryMissionCountText_) {
        std::string countText = std::to_string(std::max(0, missionCount));
        countText += missionCount == 1 ? " Mission" : " Missions";
        campaignSummaryMissionCountText_->SetText(countText.c_str());
    }
    if (campaignSummaryMapText_) {
        std::string firstMap = "First Map: ";
        if (campaign && !campaign->missions.empty()) {
            firstMap += NonEmptyOr(campaign->missions.front().mapPath, "Maps\\Campaign\\Prologue01.w3m");
        } else {
            firstMap += "Maps\\Campaign\\Prologue01.w3m";
        }
        campaignSummaryMapText_->SetText(firstMap.c_str());
    }

    if (campaign) {
        if (campaignMissionHeaderText_) campaignMissionHeaderText_->SetText(campaignHeader);
        if (campaignMissionNameText_) campaignMissionNameText_->SetText(campaignName);
    } else {
        if (campaignMissionHeaderText_) campaignMissionHeaderText_->SetText(campaignHeader);
        if (campaignMissionNameText_) campaignMissionNameText_->SetText(campaignName);
    }
    if (campaignMissionMapText_) {
        if (selectedMission) {
            const std::string mapLabel = std::string("Map: ") + NonEmptyOr(selectedMission->mapPath, "Maps\\Campaign\\Prologue01.w3m");
            campaignMissionMapText_->SetText(mapLabel.c_str());
        } else {
            campaignMissionMapText_->SetText(selectedCampaignMissionIndex_ == 1
                ? "Map: Maps\\Campaign\\Prologue02.w3m"
                : "Map: Maps\\Campaign\\Prologue01.w3m");
        }
    }

    for (int i = 0; i < kGlueMissionButtonCount; ++i) {
        const bool hasMission = campaign ? i < (int)campaign->missions.size() : i < 2;
        if (buttons_[kGlueMenuCampaignMissionOne + i]) {
            buttons_[kGlueMenuCampaignMissionOne + i]->SetVisibility(hasMission
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
        if (missionButtonSelected_[i]) {
            missionButtonSelected_[i]->SetVisibility(hasMission && i == selectedCampaignMissionIndex_
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
        if (!missionButtonTexts_[i]) continue;
        if (campaign && i < (int)campaign->missions.size()) {
            const std::string label = MissionButtonLabel(campaign->missions[i], i);
            missionButtonTexts_[i]->SetText(label.c_str());
        } else if (!campaign) {
            missionButtonTexts_[i]->SetText(i == 0 ? "01  CHASING VISIONS" : "02  DEPARTURES");
        }
    }
}

void GlueMenuControl::ApplyState() {
    SetVisibility(Aero::Visibility_Visible);

    const bool mainVisible = page_ == Page::MainMenu;
    const bool battleNetVisible = page_ == Page::BattleNet;
    const bool localAreaNetworkVisible = page_ == Page::LocalAreaNetwork;
    const bool singlePlayerVisible = page_ == Page::SinglePlayer;
    const bool campaignVisible = page_ == Page::Campaign;
    const bool missionSelectVisible = page_ == Page::MissionSelect;
    const bool customGameVisible = page_ == Page::CustomGame;
    const bool optionsVisible = page_ == Page::Options;
    if (!mainVisible) {
        quitConfirmOpen_ = false;
    }
    if (!singlePlayerVisible) {
        singlePlayerProfileOpen_ = false;
    }
    if (nightElfBackdropLayer_) {
        const bool nightElfBackdropVisible = singlePlayerVisible || customGameVisible || optionsVisible;
        nightElfBackdropLayer_->SetVisibility(nightElfBackdropVisible ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
    }
    if (mainMenuBackdropFrame_) {
        const bool mainMenuBackdropVisible = mainVisible || singlePlayerVisible;
        mainMenuBackdropFrame_->SetVisibility(mainMenuBackdropVisible
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (mainMenuFrame_) mainMenuFrame_->SetVisibility(mainVisible ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
    if (quitConfirmDialog_) {
        quitConfirmDialog_->SetVisibility(mainVisible && quitConfirmOpen_
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (battleNetMenu_) battleNetMenu_->SetVisibility(battleNetVisible ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
    if (localAreaNetworkMenu_) {
        localAreaNetworkMenu_->SetVisibility(localAreaNetworkVisible
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (singlePlayerMenu_) singlePlayerMenu_->SetVisibility(singlePlayerVisible ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
    if (singlePlayerMainPanel_) {
        singlePlayerMainPanel_->SetVisibility(singlePlayerVisible && !singlePlayerProfileOpen_
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (profilePanel_) {
        profilePanel_->SetVisibility(singlePlayerVisible && singlePlayerProfileOpen_
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (campaignMenu_) {
        campaignMenu_->SetVisibility((campaignVisible || missionSelectVisible) ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
    }
    if (campaignSelectFrame_) {
        campaignSelectFrame_->SetVisibility((campaignVisible || missionSelectVisible) ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
    }
    if (campaignRootMenuUI_) {
        campaignRootMenuUI_->SetVisibility(campaignVisible
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (campaignMissionSelectUI_) campaignMissionSelectUI_->SetVisibility(missionSelectVisible ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
    if (customGameMenu_) customGameMenu_->SetVisibility(customGameVisible ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
    if (optionsMenu_) optionsMenu_->SetVisibility(optionsVisible ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
    if (optionsVisible) {
        selectedOptionsTab_ = std::clamp(selectedOptionsTab_, -1, kGlueOptionsTabCount - 1);
    } else {
        selectedOptionsTab_ = -1;
    }
    if (optionsListPanel_) {
        optionsListPanel_->SetVisibility(optionsVisible && selectedOptionsTab_ < 0
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (optionsListBottomPanel_) {
        optionsListBottomPanel_->SetVisibility(optionsVisible && selectedOptionsTab_ < 0
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (optionsDetailBottomPanel_) {
        optionsDetailBottomPanel_->SetVisibility(optionsVisible && selectedOptionsTab_ >= 0
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    for (int i = 0; i < kGlueOptionsTabCount; ++i) {
        if (optionsPanels_[i]) {
            optionsPanels_[i]->SetVisibility(optionsVisible && i == selectedOptionsTab_
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
        if (optionsSelected_[i]) {
            optionsSelected_[i]->SetVisibility(optionsVisible && i == selectedOptionsTab_
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
    }
    ApplyCampaignData();
    if (customGameTeamSetupPane_) {
        customGameTeamSetupPane_->SetVisibility(customGameVisible && !customGameMapInfoOpen_ && !customGameAdvancedOptionsOpen_
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (customGameMapInfoPane_) {
        customGameMapInfoPane_->SetVisibility(customGameVisible && customGameMapInfoOpen_
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (customGameAdvancedOptionsPane_) {
        customGameAdvancedOptionsPane_->SetVisibility(customGameVisible && customGameAdvancedOptionsOpen_
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }

    const bool hasFullMaps = !allMapNames_.empty();
    const int mapCount = hasFullMaps ? (int)allMapNames_.size() : visibleMapCount_;
    if (hasFullMaps) {
        mapListOffset_ = std::clamp(mapListOffset_, 0, std::max(0, mapCount - 1));
        visibleMapCount_ = std::clamp(std::min(kGlueMapSlotCount, mapCount - mapListOffset_), 1, kGlueMapSlotCount);
        if (mapListOffset_ + visibleMapCount_ > mapCount) {
            mapListOffset_ = std::max(0, mapCount - visibleMapCount_);
        }
        selectedMapIndex_ = std::clamp(selectedMapIndex_, 0, mapCount - 1);
    } else {
        mapListOffset_ = 0;
        visibleMapCount_ = std::clamp(visibleMapCount_, 1, kGlueMapSlotCount);
        selectedMapIndex_ = std::clamp(selectedMapIndex_, 0, visibleMapCount_ - 1);
    }
    for (int i = 0; i < kGlueMapSlotCount; ++i) {
        const int mapIndex = mapListOffset_ + i;
        const bool hasMap = hasFullMaps
            ? i < visibleMapCount_ && mapIndex < (int)allMapNames_.size()
            : i < visibleMapCount_ && !mapNames_[i].empty();
        if (buttons_[kGlueMenuMapItem0 + i]) {
            buttons_[kGlueMenuMapItem0 + i]->SetVisibility(hasMap ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
        if (mapItemSelected_[i]) {
            mapItemSelected_[i]->SetVisibility(hasMap && mapIndex == selectedMapIndex_
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
        if (mapItemTexts_[i]) {
            std::string text = (mapIndex == selectedMapIndex_) ? "> " : "  ";
            if (hasMap) {
                text += hasFullMaps ? allMapNames_[mapIndex] : mapNames_[i];
            }
            mapItemTexts_[i]->SetText(text.c_str());
        }
    }
    const int selectedMapDataIndex = hasFullMaps
        ? std::clamp(selectedMapIndex_, 0, (int)allMapNames_.size() - 1)
        : std::clamp(selectedMapIndex_, 0, kGlueMapSlotCount - 1);
    for (int i = 0; i < kGlueMapSlotCount; ++i) {
        if (!selectedMapPreviewImages_[i]) continue;
        selectedMapPreviewImages_[i]->SetVisibility(
            selectedMapDataIndex == i ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
    }
    if (selectedMapText_) {
        selectedMapText_->SetText(hasFullMaps ? allMapLabels_[selectedMapDataIndex].c_str() : mapLabels_[selectedMapDataIndex].c_str());
    }
    if (selectedMapPlayersText_) {
        selectedMapPlayersText_->SetText(hasFullMaps ? allMapPlayers_[selectedMapDataIndex].c_str() : mapPlayers_[selectedMapDataIndex].c_str());
    }
    const int selectedMapSlots = hasFullMaps
        ? allMapPlayerSlotCounts_[selectedMapDataIndex]
        : mapPlayerSlotCounts_[selectedMapDataIndex];
    if (selectedMapMaxPlayersText_) {
        std::string maxPlayers = "Max Players: ";
        maxPlayers += selectedMapSlots > 0 ? std::to_string(selectedMapSlots) : "Unknown";
        selectedMapMaxPlayersText_->SetText(maxPlayers.c_str());
    }
    const std::string selectedMapSize = hasFullMaps
        ? allMapSizes_[selectedMapDataIndex]
        : mapSizes_[selectedMapDataIndex];
    const std::string selectedMapTileset = hasFullMaps
        ? allMapTilesets_[selectedMapDataIndex]
        : mapTilesets_[selectedMapDataIndex];
    const std::string selectedMapAuthor = hasFullMaps
        ? allMapAuthors_[selectedMapDataIndex]
        : mapAuthors_[selectedMapDataIndex];
    if (selectedMapSizeText_) {
        const std::string text = LabeledValue("Map Size", selectedMapSize);
        selectedMapSizeText_->SetText(text.c_str());
    }
    if (selectedMapTilesetText_) {
        const std::string text = LabeledValue("Tileset", selectedMapTileset);
        selectedMapTilesetText_->SetText(text.c_str());
    }
    if (selectedMapAuthorText_) {
        const std::string text = LabeledValue("Author", selectedMapAuthor);
        selectedMapAuthorText_->SetText(text.c_str());
    }
    if (selectedMapPageText_) {
        const int totalMaps = hasFullMaps ? (int)allMapNames_.size() : visibleMapCount_;
        std::string pageText = MapPageSummary(selectedMapDataIndex, totalMaps, mapListOffset_);
        selectedMapPageText_->SetText(pageText.c_str());
    }
    if (selectedMapPathText_) {
        selectedMapPathText_->SetText(hasFullMaps ? allMapPaths_[selectedMapDataIndex].c_str() : mapPaths_[selectedMapDataIndex].c_str());
    }
    if (selectedMapDescriptionText_) {
        selectedMapDescriptionText_->SetText(hasFullMaps ? allMapDescriptions_[selectedMapDataIndex].c_str() : mapDescriptions_[selectedMapDataIndex].c_str());
    }

    const int visiblePlayerSlots = VisiblePlayerSlotsForMap(selectedMapSlots);
    for (int i = 0; i < kGluePlayerSlotCount; ++i) {
        const bool showSlot = i < visiblePlayerSlots;
        if (playerSlotRows_[i]) {
            playerSlotRows_[i]->SetVisibility(showSlot ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
        playerSlotTypeIndices_[i] = std::clamp(playerSlotTypeIndices_[i], 0, 4);
        playerSlotRaceIndices_[i] = std::clamp(playerSlotRaceIndices_[i], 0, 4);
        if (playerSlotNameTexts_[i]) {
            const char* label = i == 0 ? "Player 1" : kGlueSlotTypeLabels[playerSlotTypeIndices_[i]];
            playerSlotNameTexts_[i]->SetText(label);
        }
        if (playerSlotRaceTexts_[i]) {
            playerSlotRaceTexts_[i]->SetText(kGlueRaceLabels[playerSlotRaceIndices_[i]]);
        }
        if (playerSlotTeamTexts_[i]) {
            std::string team = "Team ";
            team += std::to_string(i + 1);
            playerSlotTeamTexts_[i]->SetText(team.c_str());
        }
        playerSlotColorIndices_[i] = std::clamp(playerSlotColorIndices_[i], 0, 11);
        if (playerSlotColorButtonValues_[i]) {
            if (Aero::Brush* brush = FindGlueTeamColorBrush(customGameMenu_, playerSlotColorIndices_[i])) {
                playerSlotColorButtonValues_[i]->SetBackground(brush);
            }
        }
    }
    if (teamSetupSummaryText_) {
        const std::string summary = TeamSetupSummary(selectedMapSlots, selectedMapSize, selectedMapTileset);
        teamSetupSummaryText_->SetText(summary.c_str());
    }
    if (teamsTogetherToggleText_) {
        teamsTogetherToggleText_->SetText(OnOff(advancedTeamsTogether_));
    }
    if (fixedTeamsToggleText_) {
        fixedTeamsToggleText_->SetText(OnOff(advancedFixedTeams_));
    }
    if (fullSharedControlToggleText_) {
        fullSharedControlToggleText_->SetText(OnOff(advancedFullSharedControl_));
    }
    if (stateText_) stateText_->SetText(PageLabel());

    for (int i = 0; i < kActionCount; ++i) {
        const bool visible = IsActionVisible(i);
        const bool mainButtonVisibleBehindDialog = quitConfirmOpen_
            && page_ == Page::MainMenu
            && i >= kGlueMenuSinglePlayer
            && i <= kGlueMenuExit;
        const bool visualVisible = visible || mainButtonVisibleBehindDialog;
        const bool pressed = visible && pressedAction_ == i;
        const bool hovered = visible && hoveredAction_ == i;
        if (buttons_[i]) {
            buttons_[i]->SetVisibility(visualVisible ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
        if (buttonHighlights_[i]) {
            buttonHighlights_[i]->SetVisibility(hovered && !pressed ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
        if (buttonPressed_[i]) {
            buttonPressed_[i]->SetVisibility(pressed ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
    }
}

const char* GlueMenuControl::PageLabel() const {
    switch (page_) {
    case Page::BattleNet: return "BattleNetMenu";
    case Page::LocalAreaNetwork: return "LocalAreaNetworkMenu";
    case Page::SinglePlayer: return singlePlayerProfileOpen_ ? "ProfilePanel" : "SinglePlayerMenu";
    case Page::Campaign: return "CampaignMenu";
    case Page::MissionSelect: return "MissionSelect";
    case Page::CustomGame: return "CustomGameMenu";
    case Page::Options: return "OptionsMenu";
    case Page::MainMenu:
    default:
        return quitConfirmOpen_ ? "QuitConfirmDialog" : "MainMenuFrame";
    }
}

NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(GlueMenuControl, "War3UI.GlueMenuControl")
{
}

NS_END_COLD_REGION

} // namespace War3UI

