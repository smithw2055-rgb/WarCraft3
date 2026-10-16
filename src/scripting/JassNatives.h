#pragma once
#include "JassVM.h"
#include "resource/slk.h"
#include "ecs/melee_ai.h"
#include <memory>
#include <unordered_map>
#include <map>
#include <string>
#include <vector>
#include <functional>
#include <array>
#include <deque>
#include <unordered_set>

// Forward declaration (avoid circular include)
class CSimulation;
class CItem;

// JASS event type constants (matching Blizzard common.j / JassGameEventsWar3 ordinals)
enum JassEventType {
    // Unit events (TriggerRegisterUnitEvent) — ConvertUnitEvent ids.
    JASS_EVENT_UNIT_DAMAGED = 52,
    JASS_EVENT_UNIT_DEATH = 53,
    JASS_EVENT_UNIT_DECAY = 54,
    JASS_EVENT_UNIT_DETECTED = 55,
    JASS_EVENT_UNIT_HIDDEN = 56,
    JASS_EVENT_UNIT_SELECTED = 57,
    JASS_EVENT_UNIT_DESELECTED = 58,
    JASS_EVENT_UNIT_STATE_LIMIT = 59,
    JASS_EVENT_UNIT_ACQUIRED_TARGET = 60,
    JASS_EVENT_UNIT_TARGET_IN_RANGE = 61,
    JASS_EVENT_UNIT_ATTACKED = 62,
    JASS_EVENT_UNIT_RESCUED = 63,
    JASS_EVENT_UNIT_CONSTRUCT_CANCEL = 64,
    JASS_EVENT_UNIT_CONSTRUCT_FINISH = 65,
    JASS_EVENT_UNIT_UPGRADE_START = 66,
    JASS_EVENT_UNIT_UPGRADE_CANCEL = 67,
    JASS_EVENT_UNIT_UPGRADE_FINISH = 68,
    JASS_EVENT_UNIT_TRAIN_START = 69,
    JASS_EVENT_UNIT_TRAIN_CANCEL = 70,
    JASS_EVENT_UNIT_TRAIN_FINISH = 71,
    JASS_EVENT_UNIT_RESEARCH_START = 72,
    JASS_EVENT_UNIT_RESEARCH_CANCEL = 73,
    JASS_EVENT_UNIT_RESEARCH_FINISH = 74,
    JASS_EVENT_UNIT_ISSUED_ORDER = 75,
    JASS_EVENT_UNIT_ISSUED_POINT_ORDER = 76,
    JASS_EVENT_UNIT_ISSUED_TARGET_ORDER = 77,
    JASS_EVENT_UNIT_HERO_LEVEL = 78,
    JASS_EVENT_UNIT_HERO_SKILL = 79,
    JASS_EVENT_UNIT_HERO_REVIVABLE = 80,
    JASS_EVENT_UNIT_HERO_REVIVE_START = 81,
    JASS_EVENT_UNIT_HERO_REVIVE_CANCEL = 82,
    JASS_EVENT_UNIT_HERO_REVIVE_FINISH = 83,
    JASS_EVENT_UNIT_SUMMON = 84,
    JASS_EVENT_UNIT_DROP_ITEM = 85,
    JASS_EVENT_UNIT_PICKUP_ITEM = 86,
    JASS_EVENT_UNIT_USE_ITEM = 87,
    JASS_EVENT_UNIT_LOADED = 88,
    JASS_EVENT_WIDGET_DEATH = 89,
    JASS_EVENT_UNIT_SELL = 286,
    JASS_EVENT_UNIT_CHANGE_OWNER = 287,
    JASS_EVENT_UNIT_SELL_ITEM = 288,
    JASS_EVENT_UNIT_SPELL_CHANNEL = 289,
    JASS_EVENT_UNIT_SPELL_CAST = 290,
    JASS_EVENT_UNIT_SPELL_EFFECT = 291,
    JASS_EVENT_UNIT_SPELL_FINISH = 292,
    JASS_EVENT_UNIT_SPELL_ENDCAST = 293,
    JASS_EVENT_UNIT_PAWN_ITEM = 294,
    JASS_EVENT_UNIT_DAMAGING = 305,
    JASS_EVENT_UNIT_STACK_ITEM = 318,
    JASS_EVENT_DIALOG_BUTTON_CLICK = 90,
    JASS_EVENT_DIALOG_CLICK = 91,
    // Player unit events (TriggerRegisterPlayerUnitEvent) — ConvertPlayerUnitEvent ids.
    JASS_EVENT_PLAYER_UNIT_RESCUED = 19,
    JASS_EVENT_PLAYER_UNIT_DEATH = 20,
    JASS_EVENT_PLAYER_UNIT_DECAY = 21,
    JASS_EVENT_PLAYER_UNIT_TRAIN_FINISH = 34,
    JASS_EVENT_PLAYER_UNIT_RESEARCH_FINISH = 37,
    JASS_EVENT_PLAYER_UNIT_ISSUED_ORDER = 38,
    JASS_EVENT_PLAYER_UNIT_ISSUED_POINT_ORDER = 39,
    JASS_EVENT_PLAYER_UNIT_ISSUED_TARGET_ORDER = 40,
    JASS_EVENT_PLAYER_HERO_REVIVE_FINISH = 46,
    JASS_EVENT_PLAYER_HERO_REVIVABLE = 43,
    JASS_EVENT_PLAYER_HERO_REVIVE_START = 44,
    JASS_EVENT_PLAYER_HERO_REVIVE_CANCEL = 45,
    JASS_EVENT_PLAYER_UNIT_SUMMON = 47,
    JASS_EVENT_PLAYER_UNIT_DROP_ITEM = 48,
    JASS_EVENT_PLAYER_UNIT_PICKUP_ITEM = 49,
    JASS_EVENT_PLAYER_UNIT_USE_ITEM = 50,
    JASS_EVENT_PLAYER_UNIT_LOADED = 51,
    JASS_EVENT_PLAYER_UNIT_UPGRADE_START = 29,
    JASS_EVENT_PLAYER_UNIT_UPGRADE_CANCEL = 30,
    JASS_EVENT_PLAYER_UNIT_UPGRADE_FINISH = 31,
    JASS_EVENT_PLAYER_UNIT_TRAIN_START = 32,
    JASS_EVENT_PLAYER_UNIT_TRAIN_CANCEL = 33,
    JASS_EVENT_PLAYER_UNIT_RESEARCH_START = 35,
    JASS_EVENT_PLAYER_UNIT_RESEARCH_CANCEL = 36,
    JASS_EVENT_PLAYER_UNIT_CONSTRUCT_START = 26,
    JASS_EVENT_PLAYER_UNIT_CONSTRUCT_CANCEL = 27,
    JASS_EVENT_PLAYER_UNIT_CONSTRUCT_FINISH = 28,
    JASS_EVENT_PLAYER_UNIT_SELL = 269,
    JASS_EVENT_PLAYER_UNIT_CHANGE_OWNER = 270,
    JASS_EVENT_PLAYER_UNIT_SELL_ITEM = 271,
    JASS_EVENT_PLAYER_UNIT_SPELL_CHANNEL = 272,
    JASS_EVENT_PLAYER_UNIT_SPELL_CAST = 273,
    JASS_EVENT_PLAYER_UNIT_SPELL_EFFECT = 274,
    JASS_EVENT_PLAYER_UNIT_SPELL_FINISH = 275,
    JASS_EVENT_PLAYER_UNIT_SPELL_ENDCAST = 276,
    JASS_EVENT_PLAYER_UNIT_PAWN_ITEM = 277,
    JASS_EVENT_PLAYER_UNIT_DAMAGING = 309,
    JASS_EVENT_PLAYER_UNIT_STACK_ITEM = 319,
};

struct JassTrigger {
    int id = -1;
    bool enabled = true;
    // Ref Trigger.waitOnSleeps (default true): consulted by
    // TriggerSleepAction/TriggerWaitForSound for the currently executing
    // trigger; TriggerExecuteWait forces it true on the callee. Per-trigger,
    // NOT global (retail signatures take whichTrigger).
    bool waitOnSleeps = true;
    std::vector<int> registeredUnitHandles;
    std::vector<int> eventTypes;
    std::vector<std::string> conditionFuncNames;
    std::vector<std::string> actionFuncNames;
    std::vector<std::pair<int, int>> registeredEvents;
    // Polled trigger events (ref event objects): evaluated edge-triggered
    // (false -> true) by the state poll pass in UpdateTimers.
    struct JassUnitStateEvent {
        int unitHandle = 0;
        int state = 0;
        int opcode = 0;
        float limit = 0.0f;
        bool wasTrue = false;
    };
    std::vector<JassUnitStateEvent> unitStateEvents;
    // Proximity events (ref CUnitEventInRange): range + filter were dropped
    // by the first port; retained here for the poll pass.
    struct JassUnitInRangeEvent {
        int unitHandle = 0;
        float range = 0.0f;
        std::string filterFunc;
        bool inside = false;
    };
    std::vector<JassUnitInRangeEvent> unitInRangeEvents;
    // Region enter/leave events (ref CRegionTriggerEnter/Leave): the filter
    // boolexpr was dropped by the first port; retained here for the poll pass.
    struct JassRegionEvent {
        int regionHandle = 0;
        int code = 0; // 4001 enter, 4002 leave
        std::string filterFunc;
    };
    std::vector<JassRegionEvent> regionEvents;
    // Player-state threshold events (TriggerRegisterPlayerStateEvent).
    struct JassPlayerStateEvent {
        int playerHandle = 0;
        int state = 0;
        int opcode = 0;
        float limit = 0.0f;
        bool wasTrue = false;
    };
    std::vector<JassPlayerStateEvent> playerStateEvents;
    // Variable-watch events (ref registerVariableEvent).
    struct JassVarEvent {
        std::string varName;
        int opcode = 0;
        double limit = 0.0;
        bool wasTrue = false;
    };
    std::vector<JassVarEvent> varEvents;
    // Time-of-day events (ref registerTimeOfDayEvent; TIME_OF_DAY only,
    // matching the ref guard that throws for other game states).
    struct JassGameStateEvent {
        int state = 0;
        int opcode = 0;
        float limit = 0.0f;
        bool wasTrue = false;
    };
    std::vector<JassGameStateEvent> gameStateEvents;
    // Chat events: player + substring filter + exact-match flag.
    struct JassChatEvent {
        int playerHandle = 0;
        std::string find;
        bool exact = false;
    };
    std::vector<JassChatEvent> chatEvents;
    int evalCount = 0;
    int execCount = 0;
};

struct JassTimer {
    float remaining = 0;
    float timeout = 0;
    bool periodic = false;
    bool paused = false;
    std::string callbackFunc;
};

struct JassTimerDialog {
    int timerSimId = -1;
    std::string title;
    bool displayed = false;
    float speed = 1.0f;
    int titleColor[4] = {255, 255, 255, 255};
    int timeColor[4] = {255, 255, 255, 255};
};

// Game setup / control state (ref CGameState / melee AI / fog).
// common.j ordinals: GAME_STATE_DIVINE_INTERVENTION=0, DISCONNECTED=1,
// TIME_OF_DAY=2 (float). Difficulty: EASY=0, NORMAL=1, INSANE=2.
struct JassGameConfig {
    int difficulty = 1;
    int defaultDifficulty = 1;
    int gameSpeed = 2; // MAP_SPEED_NORMAL
    int gameType = 0;
    int placement = 0;
    int teams = 1;
    int mapFlags = 0;
    bool missionAvailable[12] = {};
};

struct JassPlayerAIState {
    bool meleeAI = false;
    bool campaignAI = false;
    int commandCount = 0;
    int difficulty = 1; // AIDifficulty ordinal at StartMeleeAI time
};

struct JassFogState {
    bool fogEnabled = true;
    bool maskEnabled = true;
    int maskStyle = 0;
    float zHeight = 0.0f;
};

// Script terrain-fog override (ref SetTerrainFogEx / ResetTerrainFog via
// war3MapViewer fogSettings, defaultFogSettings as baseline). The renderer
// (linear-only, see P3-22) is fed through terrainFogHook, wired by the
// game controller to DayNightCycle; headless/tests just observe the state.
struct JassTerrainFog {
    bool overridden = false;
    int style = 0; // retail 0=linear, 1=exp, 2=exp2 (renderer: linear only)
    float start = 0.0f;
    float end = 0.0f;
    float density = 0.0f;
    float r = 0.5f, g = 0.5f, b = 0.5f;
};

struct JassGroup {
    std::vector<int> unitHandles;
};

struct JassForce {
    std::vector<int> playerIds;
};

struct JassLocation {
    float x = 0, y = 0;
};

struct JassRect {
    float minX = 0, minY = 0, maxX = 0, maxY = 0;
};

struct JassRegion {
    std::vector<int> rectIds;
    std::vector<JassRect> rects;
    // 32x32 cell squares from RegionAddCell* (ref cells); kept apart from
    // rects so RegionClearRect (which rebuilds from rectIds) can't wipe them.
    std::vector<JassRect> cells;
};

struct JassSound {
    std::string label;
    std::string file; // real filename for FilenameWithLabel sounds
    float volume = 1.0f;
    float pitch = 1.0f;
    int durationMs = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    bool looping = false;
    // Extended playback state (ref CSound).
    int channel = -1;
    float minDistance = 0.0f;
    float maxDistance = 0.0f;
    float distanceCutoff = 0.0f;
    bool killWhenDone = false;
    bool playing = false;
    int attachedUnitSimId = -1;
    bool stackedByPosition = false;
    float stackedWidth = 0.0f;
    float stackedHeight = 0.0f;
    // 3D cone + doppler + seek offset (ref CSound fields).
    float coneInside = 0.0f;
    float coneOutside = 0.0f;
    int coneOutsideVolume = 0;
    float orientX = 0.0f, orientY = 0.0f, orientZ = 0.0f;
    float velX = 0.0f, velY = 0.0f, velZ = 0.0f;
    int playPositionMs = 0;
};

// Music playback state (ref meleeUI music control).
struct JassMusicState {
    std::string mapMusic;
    bool mapMusicRandom = false;
    int mapMusicIndex = 0;
    std::string current;
    bool playing = false;
    int volume = 127;
    int playPositionMs = 0;
    std::string soundEnvironment;
};

struct JassTextTag {
    std::string text;
    float x = 0, y = 0;
    float vx = 0, vy = 0; // drift from SetTextTagVelocity
    float height = 0; // legacy shared field (kept in sync with z/fontSize)
    float z = 0; // world height offset (SetTextTagPos/PosUnit height arg)
    float fontSize = 12.0f; // text size (SetTextTagText height arg)
    float age = 0;
    float lifespan = 0;
    float fadepoint = 0;
    int color = 0xFFFFFFFF;
    bool permanent = false;
    bool visible = true;
    bool suspended = false;
    bool centered = false; // SetTextTagCentered render hint
};

struct JassEffect {
    std::string modelPath;
    float x = 0, y = 0, z = 0;
    // Spell-effect attachment (ref AddSpellEffectTarget*): target widget
    // sim handle + attach point, or -1/empty for point effects.
    int targetSimHandle = -1;
    std::string attachPoint;
    int abilityId = 0;
    int effectType = 0;
};

// Billboard image (ref image API). The renderer draws a camera-facing
// quad at the script height, tinted with the script color.
struct JassImage {
    std::string file;
    float sizeX = 0, sizeY = 0, sizeZ = 0;
    float posX = 0, posY = 0, posZ = 0;
    float originX = 0, originY = 0, originZ = 0;
    int imageType = 0;
    bool visible = false;
    bool render = true;
    bool renderAlways = false;
    bool aboveWater = false;
    bool constantHeight = false;
    int red = 255, green = 255, blue = 255, alpha = 255;
};

// Clickable trackable. Hit (4101) fires on a world click inside the
// radius; track (4102) fires when a unit enters the radius.
struct JassTrackable {
    std::string modelPath;
    float x = 0, y = 0, facing = 0;
    int jassHandle = 0;
    bool occupancyReady = false;
    bool unitInside = false;
};

// Ground decal (ref ubersplat API): renderer consumes finished/color.
struct JassUbersplat {
    float x = 0, y = 0;
    std::string name;
    int red = 255, green = 255, blue = 255, alpha = 255;
    bool forcePaused = false;
    bool noBirthTime = false;
    bool finished = false;
    bool render = true;
    bool renderAlways = false;
};

struct JassQuestItem {
    std::string description;
    bool completed = false;
};

struct JassQuest {
    std::string title;
    std::string description;
    std::string iconPath;
    bool required = false;
    bool completed = false;
    bool discovered = false;
    bool failed = false;
    bool enabled = true;
    std::vector<int> itemIds;
};

struct JassAbilityHandle {
    War3ID code;
    int level = 1;
    int unitSimId = -1;
};

struct JassHandleInfo {
    enum Type {
        UNIT, PLAYER, TRIGGER, TIMER, TIMER_DIALOG, LOCATION, GROUP, FORCE,
        ITEM, DESTRUCTABLE, RECT, REGION, SOUND, TEXT_TAG,
        EFFECT, LIGHTNING, WEATHER, FOG_MODIFIER, QUEST, QUEST_ITEM, CAMERA_SETUP, ABILITY,
        PRIMARY_TAG, SECONDARY_TAG, PRIMARY_TAGS, SECONDARY_TAGS,
        DIALOG, BUTTON, MULTIBOARD, MULTIBOARDITEM, LEADERBOARD,
        IMAGE, TRACKABLE, UBERSPLAT, HASHTABLE, GAMECACHE, PROJECTILE, TARGET_TYPES,
        STAT_BONUS, STATE_MOD, HANDLE_LIST, STRING_LIST, INT_EXPR, FRAME, LOCALSTORE
    };
    Type type;
    int id;
};

struct JassStartLocationConfig {
    float x = 0.0f;
    float y = 0.0f;
    bool defined = false;
    // Start-location preference table (ref SetStartLocPrio/Count): slot ->
    // (other start loc, priority). Unused by fixed placement, kept for AI/UI.
    std::vector<std::pair<int, int>> prio;
};

struct JassPlayerMapConfig {
    int startLocationIndex = -1;
    bool slotActive = false;
    int team = -1;
    int controller = -1; // mapcontrol ordinal from SetPlayerController (-1 unset)
    int racePreference = -1;
    bool raceSelectable = true;
    int color = -1; // playercolor ordinal from SetPlayerColor (-1 unset)
};

class JassNativesData {
public:
    int AllocHandle(JassHandleInfo::Type type, int id);
    void FreeHandle(int handle);
    int GetSimHandle(int jassHandle, JassHandleInfo::Type type) const;
    JassHandleInfo* GetHandleInfo(int jassHandle);
    int CreateItem(CSimulation* sim, War3ID typeId, float x, float y);
    int CreateItemHandle(CSimulation* sim, War3ID typeId, float x, float y);

    std::unordered_map<int, JassHandleInfo> handles;
    int nextHandleId = 0x01000000;

    std::unordered_map<int, std::shared_ptr<JassTrigger>> triggers;
    int nextTriggerId = 0x02000000;
    int currentTriggerId = -1;
    int currentTriggerUnitHandle = -1;
    int currentTriggerEventId = 0;

    std::unordered_map<int, std::shared_ptr<JassTimer>> timers;
    int nextTimerId = 0x03000000;

    std::unordered_map<int, std::shared_ptr<JassTimerDialog>> timerDialogs;
    int nextTimerDialogId = 0x12000000;

    // Script dialog / multiboard / leaderboard UI state (ref meleeUI).
    // Stored sim-side so scripts can build and query boards; the renderer
    // consumes them when drawing the UI.
    struct JassDialogButton {
        int dialogSimId = -1;
        std::string text;
        int hotkey = 0;
        bool quitButton = false;
        bool showScore = false;
    };
    struct JassDialog {
        std::string message;
        std::vector<int> buttonIds;
        bool displayed = false;
    };
    struct JassMultiboardItem {
        int boardSimId = -1;
        int row = 0;
        int col = 0;
        std::string value;
        bool showValue = true;
        bool showIcon = false;
        float width = 0.0f;
        std::string icon;
    };
    struct JassMultiboard {
        std::string title;
        int titleColor[4] = {255, 255, 255, 255};
        int rows = 0;
        int cols = 0;
        bool displayed = false;
        bool minimized = false;
        bool suppressDisplay = false;
        // (row, col) -> item sim id
        std::map<std::pair<int, int>, int> items;
    };
    struct JassLeaderboardItem {
        std::string label;
        int value = 0;
        int playerId = -1;
        bool showLabel = true;
        bool showValue = true;
        bool showIcon = true;
        int labelColor[4] = {255, 255, 255, 255};
        int valueColor[4] = {255, 255, 255, 255};
    };
    struct JassLeaderboard {
        std::string label;
        bool displayed = false;
        std::vector<JassLeaderboardItem> items;
        int labelColor[4] = {255, 255, 255, 255};
        int valueColor[4] = {255, 255, 255, 255};
        bool showLabel = true;
        bool showNames = true;
        bool showValues = true;
        bool showIcons = true;
        int sizeByItemCount = 0;
    };
    // Player -> leaderboard sim id (ref PlayerSetLeaderboard).
    std::unordered_map<int, int> playerLeaderboards;

    std::unordered_map<int, std::shared_ptr<JassDialog>> dialogs;
    int nextDialogId = 0x13000000;
    std::unordered_map<int, std::shared_ptr<JassDialogButton>> dialogButtons;
    int nextDialogButtonId = 0x14000000;
    std::unordered_map<int, std::shared_ptr<JassMultiboard>> multiboards;
    int nextMultiboardId = 0x15000000;
    std::unordered_map<int, std::shared_ptr<JassMultiboardItem>> multiboardItems;
    int nextMultiboardItemId = 0x16000000;
    std::unordered_map<int, std::shared_ptr<JassLeaderboard>> leaderboards;
    int nextLeaderboardId = 0x17000000;

    // Camera setup / bounds state (ref CustomCameraSetup / CameraManager).
    // Field ids are JASS camerafield ordinals (ConvertCameraField).
    struct JassCameraSetup {
        std::unordered_map<int, float> fields;
        float destX = 0.0f;
        float destY = 0.0f;
    };
    struct JassCameraBounds {
        float minX = 0.0f;
        float minY = 0.0f;
        float maxX = 0.0f;
        float maxY = 0.0f;
        float margin = 0.0f;
    };

    std::unordered_map<int, std::shared_ptr<JassCameraSetup>> cameraSetups;
    int nextCameraSetupId = 0x18000000;
    JassCameraBounds cameraBounds;
    float cameraSmoothingFactor = 0.0f;
    std::string cinematicCameraModel;
    bool cameraStopped = false;

    // Cinematic / sky / water render state. No reference implementation
    // exists (the Java port leaves these to the renderer), so the sim
    // stores the script-provided values for the renderer to consume.
    struct JassCineFilter {
        std::string texture;
        int blendMode = 0;
        int texMapFlags = 0;
        float startUV[4] = {0.0f, 0.0f, 1.0f, 1.0f};
        float endUV[4] = {0.0f, 0.0f, 1.0f, 1.0f};
        int startColor[4] = {255, 255, 255, 255};
        int endColor[4] = {255, 255, 255, 255};
        float duration = 0.0f;
        bool displayed = false;
        float age = 0.0f; // seconds since DisplayCineFilter(true)
    };
    struct JassSkyState {
        std::string model;
        std::string texture;
        int color[3] = {255, 255, 255};
        float fogNear = 0.0f;
        float fogFar = 0.0f;
    };
    struct JassWaterState {
        int baseColor[4] = {255, 255, 255, 255};
        int color[4] = {255, 255, 255, 255};
        float transparency = 1.0f;
        float z = 0.0f;
        int deformStyle = 0;
    };
    JassCineFilter cineFilter;
    JassSkyState sky;
    JassWaterState water;
    bool cinematicMode = false;
    std::string dayNightTerrainModel;
    std::string dayNightUnitModel;

    int localPlayerIndex = 0;

    std::unordered_map<int, std::shared_ptr<JassGroup>> groups;
    int nextGroupId = 0x04000000;

    std::unordered_map<int, std::shared_ptr<JassForce>> forces;
    int nextForceId = 0x05000000;

    std::unordered_map<int, CItem*> items;
    std::unordered_map<int, int> unitUserData;

    std::unordered_map<int, std::shared_ptr<JassLocation>> locations;
    int nextLocId = 0x06000000;

    std::unordered_map<int, std::shared_ptr<JassRect>> rects;
    int nextRectId = 0x07000000;

    std::unordered_map<int, std::shared_ptr<JassRegion>> regions;
    int nextRegionId = 0x08000000;

    std::unordered_map<int, std::shared_ptr<JassSound>> sounds;
    int nextSoundId = 0x09000000;
    JassMusicState music;

    std::unordered_map<int, std::shared_ptr<JassTextTag>> textTags;
    int nextTextTagId = 0x0A000000;

    std::unordered_map<int, std::shared_ptr<JassEffect>> effects;
    int nextEffectId = 0x0B000000;

    std::unordered_map<int, std::shared_ptr<JassImage>> images;
    int nextImageId = 0x16000000;
    std::unordered_map<int, std::shared_ptr<JassTrackable>> trackables;
    int nextTrackableId = 0x17000000;
    std::unordered_map<int, std::shared_ptr<JassUbersplat>> ubersplats;
    int nextUbersplatId = 0x18000000;

    // Lightning bolts (ref lightning API): coordinates + color retained for
    // the ribbon renderer; previously handles only.
    struct JassLightning {
        std::string code;
        bool checkVisibility = false;
        float x1 = 0, y1 = 0, z1 = 0, x2 = 0, y2 = 0, z2 = 0;
        float r = 1, g = 1, b = 1, a = 1;
    };
    std::unordered_map<int, std::shared_ptr<JassLightning>> lightnings;

    // Weather effects. Enabled regions spawn falling streaks each frame.
    struct JassWeather {
        War3ID code;
        float minX = 0, minY = 0, maxX = 0, maxY = 0;
        bool enabled = false;
    };
    std::unordered_map<int, std::shared_ptr<JassWeather>> weathers;

    // Weighted random pools (ref itempool/unitpool): entries keep insertion
    // order; rolls use the deterministic sim RNG so setup stays in sync.
    struct JassWeightedPool {
        std::vector<std::pair<uint32_t, double>> entries; // rawcode + weight
    };
    std::unordered_map<int, std::shared_ptr<JassWeightedPool>> itemPools;
    int nextItemPoolId = 0x19000000;
    std::unordered_map<int, std::shared_ptr<JassWeightedPool>> unitPools;
    int nextUnitPoolId = 0x1A000000;
    // Global shop stock from AddItemToAllStock/AddUnitToAllStock: applied to
    // every existing shop ability immediately. Shops created later need an
    // explicit AddItemToStock (no central shop-creation hook exists yet).
    struct JassGlobalStock {
        uint32_t id = 0;
        int current = 0;
        int max = 0;
        bool isUnit = false;
    };
    std::vector<JassGlobalStock> globalShopStock;

    // ---- Batch 5: camera / cinematic / game-flow / UI state ----
    struct JassCameraNoise {
        float magnitude = 0.0f;
        float velocity = 0.0f;
        bool vertOnly = false;
        bool horizOnly = false;
    };
    JassCameraNoise sourceNoise;
    JassCameraNoise targetNoise;
    struct JassCameraController {
        int unitSimId = -1;
        float xOffset = 0.0f;
        float yOffset = 0.0f;
        bool inheritOrientation = false;
        bool active = false;
    };
    JassCameraController targetController;
    JassCameraController orientController;
    struct JassCameraRotate {
        bool active = false;
        float x = 0, y = 0, radians = 0, duration = 0;
    };
    JassCameraRotate rotateMode;
    struct JassCinematicState {
        bool playing = false;
        std::string scene;
        std::string movie;
        std::string introModel;
        std::string introText;
        bool subtitles = false;
        bool loadDialog = false;
        int portraitUnitId = 0;
        int portraitColor = 0;
        std::string speakerTitle;
        std::string text;
        float sceneDuration = 0.0f;
        float voiceDuration = 0.0f;
    };
    JassCinematicState cinematic;
    bool gameEnded = false;
    bool gameVictory = false;
    bool noDefeatCheat = false;
    bool noVictoryCheat = false;
    bool restartDisabled = false;
    std::unordered_set<int> crippledPlayers;
    std::unordered_map<int, bool> compAIpaused;
    // Live melee AI controllers by player index (ref StartMeleeAI).
    std::unordered_map<int, std::shared_ptr<MeleeAIController>> aiControllers;
    bool syncReady = false; // TriggerSync* needs the net protocol; stays false
    std::unordered_map<int, int> removedPlayerResults;
    struct JassUIConfig {
        bool dragSelect = true;
        bool preSelect = true;
        bool select = true;
        bool userControl = true;
        bool minimapFilterButtons = true;
        bool minimapAllyFilter = true;
        bool minimapCreepFilter = true;
        bool occlusion = true;
        bool worldFogBoundary = true;
        bool showInterface = true;
    };
    JassUIConfig uiConfig;
    bool allyColorFilter = false;
    bool creepCampFilter = false;
    bool dawnDuskEnabled = true; // retail day/night cycle switch
    // Player text messages (ref DisplayTextToPlayer family): queued for the
    // HUD push; duration <= 0 stays until ClearTextMessages.
    struct JassTextMessage {
        int player = -1;
        std::string text;
        float duration = 0.0f;
    };
    std::vector<JassTextMessage> textMessages;
    int textClearSeq = 0; // bumped by ClearTextMessages so the HUD drops shown toasts
    std::unordered_set<int> supportedGameTypes;
    int resourceDensity = 0;
    int creatureDensity = 0;
    std::unordered_map<int, int> volumeGroups; // ordinal -> 0..127
    struct JassMinimapPing {
        float x = 0, y = 0, duration = 0;
        int red = 255, green = 0, blue = 0;
        bool extra = false;
        float age = 0; // seconds since PingMinimap*; aged in UpdateTimers
    };
    std::vector<JassMinimapPing> minimapPings;
    struct JassDefeatCondition {
        std::string description;
    };
    std::unordered_map<int, std::shared_ptr<JassDefeatCondition>> defeatConditions;
    int nextDefeatConditionId = 0x1B000000;

    // Hashtables (ref hashtable API): two cells per key — typed values
    // (int/real/bool/string) and raw handle ints. Keyed by
    // ((int64)parent << 32) | child.
    struct JassHashtable {
        std::unordered_map<int64_t, JassValue> values;
        std::unordered_map<int64_t, int> handles;
    };
    std::unordered_map<int, std::shared_ptr<JassHashtable>> hashtables;
    int nextHashtableId = 0x1D000000;

    // Game caches (ref gamecache API): per-mission string-keyed values
    // plus stored-unit snapshots. SaveGameCache persists to disk.
    struct JassStoredUnit {
        uint32_t typeId = 0;
        int level = 1;
        float lifeFraction = 1.0f;
        int owner = -1;
    };
    struct JassGameCache {
        std::string campaignFile;
        std::unordered_map<std::string, std::unordered_map<std::string, JassValue>> values;
        std::unordered_map<std::string, std::unordered_map<std::string, int>> handles;
        std::unordered_map<std::string, std::unordered_map<std::string, JassStoredUnit>> units;
    };
    std::unordered_map<int, std::shared_ptr<JassGameCache>> gameCaches;
    int nextGameCacheId = 0x1E000000;
    // campaignFile -> newest cache sim id (for reload adoption).
    std::unordered_map<std::string, int> gameCacheIdsByFile;

    // ---- Selection / unit display state (ref SelectUnit and friends) ----
    std::unordered_set<int> selectedUnits; // unit sim ids
    std::unordered_map<int, float> unitBlendTimes;
    std::unordered_set<int> creepGuardUnits;
    struct JassRescueInfo {
        int player = -1;
        bool rescuable = false;
        float range = 0.0f;
    };
    std::unordered_map<int, JassRescueInfo> unitRescue;
    // Pending map switch for the controller (ref ChangeLevel); consumed by
    // GameController::Update which routes it through the runtime reload path.
    std::string pendingChangeLevel;
    bool pendingChangeLevelScore = false;
    bool noReplaySave = false;
    // Campaign progression state (ref campaign API).
    std::unordered_map<int, bool> campaignAvailable;
    std::map<std::pair<int, int>, bool> missionAvailable;
    std::unordered_map<int, bool> edCinematics;
    std::unordered_map<int, bool> opCinematics;
    bool tutorialCleared = false;
    std::unordered_map<int, bool> customCampaignButtons;
    bool forceCampaignSelectScreen = false;
    // Fog-state paint requests (ref SetFogStateRadiusLoc) for the FoW pass.
    struct JassFogRadiusRequest {
        int player = -1;
        int state = 0;
        float x = 0, y = 0, radius = 0;
        bool sharedVision = false;
    };
    std::vector<JassFogRadiusRequest> fogRadiusRequests;

    // Terrain deformations (ref TerrainDeform*): parameters retained and
    // applied in the terrain height pass via TerrainDeformOffset.
    struct JassTerrainDeform {
        int kind = 0; // 0 crater, 1 ripple, 2 wave, 3 random, 4 registration
        float x = 0, y = 0, radius = 0, depth = 0;
        int duration = 0;
        bool permanent = false;
        bool stopped = false;
    };
    std::unordered_map<int, std::shared_ptr<JassTerrainDeform>> terrainDeforms;
    int nextTerrainDeformId = 0x1C000000;
    // Crater is negative, ripple/wave positive, faded to the rim.
    float TerrainDeformOffset(float x, float y) const;

    // Doodad animation requests. The scene plays these as they arrive.
    struct JassDoodadAnim {
        float x = 0, y = 0, radius = 0;
        uint32_t doodadId = 0;
        bool nearestOnly = false;
        std::string animName;
        bool animRandom = false;
    };
    std::vector<JassDoodadAnim> doodadAnims;

    // UnitShareVision feeds the fog sight pass. Indicators are drawn on the
    // minimap. SetUnitFog overrides terrain fog when an end distance is set.
    std::unordered_map<int, std::unordered_set<int>> unitSharedVision;
    struct JassUnitIndicator {
        int red = 255, green = 255, blue = 255, alpha = 255;
    };
    std::unordered_map<int, JassUnitIndicator> unitIndicators;
    struct JassUnitFog {
        float a = 0, b = 0, c = 0, d = 0, e = 0;
    };
    // SetUnitFog takes 5 reals (global unit-fog params, not per unit).
    JassUnitFog globalUnitFog;

    std::unordered_map<int, std::shared_ptr<JassQuest>> quests;
    int nextQuestId = 0x10000000;

    std::unordered_map<int, std::shared_ptr<JassQuestItem>> questItems;
    int nextQuestItemId = 0x11000000;

    std::unordered_map<int, std::shared_ptr<JassAbilityHandle>> abilities;
    int nextAbilityId = 0x0D000000;

    std::unordered_map<int, std::unordered_set<int>> primaryTagSets;
    std::unordered_map<int, std::unordered_set<int>> secondaryTagSets;
    int nextPrimaryTagSetId = 0x0E000000;
    int nextSecondaryTagSetId = 0x0F000000;

    std::unordered_map<int, std::shared_ptr<JassTimer>> timerCallbacks;
    int nextTimerCallbackId = 0x0C000000;

    int configuredPlayerCount = 0;
    std::array<JassStartLocationConfig, 24> startLocations;
    std::array<JassPlayerMapConfig, 24> playerMapConfig;

    JassGameConfig gameConfig;
    std::array<JassPlayerAIState, 24> playerAI;
    JassFogState fog;
    JassTerrainFog terrainFog;
    // Resource trade tax: (player<<32 | other)<<4 | resource -> percent.
    // (ref SetPlayerTaxRate; retail default 0 until set.)
    std::unordered_map<int64_t, int> playerTaxRates;
    // Per-player selectability (ref GetPlayerSelectable; default true).
    std::unordered_map<int, bool> playerSelectable;
    // Integer game states set via SetIntegerGameState (ids other than
    // DIVINE_INTERVENTION=0 / DISCONNECTED=1, which always read 0).
    std::unordered_map<int, int> intGameStates;

    int filterUnitHandle = -1;
    int enumUnitHandle = -1;
    int filterItemHandle = -1;
    int enumItemHandle = -1;
    int filterDestructableHandle = -1;
    int enumDestructableHandle = -1;
    int filterPlayerHandle = -1;
    int enumPlayerHandle = -1;
    float eventDamage = 0;
    int eventDamageSourceHandle = -1;
    int eventAttackingUnitHandle = -1;

    // Spell event state
    int eventSpellAbilityId = 0;
    int eventSpellAbilityUnitHandle = -1;
    float eventSpellTargetX = 0;
    float eventSpellTargetY = 0;
    int eventSpellTargetUnitHandle = -1;

    // General event state
    int eventTriggerPlayerHandle = -1;
    int eventTriggerWidgetHandle = -1;
    int eventEnteringUnitHandle = -1;
    int eventLeavingUnitHandle = -1;
    int eventManipulatingUnitHandle = -1;
    int eventManipulatedItemHandle = -1;
    // Shop transaction context (ref sell/buy events): buyer, seller shop,
    // sold item/unit handles. Sold-unit stays -1 until unit hire lands.
    int eventBuyingUnitHandle = -1;
    int eventSellingUnitHandle = -1;
    int eventSoldItemHandle = -1;
    int eventSoldUnitHandle = -1;
    // Ownership-change context: previous owner player handle.
    int eventChangingUnitPrevOwnerHandle = -1;
    // Construction lifecycle context (ref construct scope).
    int eventConstructedUnitHandle = -1;
    int eventCancelledUnitHandle = -1;
    int eventConstructingUnitHandle = -1;
    // Rescue context: the rescuing unit (GetRescuer).
    int eventRescuerUnitHandle = -1;
    // Queued production type id (train start/cancel observe the type before
    // any unit exists; finish prefers the unit handle).
    int eventTrainedUnitTypeId = 0;
    // Hero skill learning context (ref HERO_SKILL event scope).
    int eventLearnedSkillId = 0;
    int eventLearnedSkillLevel = 0;
    int eventOrderedUnitHandle = -1;
    int eventIssuedOrderId = 0;
    float eventOrderPointX = 0.0f;
    float eventOrderPointY = 0.0f;
    int eventOrderTargetHandle = -1;
    int eventKillingUnitHandle = -1;
    int eventDyingUnitHandle = -1;
    int eventDecayingUnitHandle = -1;
    int eventRevivingUnitHandle = -1;
    int eventTransportUnitHandle = -1;
    int eventLoadedUnitHandle = -1;
    int eventTrainedUnitHandle = -1;
    int eventSummonedUnitHandle = -1;
    int eventSummoningUnitHandle = -1;
    int eventResearchingUnitHandle = -1;
    int eventResearchedId = 0;
    int eventExpiredTimerHandle = -1;
    int eventTrackableHandle = -1;
    // Dialog click + region event state (ref CommonTriggerExecutionScope).
    int eventClickedButtonHandle = -1;
    int eventClickedDialogHandle = -1;
    int eventTriggeringRegionHandle = -1;
    // Chat event scope (ref player chat execution scope).
    std::string eventChatString;
    std::string eventChatMatched;
    // Per-unit ability cooldowns: (unitSim << 32 | abilityCode) -> remaining.
    std::unordered_map<int64_t, float> abilityCooldowns;
    // Units with collision disabled (SetUnitMovementTypeNoCollision).
    std::unordered_set<int> noCollisionUnits;
    // Extended ability flags for JASS ability handles (ref CAbilityJass
    // physical/universal): bit0=physical, bit1=universal, keyed like
    // abilityCooldowns. Hardcoded sim abilities report false unless set.
    std::unordered_map<int64_t, uint8_t> abilityExtFlags;
    // String-keyed user data blobs (ref ability localStore): keyed like
    // abilityCooldowns so values survive handle re-allocation.
    struct JassAbilityUserData {
        std::unordered_map<std::string, int64_t> ints;
        std::unordered_map<std::string, double> reals;
        std::unordered_map<std::string, std::string> strs;
        std::unordered_map<std::string, bool> bools;
        bool empty() const { return ints.empty() && reals.empty() && strs.empty() && bools.empty(); }
    };
    std::unordered_map<int64_t, JassAbilityUserData> abilityUserData;
    // Spell-cast scope (ref CommonTriggerExecutionScope spell fields):
    // order id + target type of the triggering cast, cast-id counter.
    int eventSpellOrderId = 0;
    int eventSpellTargetType = 0;
    int triggerCastId = 0;
    // Preferred selection replacement (ref unitPreferredSelectionReplacement).
    std::unordered_map<int, int> preferredSelectionReplacement; // goneSim -> wantedSim
    // JASS projectiles (ref CJassProjectile): sim-ticked records below.
    struct JassProjectile {
        int casterSimId = -1;
        uint32_t rawcode = 0;
        float x = 0, y = 0, facing = 0, speed = 800.0f;
        bool homing = false;
        float damage = 0.0f;
        bool reflected = false;
        bool done = false;
        bool hasUnitTarget = false;
        int targetSimId = -1;
        bool hasPointTarget = false;
        float tx = 0, ty = 0;
        float traveled = 0.0f;
    };
    std::unordered_map<int, std::shared_ptr<JassProjectile>> projectiles;
    int nextProjectileId = 0x20000000;
    // Target-type sets (ref EnumSet<CTargetType> targeting handles):
    // stored as ref-ordinal ints (see ParseTargetType order).
    std::unordered_map<int, std::unordered_set<int>> targetTypeSets;
    int nextTargetTypeSetId = 0x21000000;
    // Non-stacking stat bonuses (ref NonStackingStatBuff): aura-mappable
    // types ride CUnit::AddAuraStatBonus slots; MAXHP/MAXMP use JASS-side
    // strongest-wins below; HPSTEAL/THORNS have no unit mutator (stored).
    struct JassStatBonus {
        int type = 0; // NonStackingStatBuffType ordinal
        std::string key;
        float value = 0.0f;
        std::unordered_set<int> holders; // unitSimIds
    };
    std::unordered_map<int, std::shared_ptr<JassStatBonus>> statBonuses;
    int nextStatBonusId = 0x22000000;
    // Strongest-wins applied MAXHP/MAXMP per unit (type -> absolute amount).
    std::unordered_map<int64_t, float> maxStatApplied; // (unitSim<<32|type)->applied
    // State mods (ref StateModBuff): mapped types drive unit flags with
    // OR-recompute over active records; unmapped types are stored.
    struct JassStateMod {
        int type = 0; // StateModBuffType ordinal
        int value = 0;
        std::unordered_set<int> holders; // unitSimIds
    };
    std::unordered_map<int, std::shared_ptr<JassStateMod>> stateMods;
    int nextStateModId = 0x23000000;
    // Generic script containers (ref HandleList/StringList/IntExpr).
    std::unordered_map<int, std::vector<JassValue>> handleLists;
    int nextHandleListId = 0x24000000;
    std::unordered_map<int, std::vector<std::string>> stringLists;
    int nextStringListId = 0x25000000;
    std::unordered_map<int, std::string> intExprs; // sim id -> func name
    int nextIntExprId = 0x26000000;
    // Looped ability sounds: (unitSim << 32 | aliasCode) -> audio id.
    std::unordered_map<int64_t, int> loopedSpellSounds;
    // Cutscene time override (ref false time of day): clock frozen at the
    // given hour while remaining ticks down, then prior suspend restored.
    struct JassFalseTime {
        float remaining = 0.0f;
        bool wasSuspended = false;
    };
    JassFalseTime falseTime;
    // Current ForFiles enumeration path (ref enumFileScope).
    std::string enumFilePath;
    // Archive file listing injected by the host (ref dataSource listfile):
    // pattern (StormLib wildcards) -> matching paths, original case.
    std::function<std::vector<std::string>(const std::string& pattern)> fileLister;
    void SetFileLister(std::function<std::vector<std::string>(const std::string&)> fn) {
        fileLister = std::move(fn);
    }
    // Script file bytes injected by the host (ref dataSource streams).
    std::function<std::vector<uint8_t>(const std::string& path)> scriptReader;
    void SetScriptReader(std::function<std::vector<uint8_t>(const std::string&)> fn) {
        scriptReader = std::move(fn);
    }
    // Per-tick trigger interest (ref registerOnTickEvent): trigger jass
    // handles fired once per UpdateTimers pass, plus unit-bound variants.
    std::unordered_set<int> onTickTriggers;
    std::unordered_map<int, int> onUnitTickTriggers; // triggerHandle -> unitSimId

    // FDF frame handles (ref Jass2 CreateFrame/CreateSimpleFrame/GetFrameByName
    // + FrameSet*/SkinGetField/LoadTOCFile). Stored sim-side so custom UI
    // scripts resolve; rendering still goes through FrameBuilder/GameUI.
    struct JassFrameAnchor {
        int point = 0;
        float x = 0, y = 0;
        bool absolute = false;
        int otherHandle = 0;
        int otherPoint = 0;
    };
    struct JassFrame {
        std::string templateName;
        std::string name;
        int ownerHandle = 0;
        int context = 0;
        int priority = 0;
        bool simple = false;
        std::string text;
        uint32_t textColor = 0xFFFFFFFF;
        std::vector<JassFrameAnchor> anchors;
        bool positioned = false;
        // Blz frame state (retail getters read back what setters store;
        // rendering stays a later pass, same as the legacy FrameSet* set).
        bool visible = true;
        bool enabled = true;
        bool focused = false;
        bool cageMouse = false;
        int alpha = 255;
        int textSizeLimit = 0;
        int level = 0;
        float value = 0.0f;
        float minValue = 0.0f;
        float maxValue = 0.0f;
        float stepSize = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        float scale = 1.0f;
        uint32_t vertexColor = 0xFFFFFFFF;
        std::string model;
        int modelCamera = 0;
        std::string texture;
        int textureFlag = 0;
        bool textureBlend = false;
        int spritePrimaryProp = 0;
        int spriteFlags = 0;
        int parentSimId = -1;
        int tooltipSimId = -1;
        std::vector<int> childSimIds; // sim ids, insertion order
    };
    std::unordered_map<int, std::shared_ptr<JassFrame>> frames;
    int nextFrameId = 0x27000000;
    std::unordered_map<std::string, int> framesByName; // name -> sim id
    std::unordered_map<std::string, std::string> skinFields;
    std::string skinName;
    std::vector<std::string> loadedTocFiles;
    bool uiAutoPosition = true;
    bool hideOriginFrames = false;

    // Warsmash LocalStore (ref CreateLocalStore/SetLocalStore*/GetLocalStore*):
    // string-keyed typeless bag per handle so AB/config scripts round-trip.
    struct JassLocalStore {
        std::unordered_map<std::string, int> ints;
        std::unordered_map<std::string, double> reals;
        std::unordered_map<std::string, std::string> strs;
        std::unordered_map<std::string, bool> bools;
        std::unordered_map<std::string, int> handles;
        std::unordered_map<std::string, int> codes;
    };
    std::unordered_map<int, std::shared_ptr<JassLocalStore>> localStores;
    int nextLocalStoreId = 0x28000000;

    // Callbacks for external system access (set from main.cpp)
    std::function<float(float, float)> terrainGroundHeight;
    void SetTerrainHeightCallback(std::function<float(float, float)> cb) {
        terrainGroundHeight = std::move(cb);
    }
    // Terrain-fog override notification (set by the game controller to push
    // JassTerrainFog into DayNightCycle; null when headless).
    std::function<void()> terrainFogHook;
    void SetLocalPlayerIndex(int index) { localPlayerIndex = index; }
    std::function<void(float,float,float)> camSetPosition;
    std::function<void(float,float,float)> camSetTarget;
    std::function<void()> camReset;
    std::function<float()> camGetX;
    std::function<float()> camGetY;
    std::function<float()> camGetZ;
    std::function<float()> camGetEyeX;
    std::function<float()> camGetEyeY;
    std::function<float()> camGetEyeZ;
    std::function<float()> camGetField;
    std::function<void(int,float)> camSetField;

    // Animation callback
    std::function<void(int,const std::string&)> unitSetAnimation;
    std::function<void(int,const std::string&)> unitQueueAnimation;
    std::function<void(int,int)> unitSetAnimationByIndex;
    std::function<void(int,int,float)> unitSetAnimationByIndexEx;
    std::function<void(int,const std::string&,const std::vector<std::string>&,float,float,bool)> unitSetAnimationByTags;
    std::function<void(int,const std::string&,const std::vector<std::string>&)> unitQueueAnimationByTags;
    std::function<void(int,const std::string&,bool)> unitAddAnimationProperties;
    std::function<bool(int,const std::string&,bool)> unitSetAnimationSecondaryTag;
    std::function<void(int)> unitForceResetAnimation;
    std::function<void(int)> unitClearAnimations;

    // Camera field enum mapping
    int GetCameraFieldId(const std::string& name) const;
};


struct JassQueuedContext {
    int currentTriggerId = -1;
    int currentTriggerUnitHandle = -1;
    int currentTriggerEventId = 0;
    int filterUnitHandle = -1;
    int enumUnitHandle = -1;
    int filterItemHandle = -1;
    int enumItemHandle = -1;
    int filterDestructableHandle = -1;
    int enumDestructableHandle = -1;
    int filterPlayerHandle = -1;
    int enumPlayerHandle = -1;
    float eventDamage = 0.0f;
    int eventDamageSourceHandle = -1;
    int eventAttackingUnitHandle = -1;
    int eventSpellAbilityId = 0;
    int eventSpellAbilityUnitHandle = -1;
    float eventSpellTargetX = 0.0f;
    float eventSpellTargetY = 0.0f;
    int eventSpellTargetUnitHandle = -1;
    int eventTriggerPlayerHandle = -1;
    int eventTriggerWidgetHandle = -1;
    int eventEnteringUnitHandle = -1;
    int eventLeavingUnitHandle = -1;
    int eventManipulatingUnitHandle = -1;
    int eventManipulatedItemHandle = -1;
    // Shop transaction context (ref sell/buy events): buyer, seller shop,
    // sold item/unit handles. Sold-unit stays -1 until unit hire lands.
    int eventBuyingUnitHandle = -1;
    int eventSellingUnitHandle = -1;
    int eventSoldItemHandle = -1;
    int eventSoldUnitHandle = -1;
    // Ownership-change context: previous owner player handle.
    int eventChangingUnitPrevOwnerHandle = -1;
    // Construction lifecycle context (ref construct scope).
    int eventConstructedUnitHandle = -1;
    int eventCancelledUnitHandle = -1;
    int eventConstructingUnitHandle = -1;
    // Rescue context: the rescuing unit (GetRescuer).
    int eventRescuerUnitHandle = -1;
    // Queued production type id (train start/cancel observe the type before
    // any unit exists; finish prefers the unit handle).
    int eventTrainedUnitTypeId = 0;
    // Hero skill learning context (ref HERO_SKILL event scope).
    int eventLearnedSkillId = 0;
    int eventLearnedSkillLevel = 0;
    int eventOrderedUnitHandle = -1;
    int eventIssuedOrderId = 0;
    float eventOrderPointX = 0.0f;
    float eventOrderPointY = 0.0f;
    int eventOrderTargetHandle = -1;
    int eventKillingUnitHandle = -1;
    int eventDyingUnitHandle = -1;
    int eventDecayingUnitHandle = -1;
    int eventRevivingUnitHandle = -1;
    int eventTransportUnitHandle = -1;
    int eventLoadedUnitHandle = -1;
    int eventTrainedUnitHandle = -1;
    int eventSummonedUnitHandle = -1;
    int eventSummoningUnitHandle = -1;
    int eventResearchingUnitHandle = -1;
    int eventResearchedId = 0;
    int eventExpiredTimerHandle = -1;
    int eventTrackableHandle = -1;
    int eventClickedButtonHandle = -1;
    int eventClickedDialogHandle = -1;
    int eventTriggeringRegionHandle = -1;
    std::string eventChatString;
    std::string eventChatMatched;
    int eventSpellOrderId = 0;
    int eventSpellTargetType = 0;
    int triggerCastId = 0;
};

struct JassQueuedThread {
    std::string functionName;
    JassQueuedContext context;
    JassVM::ThreadState vmState;
    float sleepRemaining = 0.0f;
    // TriggerExecuteWait join (0 = none): the woken caller must finish the
    // callee remainder before resuming; see ContinueTriggerJoin.
    int joinId = 0;
};

// TriggerExecuteWait continuation (ref: caller suspends until the callee's
// waits elapse). Holds the suspended callee child plus not-yet-run actions.
struct JassTriggerJoin {
    int calleeTriggerId = -1;
    JassQueuedContext calleeContext;
    std::vector<std::string> pendingActions;
    JassVM::ThreadState suspendedChild;
    bool childActive = false;
};

class JassNatives {
public:
    JassNatives(JassVM& vm, CSimulation* sim = nullptr);
    void SetSimulation(CSimulation* sim) { sim_ = sim; }
    void SetJassVM(JassVM* vm) { vmPtr_ = vm; }
    void SetData(std::shared_ptr<JassNativesData> data) { data_ = data; }

    void RegisterAll();
    void RegisterConfigNatives();
    void RegisterCommonNatives();

    void FireUnitEvent(int unitSimHandle, int eventType, int playerEventType = -1);
    void FireDeathEvent(int unitSimHandle, int killerSimHandle, int eventType);
    // Corpse begins decaying (retail EVENT_UNIT_DECAY, unit + player 54/21).
    void FireDecayEvent(int unitSimHandle);
    void FireDamageEvent(int unitSimHandle, int sourceSimHandle, float damage, int eventType);
    void FireOrderEvent(int unitSimHandle, int orderId, int targetSimHandle, float x, float y);
    void FireItemEvent(int unitSimHandle, int itemSimHandle, int eventType);
    void FireSellItemEvent(int buyerSimHandle, int sellerSimHandle, int itemSimHandle);
    void FireSellUnitEvent(int buyerSimHandle, int sellerSimHandle, int hiredSimHandle);
    void FirePawnItemEvent(int sellerSimHandle, int shopSimHandle, int itemSimHandle);
    void FireHeroSkillEvent(int unitSimHandle, int skillId, int newLevel);
    void FireConstructFinishEvent(int buildingSimHandle);
    void FireConstructCancelEvent(int buildingSimHandle);
    void FireTrainStartEvent(int buildingSimHandle, int typeId);
    void FireTrainCancelEvent(int buildingSimHandle, int typeId);
    void FireResearchStartEvent(int buildingSimHandle, int upgradeId);
    void FireResearchCancelEvent(int buildingSimHandle, int upgradeId);
    void FireUpgradeStartEvent(int buildingSimHandle, int upgradeId);
    void FireUpgradeFinishEvent(int buildingSimHandle, int upgradeId);
    void FireChangeOwnerEvent(int unitSimHandle, int prevOwnerPlayerIndex);
    void FireRegionEvent(int regionSimId, int unitSimHandle, bool isEnter);
    void FireTrainedEvent(int buildingSimHandle, int trainedSimHandle);
    void FireSummonEvent(int summonerSimHandle, int summonedSimHandle);
    void FireResearchEvent(int unitSimHandle, int upgradeId);
    void FireReviveEvent(int reviverSimHandle, int revivedSimHandle);
    void FireRevivableEvent(int heroSimHandle);
    void FireReviveStartEvent(int buildingSimHandle, int heroSimHandle);
    void FireReviveCancelEvent(int buildingSimHandle, int heroSimHandle);
    void FireSpellEvent(int casterSimHandle, int stage);
    void FireTrackableEvent(int trackableHandle, int eventCode);
    void NotifyTrackableClick(float x, float y);
    void FireDialogButtonEvent(int buttonJassHandle);
    void FireChatEvent(int fromPlayerHandle, const std::string& message);
    void FireAllianceChange(int playerIdA, int playerIdB);
    void FireCinematicSkipEvent(int playerIndex);
    void FireRescueEvent(int rescuedSimHandle, int rescuerSimHandle);
    // Transport boarding: trigger unit is the passenger (ref rescue shape);
    // GetTransportUnit/GetLoadedUnit read the dedicated fields.
    void FireLoadedEvent(int transportSimHandle, int passengerSimHandle);
    void UpdateRegions();
    void UpdateRescue();
    void UpdateTextTags(float dt);
    void ForgetItem(int itemSimHandle);
    // Advances live melee AI controllers one sim tick.
    void UpdateAI();

    int UpdateTimers(float dt);
    void QueueFunction(const std::string& functionName);
    void QueueFunctionWithContext(const std::string& functionName, const JassQueuedContext& context);
    int PumpQueuedThreads(int maxCount = 1024);
    bool HasQueuedThreads() const { return !queuedThreads_.empty() || !sleepingThreads_.empty(); }
    bool HasFunction(const std::string& functionName) const { return vm_.HasFunction(functionName); }

    std::shared_ptr<JassNativesData> GetData() const { return data_; }

private:
    JassQueuedContext CaptureContext() const;
    void ApplyContext(const JassQueuedContext& context);
    bool EvaluateTriggerConditions(JassTrigger& trigger, const JassQueuedContext& context,
                                   bool requireEnabled = true);
    void QueueTriggerActions(JassTrigger& trigger, const JassQueuedContext& context);
    void UpdateTrackables();
    void UpdateProjectiles(float dt);
    void FireTickTriggers();
    void FireStateTriggers();
    // Strongest-wins recompute for JASS-side MAXHP/MAXMP bonus records.
    void RecomputeMaxStatBonus(int unitSim, int type);
    // OR-recompute for mapped state-mod flags from active records.
    void RecomputeStateMod(int unitSim, int type);
    void RegisterHandleNatives();
    void RegisterMathNatives();
    void RegisterConversionNatives();
    void RegisterRandomNatives();
    void RegisterTimerNatives();
    void RegisterGroupNatives();
    void RegisterForceNatives();
    void RegisterLocationNatives();
    void RegisterRectNatives();
    void RegisterRegionNatives();
    void RegisterUnitNatives();
    void RegisterWidgetNatives();
    void RegisterPlayerNatives();
    void RegisterHeroNatives();
    void RegisterItemNatives();
    void RegisterDestructableNatives();
    void RegisterTriggerNatives();
    void RegisterEventNatives();
    void RegisterSoundNatives();
    void RegisterTextTagNatives();
    void RegisterEffectNatives();
    void RegisterFogNatives();
    void RegisterWeatherNatives();
    void RegisterLightningNatives();
    void RegisterCameraNatives();
    void RegisterGameNatives();

    void RegisterOrderIdNatives();
    void RegisterTerrainQueryNatives();
    void RegisterPlayerTeamNatives();
    void RegisterVisualNatives();
    void RegisterPoolStockNatives();
    void RegisterUnitStateNatives();
    void RegisterCameraFlowNatives();
    void RegisterTriggerSoundNatives();
    void RegisterUIStateNatives();
    void RegisterBoardNatives();
    void RegisterTerrainEditNatives();
    void RegisterHashtableNatives();
    void RegisterGameCacheNatives();
    void RegisterProjectileNatives();
    void RegisterTargetingNatives();
    void RegisterBonusNatives();
    void RegisterMiscNatives();
    void RegisterFrameNatives();
    void RegisterAbilityBuilderNatives();

    void RegisterConfigPlayerNatives();

    std::shared_ptr<JassNativesData> data_;
    JassVM& vm_;
    JassVM* vmPtr_ = nullptr;
    CSimulation* sim_ = nullptr;
    std::deque<JassQueuedThread> queuedThreads_;
    std::deque<JassQueuedThread> sleepingThreads_;
    // TriggerExecuteWait joins by id; the thread currently driven by
    // PumpQueuedThreads (null outside it) so the native can attach a join.
    std::unordered_map<int, JassTriggerJoin> triggerJoins_;
    int nextJoinId_ = 1;
    JassQueuedThread* activePumpThread_ = nullptr;
    // Runs a woken join to completion-or-next-sleep. Returns true when the
    // join finished and the caller thread may resume; false when the caller
    // was re-suspended (thread moved back to sleepingThreads_).
    bool ContinueTriggerJoin(JassQueuedThread& thread);
    // Engine-side region membership (region sim id -> unit sim handles inside).
    // Used by UpdateRegions() to fire enter/leave triggers on migration.
    std::unordered_map<int, std::unordered_set<int>> regionMembership_;
};
