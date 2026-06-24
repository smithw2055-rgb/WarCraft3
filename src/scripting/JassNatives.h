#pragma once
#include "JassVM.h"
#include "resource/slk.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include <array>
#include <deque>

// Forward declaration (avoid circular include)
class CSimulation;
class CItem;

// JASS event type constants (matching Blizzard common.j / JassGameEventsWar3 ordinals)
enum JassEventType {
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
    JASS_EVENT_UNIT_SUMMON = 84,
    JASS_EVENT_UNIT_DROP_ITEM = 85,
    JASS_EVENT_UNIT_PICKUP_ITEM = 86,
    JASS_EVENT_UNIT_USE_ITEM = 87,
    JASS_EVENT_UNIT_LOADED = 88,
    JASS_EVENT_WIDGET_DEATH = 89,
};

struct JassTrigger {
    int id = -1;
    std::vector<int> registeredUnitHandles;
    std::vector<int> eventTypes;
    std::vector<std::string> actionFuncNames;
    std::vector<std::pair<int, int>> registeredEvents;
};

struct JassTimer {
    float remaining = 0;
    float timeout = 0;
    bool periodic = false;
    bool paused = false;
    std::string callbackFunc;
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
};

struct JassSound {
    std::string label;
    float volume = 1.0f;
    float pitch = 1.0f;
    bool looping = false;
};

struct JassTextTag {
    std::string text;
    float x = 0, y = 0;
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
};

struct JassHandleInfo {
    enum Type {
        UNIT, PLAYER, TRIGGER, TIMER, LOCATION, GROUP, FORCE,
        ITEM, DESTRUCTABLE, RECT, REGION, SOUND, TEXT_TAG,
        EFFECT, LIGHTNING, WEATHER, FOG_MODIFIER, QUEST
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

    std::unordered_map<int, std::shared_ptr<JassTimer>> timers;
    int nextTimerId = 0x03000000;

    std::unordered_map<int, std::shared_ptr<JassGroup>> groups;
    int nextGroupId = 0x04000000;

    std::unordered_map<int, std::shared_ptr<JassForce>> forces;
    int nextForceId = 0x05000000;

    std::unordered_map<int, CItem*> items;

    std::unordered_map<int, std::shared_ptr<JassLocation>> locations;
    int nextLocId = 0x06000000;

    std::unordered_map<int, std::shared_ptr<JassRect>> rects;
    int nextRectId = 0x07000000;

    std::unordered_map<int, std::shared_ptr<JassRegion>> regions;
    int nextRegionId = 0x08000000;

    std::unordered_map<int, std::shared_ptr<JassSound>> sounds;
    int nextSoundId = 0x09000000;

    std::unordered_map<int, std::shared_ptr<JassTextTag>> textTags;
    int nextTextTagId = 0x0A000000;

    std::unordered_map<int, std::shared_ptr<JassEffect>> effects;
    int nextEffectId = 0x0B000000;

    std::unordered_map<int, std::shared_ptr<JassTimer>> timerCallbacks;
    int nextTimerCallbackId = 0x0C000000;

    int configuredPlayerCount = 0;
    std::array<JassStartLocationConfig, 24> startLocations;
    std::array<JassPlayerMapConfig, 24> playerMapConfig;

    int filterUnitHandle = -1;
    int enumUnitHandle = -1;
    int filterItemHandle = -1;
    int enumItemHandle = -1;
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
    int eventKillingUnitHandle = -1;
    int eventDyingUnitHandle = -1;
    int eventRevivingUnitHandle = -1;
    int eventTransportUnitHandle = -1;
    int eventLoadedUnitHandle = -1;

    // Callbacks for external system access (set from main.cpp)
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
    std::function<void(int,const std::string&,bool)> unitAddAnimationProperties;
    std::function<void(int)> unitClearAnimations;

    // Camera field enum mapping
    int GetCameraFieldId(const std::string& name) const;
};

struct JassQueuedContext {
    int currentTriggerId = -1;
    int currentTriggerUnitHandle = -1;
    int filterUnitHandle = -1;
    int enumUnitHandle = -1;
    int filterItemHandle = -1;
    int enumItemHandle = -1;
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
    int eventKillingUnitHandle = -1;
    int eventDyingUnitHandle = -1;
    int eventRevivingUnitHandle = -1;
    int eventTransportUnitHandle = -1;
    int eventLoadedUnitHandle = -1;
};

struct JassQueuedThread {
    std::string functionName;
    JassQueuedContext context;
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

    void FireUnitEvent(int unitSimHandle, int eventType);
    void FireDeathEvent(int unitSimHandle, int killerSimHandle, int eventType);
    void FireDamageEvent(int unitSimHandle, int sourceSimHandle, float damage, int eventType);
    void FireOrderEvent(int unitSimHandle, int orderId, int targetSimHandle, float x, float y);
    void FireItemEvent(int unitSimHandle, int itemSimHandle, int eventType);

    int UpdateTimers(float dt);
    void QueueFunction(const std::string& functionName);
    void QueueFunctionWithContext(const std::string& functionName, const JassQueuedContext& context);
    int PumpQueuedThreads(int maxCount = 1024);
    bool HasQueuedThreads() const { return !queuedThreads_.empty(); }
    bool HasFunction(const std::string& functionName) const { return vm_.HasFunction(functionName); }

    std::shared_ptr<JassNativesData> GetData() const { return data_; }

private:
    JassQueuedContext CaptureContext() const;
    void ApplyContext(const JassQueuedContext& context);
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

    void RegisterConfigPlayerNatives();

    std::shared_ptr<JassNativesData> data_;
    JassVM& vm_;
    JassVM* vmPtr_ = nullptr;
    CSimulation* sim_ = nullptr;
    std::deque<JassQueuedThread> queuedThreads_;
};
