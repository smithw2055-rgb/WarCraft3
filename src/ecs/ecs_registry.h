#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

// ============================================================
// SparseComponentPool<T> & SimEcsRegistry
//
// Unity DOTS / EnTT-inspired deterministic Sparse-Set ECS container.
// Stores component data contiguously in `dense_` for cache-friendly
// iteration while maintaining O(1) lookup, insertion, and swap-and-pop
// removal by entity handleId.
// ============================================================
class ISparseComponentPool {
public:
    virtual ~ISparseComponentPool() = default;
    virtual bool Remove(int entityId) = 0;
    virtual bool Has(int entityId) const = 0;
    virtual void Clear() = 0;
    virtual size_t Size() const = 0;
};

template <typename T>
class SparseComponentPool final : public ISparseComponentPool {
public:
    SparseComponentPool() = default;

    template <typename... Args>
    T& Emplace(int entityId, Args&&... args) {
        auto it = sparse_.find(entityId);
        if (it != sparse_.end()) {
            dense_[it->second] = T(std::forward<Args>(args)...);
            return dense_[it->second];
        }
        const size_t idx = dense_.size();
        dense_.emplace_back(std::forward<Args>(args)...);
        entities_.push_back(entityId);
        sparse_[entityId] = idx;
        return dense_.back();
    }

    T& GetOrCreate(int entityId) {
        auto it = sparse_.find(entityId);
        if (it != sparse_.end()) {
            return dense_[it->second];
        }
        return Emplace(entityId);
    }

    T* TryGet(int entityId) {
        auto it = sparse_.find(entityId);
        return (it != sparse_.end()) ? &dense_[it->second] : nullptr;
    }

    const T* TryGet(int entityId) const {
        auto it = sparse_.find(entityId);
        return (it != sparse_.end()) ? &dense_[it->second] : nullptr;
    }

    bool Has(int entityId) const override {
        return sparse_.find(entityId) != sparse_.end();
    }

    bool Remove(int entityId) override {
        auto it = sparse_.find(entityId);
        if (it == sparse_.end()) return false;
        const size_t removedIdx = it->second;
        const size_t lastIdx = dense_.size() - 1;
        if (removedIdx != lastIdx) {
            dense_[removedIdx] = std::move(dense_[lastIdx]);
            const int movedEntity = entities_[lastIdx];
            entities_[removedIdx] = movedEntity;
            sparse_[movedEntity] = removedIdx;
        }
        dense_.pop_back();
        entities_.pop_back();
        sparse_.erase(it);
        return true;
    }

    void Clear() override {
        dense_.clear();
        entities_.clear();
        sparse_.clear();
    }

    size_t Size() const override { return dense_.size(); }

    const std::vector<T>& Components() const { return dense_; }
    std::vector<T>& Components() { return dense_; }
    const std::vector<int>& Entities() const { return entities_; }

    template <typename Fn>
    void Each(Fn&& fn) {
        for (size_t i = 0; i < dense_.size(); ++i) {
            fn(entities_[i], dense_[i]);
        }
    }

    template <typename Fn>
    void Each(Fn&& fn) const {
        for (size_t i = 0; i < dense_.size(); ++i) {
            fn(entities_[i], dense_[i]);
        }
    }

private:
    std::vector<T> dense_;
    std::vector<int> entities_;
    std::unordered_map<int, size_t> sparse_;
};

class SimEcsRegistry {
public:
    SimEcsRegistry() = default;

    template <typename T>
    SparseComponentPool<T>& Pool() {
        const std::type_index key(typeid(T));
        auto it = pools_.find(key);
        if (it == pools_.end()) {
            auto pool = std::make_unique<SparseComponentPool<T>>();
            auto* raw = pool.get();
            pools_.emplace(key, std::move(pool));
            return *raw;
        }
        return *static_cast<SparseComponentPool<T>*>(it->second.get());
    }

    template <typename T>
    const SparseComponentPool<T>* TryPool() const {
        const std::type_index key(typeid(T));
        auto it = pools_.find(key);
        if (it == pools_.end()) return nullptr;
        return static_cast<const SparseComponentPool<T>*>(it->second.get());
    }

    template <typename T, typename... Args>
    T& Emplace(int entityId, Args&&... args) {
        return Pool<T>().Emplace(entityId, std::forward<Args>(args)...);
    }

    template <typename T>
    T& GetOrCreate(int entityId) {
        return Pool<T>().GetOrCreate(entityId);
    }

    template <typename T>
    T* TryGet(int entityId) {
        return Pool<T>().TryGet(entityId);
    }

    template <typename T>
    const T* TryGet(int entityId) const {
        const auto* p = TryPool<T>();
        return p ? p->TryGet(entityId) : nullptr;
    }

    template <typename T>
    bool Has(int entityId) const {
        const auto* p = TryPool<T>();
        return p ? p->Has(entityId) : false;
    }

    template <typename T>
    bool Remove(int entityId) {
        return Pool<T>().Remove(entityId);
    }

    template <typename T, typename Fn>
    void Each(Fn&& fn) {
        Pool<T>().Each(std::forward<Fn>(fn));
    }

    template <typename T, typename Fn>
    void Each(Fn&& fn) const {
        if (const auto* p = TryPool<T>()) {
            p->Each(std::forward<Fn>(fn));
        }
    }

    // Remove all components associated with an entity handleId
    void DestroyEntity(int entityId) {
        for (auto& [type, pool] : pools_) {
            (void)type;
            if (pool) pool->Remove(entityId);
        }
    }

    void Clear() {
        for (auto& [type, pool] : pools_) {
            (void)type;
            if (pool) pool->Clear();
        }
    }

private:
    std::unordered_map<std::type_index, std::unique_ptr<ISparseComponentPool>> pools_;
};
