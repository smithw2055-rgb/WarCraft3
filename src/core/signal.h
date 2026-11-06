#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include <algorithm>
#include <utility>

using SignalConnectionId = uint64_t;

// ============================================================
// Signal<Args...> - Godot-inspired multicast signal template.
//
// Supports multiple independent subscribers (via Connect/Disconnect)
// while preserving 100% backward compatibility with legacy single-slot
// Set*Callback / Get*Callback call sites via SetLegacyCallback() and
// GetDispatcher().
// ============================================================
template <typename... Args>
class Signal {
public:
    using Callback = std::function<void(Args...)>;

    Signal() { RefreshDispatcher(); }

    Signal(const Signal& other)
        : nextId_(other.nextId_)
        , legacyCallback_(other.legacyCallback_)
        , slots_(other.slots_) {
        RefreshDispatcher();
    }

    Signal& operator=(const Signal& other) {
        if (this != &other) {
            nextId_ = other.nextId_;
            legacyCallback_ = other.legacyCallback_;
            slots_ = other.slots_;
            RefreshDispatcher();
        }
        return *this;
    }

    Signal(Signal&& other) noexcept
        : nextId_(other.nextId_)
        , legacyCallback_(std::move(other.legacyCallback_))
        , slots_(std::move(other.slots_)) {
        RefreshDispatcher();
        other.RefreshDispatcher();
    }

    Signal& operator=(Signal&& other) noexcept {
        if (this != &other) {
            nextId_ = other.nextId_;
            legacyCallback_ = std::move(other.legacyCallback_);
            slots_ = std::move(other.slots_);
            RefreshDispatcher();
            other.RefreshDispatcher();
        }
        return *this;
    }

    // Subscribe a multicast listener. Returns a connection handle for Disconnect().
    SignalConnectionId Connect(Callback cb) {
        if (!cb) return 0;
        const SignalConnectionId id = nextId_++;
        slots_.push_back({id, std::move(cb)});
        RefreshDispatcher();
        return id;
    }

    // Unsubscribe a previously connected listener.
    bool Disconnect(SignalConnectionId id) {
        if (id == 0) return false;
        auto it = std::remove_if(slots_.begin(), slots_.end(),
                                 [id](const Slot& s) { return s.id == id; });
        if (it == slots_.end()) return false;
        slots_.erase(it, slots_.end());
        RefreshDispatcher();
        return true;
    }

    // Clear all multicast subscribers (preserves or clears legacy slot based on flag).
    void Clear(bool clearLegacy = true) {
        slots_.clear();
        if (clearLegacy) legacyCallback_ = nullptr;
        RefreshDispatcher();
    }

    // Legacy single-slot compatibility (used by CSimulation::Set*Callback).
    void SetLegacyCallback(Callback cb) {
        legacyCallback_ = std::move(cb);
        RefreshDispatcher();
    }

    // Emit signal to legacy slot (if set) and all multicast subscribers in order.
    void Emit(Args... args) const {
        if (legacyCallback_) {
            legacyCallback_(args...);
        }
        if (!slots_.empty()) {
            // Snapshot slots in case a listener connects/disconnects during emission.
            const auto snapshot = slots_;
            for (const auto& slot : snapshot) {
                if (slot.cb) slot.cb(args...);
            }
        }
    }

    void operator()(Args... args) const {
        Emit(args...);
    }

    bool HasSubscribers() const {
        return static_cast<bool>(legacyCallback_) || !slots_.empty();
    }

    size_t SubscriberCount() const {
        return slots_.size() + (legacyCallback_ ? 1u : 0u);
    }

    // Returns a const std::function<void(Args...)>& that evaluates to true
    // iff HasSubscribers() is true, and dispatches to Emit() when called.
    const Callback& GetDispatcher() const {
        return dispatcher_;
    }

private:
    struct Slot {
        SignalConnectionId id = 0;
        Callback cb;
    };

    void RefreshDispatcher() {
        if (HasSubscribers()) {
            dispatcher_ = [this](Args... args) { this->Emit(args...); };
        } else {
            dispatcher_ = nullptr;
        }
    }

    SignalConnectionId nextId_ = 1;
    Callback legacyCallback_;
    std::vector<Slot> slots_;
    Callback dispatcher_;
};
