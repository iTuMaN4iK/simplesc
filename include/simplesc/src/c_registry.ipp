﻿#pragma once

#include "simplesc/c_registry_api.h"

#include "simplesc/generic/selector.h"
#include "simplesc/impl/selector.h"

#include <cassert>
#include <iterator>
#include <optional>
#include <unordered_map>

namespace std
{
    template<>
    struct hash<eld::c_api::component_id>
    {
        size_t operator()(const eld::c_api::component_id &component) const noexcept
        {
            return hash<size_t>()(component.id);
        }
    };
}   // namespace std

namespace eld
{
    class entity_selection
    {
    public:
        explicit entity_selection(std::vector<c_api::entity> &&selection)
          : selection_(std::move(selection))
        {
        }

        entity_selection(entity_selection &&) = default;
        entity_selection &operator=(entity_selection &&) = default;

    private:
        std::vector<c_api::entity> selection_;
    };

    /**
     * Component-Entity table.
     */
    class components_table
    {
    public:
        static components_table &instance() { return components_table::instance_; }

        void register_components(const c_api::entity &owningEntity,
                                 const c_api::component_id *componentArray,
                                 size_t length,
                                 c_api::reg_error *resArray)
        {
            for (size_t i = 0; i != length; ++i)
            {
                const auto res = register_component(owningEntity, componentArray[i]);
                if (resArray)
                    resArray[i] = res;
            }
        }

        c_api::reg_error register_component(const c_api::entity &owningEntity,
                                const c_api::component_id &component)
        {
            auto optionalColumn = get_column(component);
            if (!optionalColumn)
            {
                componentsTable_.emplace(std::make_pair(component, std::vector{ owningEntity }));
                return c_api::reg_error::success;
            }

            component_column &column = *optionalColumn;
            auto iter = std::find(column.cbegin(), column.cend(), owningEntity);
            if (iter != column.cend())
                return c_api::reg_error::component_already_registered;

            column.push_back(owningEntity);
            std::sort(column.begin(), column.end());

            return c_api::reg_error::success;
        }

        void unregister_components(const c_api::entity &owningEntity,
                                   const c_api::component_id *componentArray,
                                   size_t length,
                                   c_api::unreg_error *resArray)
        {
            for (size_t i = 0; i != length; ++i)
            {
                const auto res = unregister_component(owningEntity, componentArray[i]);
                if (resArray)
                    resArray[i] = res;
            }
        }

        c_api::unreg_error unregister_component(const c_api::entity &owningEntity,
                                  const c_api::component_id &component)
        {
            auto optionalColumn = get_column(component);
            if (!optionalColumn)
                return c_api::unreg_error::component_not_found;

            component_column &column = *optionalColumn;

            auto iter = std::find(column.cbegin(), column.cend(), owningEntity);
            if (iter == column.cend())
                return c_api::unreg_error::component_not_registered;

            column.erase(iter);
            return c_api::unreg_error::success;
        }

        c_api::entity_selection select(const std::vector<c_api::component_id> &selectComponents)
        {
            auto selected = selector_(componentsTable_, selectComponents);
            const auto id = get_next_id();

            c_api::entity_selection selection{ id, selected.data(), selected.size() };

            [[maybe_unused]] auto res =
                selections_.emplace(std::make_pair(id, std::move(selected)));
            assert(res.second && "Unexpected ID conflict");

            return selection;
        }

        void free(c_api::entity_selection &selection)
        {
            selections_.erase(selection.handle);
            selection.array = nullptr;
            selection.length = 0;
            selection.handle = c_api::invalid_id;
        }

    private:
        using component_column = std::vector<c_api::entity>;

        size_t get_next_id()
        {
            if (!freedSelection_)
                return selectionsCounter_++;

            auto temp = freedSelection_;
            freedSelection_ = 0;
            return temp;
        }

        auto get_column(const c_api::component_id &component)
            -> std::optional<std::reference_wrapper<component_column>>
        {
            auto found = componentsTable_.find(component);
            if (found == componentsTable_.cend())
                return std::nullopt;

            return found->second;
        }

    private:
        static components_table instance_;

        eld::generic::selector<eld::impl::selector_std<c_api::entity, c_api::component_id>> selector_;

        std::unordered_map<c_api::component_id, component_column> componentsTable_;

        // TODO: standalone class
        std::unordered_map<size_t, entity_selection> selections_;
        size_t selectionsCounter_ = 0,   //
            freedSelection_ = 0;
    };

    components_table components_table::instance_{};

    namespace c_api
    {
        void register_components(const entity &owningEntity,
                                 const component_id *array,
                                 size_t length,
                                 reg_error *results)
        {
            if (!array ||   //
                !length)
                return;

            components_table::instance().register_components(owningEntity, array, length, results);
        }

        void unregister_components(const entity &owningEntity,
                                   const component_id *array,
                                   size_t length,
                                   unreg_error *results)
        {
            if (!array ||   //
                !length)
                return;

            components_table::instance().unregister_components(owningEntity, array, length, results);
        }

        void select_entities_by_components(const component_id *array,
                                           size_t length,
                                           entity_selection &result)
        {
            using difference_type = std::iterator_traits<decltype(array)>::difference_type;
            result = components_table::instance().select(
                std::vector<component_id>(array, std::next(array, difference_type(length))));
        }

        void free_entity_selection(entity_selection &selection)
        {
            components_table::instance().free(selection);
        }

        void allocate_entities(entity *&array, size_t length, entity_allocation_error *&results)
        {
            // TODO: implement
        }

        void deallocate_entities(entity *&array, size_t length, entity_allocation_error *&results)
        {
            // TODO: implement
        }

    }   // namespace c_api

}   // namespace eld