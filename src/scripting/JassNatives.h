#pragma once
#include "JassVM.h"
#include "resource/slk.h"
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
    JASS_EVENT_UNIT_HERO_REVIVE_FINISH = 83,
    JASS_EVENT_UNIT_SUMMON = 84,
    JASS_EVENT_UNIT_DROP_ITEM = 85,
    JASS_EVENT_UNIT_PICKUP_ITEM = 86,
    JASS_EVENT_UNIT_USE_ITEM = 87,
    JASS_EVENT_UNIT_LOADED = 88,
    JASS_EVENT_WIDGET_DEATH = 89,
    // Player unit events (TriggerRegisterPlayerUnitEvent) — ConvertPlayerUnitEvent ids.
    JASS_EVENT_PLAYER_UNIT_DEATH = 20,
    JASS_EVENT_PLAYER_UNIT_TRAIN_FINISH = 34,
    JASS_EVENT_PLAYER_UNIT_RESEARCH_FINISH = 37,
    JASS_EVENT_PLAYER_UNIT_ISSUED_ORDER = 38,
    JASS_EVENT_PLAYER_UNIT_ISSUED_POINT_ORDER = 39,
    JASS_EVENT_PLAYER_UNIT_ISSUED_TARGET_ORDER = 40,
    JASS_EVENT_PLAYER_HERO_REVIVE_FINISH = 46,
    JASS_EVENT_PLAYER_UNIT_SUMMON = 47,
    JASS_EVENT_PLAYER_UNIT_DROP_ITEM = 48,
    JASS_EVENT_PLAYER_UNIT_PICKUP_ITEM = 49,
    JASS_EVENT_PLAYER_UNIT_USE_ITEM = 50,
    JASS_EVENT_PLAYER_UNIT_LOADED = 51,
};

struct JassTrigger {
    int id = -1;
    bool enabled = true;
    std::vector<int> registeredUnitHandles;
    std::vector<int> eventTypes;
    std::vector<std::string> conditionFuncNames;
    std::vector<std::string> actionFuncNames;
    std::vector<std::pair<int, int>> registeredEvents;
    // Unit-state events (ref TriggerRegisterUnitStateEvent): fired by the
    // trigger poll pass once wired; retained so registration never loses.
    struct JassUnitStateEvent {
        int unitHandle = 0;
        int state = 0;
        int opcode = 0;
        float limit = 0.0f;
    };
    std::vector<JassUnitStateEvent> unitStateEvents;
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
};

struct JassFogState {
    bool fogEnabled = true;
    bool maskEnabled = true;
    int maskStyle = 0;
    float zHeight = 0.0f;
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
    float height = 0;
    float lifespan = 0;
    float fadepoint = 0;
    int color = 0xFFFFFFFF;
    bool permanent = false;
    bool visible = true;
    bool suspended = false;
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

// Billboard image (ref image API): full state retained for the renderer;
// render consumption lands with the UI/scene image pass.
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

// Clickable trackable (ref trackable API): firing hit/track events needs
// the render picking path; registration below stores trigger interest.
struct JassTrackable {
    std::string modelPath;
    float x = 0, y = 0, facing = 0;
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
        IMAGE, TRACKABLE, UBERSPLAT
    };
    Type type;
    int id;
};

struct JassStartLocationConfig {
    float x = 0.0f;
    float y = 0.0f;
    bool defined = false;
};

struct JassPlayerMapConfig {
    int startLocationIndex = -1;
    bool slotActive = false;
    int team = -1;
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
    bool triggerWaitOnSleeps = true;
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
    std::unordered_set<int> supportedGameTypes;
    int resourceDensity = 0;
    int creatureDensity = 0;
    std::unordered_map<int, int> volumeGroups; // ordinal -> 0..127
    struct JassMinimapPing {
        float x = 0, y = 0, duration = 0;
        int red = 255, green = 0, blue = 0;
        bool extra = false;
    };
    std::vector<JassMinimapPing> minimapPings;
    struct JassDefeatCondition {
        std::string description;
    };
    std::unordered_map<int, std::shared_ptr<JassDefeatCondition>> defeatConditions;
    int nextDefeatConditionId = 0x1B000000;

    // Terrain deformations (ref TerrainDeform*): parameters retained for
    // the terrain render pass; no mesh effect yet (documented).
    struct JassTerrainDeform {
        int kind = 0; // 0 crater, 1 ripple, 2 wave, 3 random, 4 registration
        float x = 0, y = 0, radius = 0, depth = 0;
        int duration = 0;
        bool permanent = false;
        bool stopped = false;
    };
    std::unordered_map<int, std::shared_ptr<JassTerrainDeform>> terrainDeforms;
    int nextTerrainDeformId = 0x1C000000;

    // Doodad animation requests (ref SetDoodadAnimation): retained for the
    // scene doodad pass; no consumer yet (documented).
    struct JassDoodadAnim {
        float x = 0, y = 0, radius = 0;
        uint32_t doodadId = 0;
        bool nearestOnly = false;
        std::string animName;
        bool animRandom = false;
    };
    std::vector<JassDoodadAnim> doodadAnims;

    // Unit-attached visual/vision state without a sim consumer yet (ref
    // UnitShareVision/UnitAddIndicator/SetUnitFog): retained per unit sim id
    // for the renderer/FoW passes to consume.
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
    int eventOrderedUnitHandle = -1;
    int eventIssuedOrderId = 0;
    float eventOrderPointX = 0.0f;
    float eventOrderPointY = 0.0f;
    int eventOrderTargetHandle = -1;
    int eventKillingUnitHandle = -1;
    int eventDyingUnitHandle = -1;
    int eventRevivingUnitHandle = -1;
    int eventTransportUnitHandle = -1;
    int eventLoadedUnitHandle = -1;
    int eventTrainedUnitHandle = -1;
    int eventSummonedUnitHandle = -1;
    int eventSummoningUnitHandle = -1;
    int eventResearchingUnitHandle = -1;
    int eventResearchedId = 0;
    int eventExpiredTimerHandle = -1;

    // Callbacks for external system access (set from main.cpp)
    std::function<float(float, float)> terrainGroundHeight;
    void SetTerrainHeightCallback(std::function<float(float, float)> cb) {
        terrainGroundHeight = std::move(cb);
    }
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
    int eventOrderedUnitHandle = -1;
    int eventIssuedOrderId = 0;
    float eventOrderPointX = 0.0f;
    float eventOrderPointY = 0.0f;
    int eventOrderTargetHandle = -1;
    int eventKillingUnitHandle = -1;
    int eventDyingUnitHandle = -1;
    int eventRevivingUnitHandle = -1;
    int eventTransportUnitHandle = -1;
    int eventLoadedUnitHandle = -1;
    int eventTrainedUnitHandle = -1;
    int eventSummonedUnitHandle = -1;
    int eventSummoningUnitHandle = -1;
    int eventResearchingUnitHandle = -1;
    int eventResearchedId = 0;
    int eventExpiredTimerHandle = -1;
};

struct JassQueuedThread {
    std::string functionName;
    JassQueuedContext context;
    JassVM::ThreadState vmState;
    float sleepRemaining = 0.0f;
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
    void FireDamageEvent(int unitSimHandle, int sourceSimHandle, float damage, int eventType);
    void FireOrderEvent(int unitSimHandle, int orderId, int targetSimHandle, float x, float y);
    void FireItemEvent(int unitSimHandle, int itemSimHandle, int eventType);
    void FireRegionEvent(int regionSimId, int unitSimHandle, bool isEnter);
    void FireTrainedEvent(int buildingSimHandle, int trainedSimHandle);
    void FireSummonEvent(int summonerSimHandle, int summonedSimHandle);
    void FireResearchEvent(int unitSimHandle, int upgradeId);
    void FireReviveEvent(int reviverSimHandle, int revivedSimHandle);
    void UpdateRegions();
    void ForgetItem(int itemSimHandle);

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

    void RegisterConfigPlayerNatives();

    std::shared_ptr<JassNativesData> data_;
    JassVM& vm_;
    JassVM* vmPtr_ = nullptr;
    CSimulation* sim_ = nullptr;
    std::deque<JassQueuedThread> queuedThreads_;
    std::deque<JassQueuedThread> sleepingThreads_;
    // Engine-side region membership (region sim id -> unit sim handles inside).
    // Used by UpdateRegions() to fire enter/leave triggers on migration.
    std::unordered_map<int, std::unordered_set<int>> regionMembership_;
};
