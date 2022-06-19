#pragma once

#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include "mt/thread_pool/thread_pool.h"
#include "ecs/archetype.h"

namespace mkr {
    class job_handle {
    private:
        std::atomic_bool complete_ = false;

    public:
        inline bool is_complete() const { return complete_.load(); }
        inline void complete() { complete_.store(true); }
    };

    class world {
    private:
        std::unordered_map<component_bitset, std::shared_ptr<archetype>> archetypes_;
        std::unordered_map<uint64_t, component_bitset> entity_bitsets_;
        std::unordered_map<uint64_t, uint64_t> entity_indices_;
        std::unordered_set<uint64_t> entities_;
        std::atomic<uint64_t> curr_entity_id_ = 1;
        thread_pool thread_pool_;

        std::shared_ptr<archetype> get_archetype(const component_bitset& _bitset) {
            if (nullptr == archetypes_[_bitset]) {
                archetypes_[_bitset] = std::make_shared<archetype>(_bitset);
            }
            return archetypes_[_bitset];
        }

    public:
        world() = default;
        ~world() = default;

        uint64_t create_entity() {
            entities_.insert(curr_entity_id_);
            return curr_entity_id_++;
        }

        void destroy_entity(uint64_t _entity) {
            if (!has_entity(_entity)) {
                throw std::invalid_argument("entity not in world");
            }

            std::shared_ptr<archetype> arc = get_archetype(entity_bitsets_[_entity]);
            archetype::remove_components(arc, entity_indices_[_entity]);
            entities_.erase(_entity);
        }

        bool has_entity(uint64_t _entity) const {
            return entities_.contains(_entity);
        }

        template<class Callable>
        job_handle schedule(Callable _func, job_handle* _dependency = nullptr) {
            return {};
        }

        template<class C, class ...Args>
        void add_component(uint64_t _entity, Args... _args) {
            archetype::register_component_type<C>();

            const uint64_t family_id = FAMILY_ID(component, C);
            component_bitset prev_bitset = entity_bitsets_[_entity];

            // Check if the entity already has this component.
            if (prev_bitset[family_id]) {
                throw std::invalid_argument("entity already has this component");
            }

            component_bitset next_bitset = entity_bitsets_[_entity];
            next_bitset.set(family_id);

            std::shared_ptr<archetype> prev_arc = get_archetype(prev_bitset);
            std::shared_ptr<archetype> next_arc = get_archetype(next_bitset);
            uint64_t prev_index = entity_indices_[_entity];

            // Move components from previous archetype to next archetype.
            archetype::copy_components(prev_arc, next_arc, prev_index, prev_bitset);
            // Remove components from old archetype.
            archetype::remove_components(prev_arc, prev_index);
            // Add new component to next archetype.
            uint64_t next_index = archetype::add_component<C>(next_arc, C{std::forward<Args...>(_args)...});

            // Update entity info.
            entity_indices_[_entity] = next_index;
            entity_bitsets_[_entity] = next_bitset;
        }

        template<class C>
        void remove_component(uint64_t _entity) {
            const uint64_t family_id = FAMILY_ID(component, C);
            component_bitset prev_bitset = entity_bitsets_[_entity];

            // Check if the entity does not have this component.
            if (!prev_bitset[family_id]) {
                throw std::invalid_argument("entity does not have this component");
            }

            component_bitset next_bitset = entity_bitsets_[_entity];
            next_bitset.reset(family_id);

            std::shared_ptr<archetype> prev_arc = get_archetype(prev_bitset);
            std::shared_ptr<archetype> next_arc = get_archetype(next_bitset);
            uint64_t prev_index = entity_indices_[_entity];

            // Move components from previous archetype to next archetype.
            archetype::copy_components(prev_arc, next_arc, prev_index, next_bitset);
            // Remove components from old archetype.
            archetype::remove_components(prev_arc, prev_index);
        }

        template<class Function, class... Args>
        void schedule_job() {
        }
    };
}