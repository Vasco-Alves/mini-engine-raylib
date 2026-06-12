#pragma once

#include <mini-ecs/registry.hpp>
#include <mini-ecs/entity.hpp>

#include <raylib.h>

#include <mini-engine-raylib/core/logger.hpp>
#include <mini-engine-raylib/ecs/components.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace editor {

    // =========================================================================
    // COMMAND INTERFACE
    // =========================================================================
    class ICommand {
    public:
        virtual ~ICommand() = default;
        virtual void Execute() = 0;
        virtual void Undo() = 0;
    };

    // =========================================================================
    // SPECIFIC COMMANDS
    // =========================================================================
    template <typename T>
    class ModifyComponentCommand : public ICommand {
    private:
        me::Entity m_Entity;
        T m_OldState;
        T m_NewState;

    public:
        ModifyComponentCommand(me::Entity entity, const T& oldState, const T& newState)
            : m_Entity(entity), m_OldState(oldState), m_NewState(newState) {}

        void Execute() override {
            if (m_Entity.is_valid() && m_Entity.has_component<T>()) {
                m_Entity.get_component<T>() = m_NewState;
            }
        }

        void Undo() override {
            if (m_Entity.is_valid() && m_Entity.has_component<T>()) {
                m_Entity.get_component<T>() = m_OldState;
            }
        }
    };

    // =========================================================================
    // COMMAND HISTORY MANAGER
    // =========================================================================
    class CommandHistory {
    private:
        std::vector<std::unique_ptr<ICommand>> m_UndoStack;
        std::vector<std::unique_ptr<ICommand>> m_RedoStack;

    public:
        // Called whenever the scene state changes (add, undo, or redo).
        std::function<void()> on_scene_changed;

        // Executes a new command and pushes it to the Undo stack, clearing the Redo stack
        void AddCommand(std::unique_ptr<ICommand> command) {
            command->Execute();
            m_UndoStack.push_back(std::move(command));
            m_RedoStack.clear(); // A new action invalidates all future redos
            if (on_scene_changed) on_scene_changed();
        }

        void Undo() {
            if (m_UndoStack.empty()) {
                return;
            }

            // Pop the top command from the Undo stack
            auto command = std::move(m_UndoStack.back());
            m_UndoStack.pop_back();

            // Revert the action and push to Redo stack
            command->Undo();
            m_RedoStack.push_back(std::move(command));
            if (on_scene_changed) on_scene_changed();
        }

        void Redo() {
            if (m_RedoStack.empty()) {
                return;
            }

            // Pop the top command from the Redo stack
            auto command = std::move(m_RedoStack.back());
            m_RedoStack.pop_back();

            // Execute the action again and push back to Undo stack
            command->Execute();
            m_UndoStack.push_back(std::move(command));
            if (on_scene_changed) on_scene_changed();
        }

        // Utility functions for UI button states
        bool CanUndo() const { return !m_UndoStack.empty(); }
        bool CanRedo() const { return !m_RedoStack.empty(); }

        void Clear() {
            m_UndoStack.clear();
            m_RedoStack.clear();
        }
    };

} // namespace editor