#pragma once

// Undo/redo engine: the native equivalent of the Java IUndo / UndoMultiEntry
// pair. Commands are UI-agnostic -- each knows how to undo and redo one change
// and the stack owns ordering, so the same engine drives the desktop editor,
// the CLI and the tests.
//
// Contract: a command is pushed to the stack *after* the change it describes
// has already been applied, so push() never re-applies it. redo() is therefore
// the only place that re-applies, and it is called solely when moving the
// cursor forward.

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace whitehole::edit {

class IUndo {
public:
    IUndo() = default;
    virtual ~IUndo() = default;

    IUndo(const IUndo&) = delete;
    IUndo& operator=(const IUndo&) = delete;

    virtual void undo() = 0;
    virtual void redo() = 0;
    // Short human-readable action name, e.g. "Move 3 objects".
    [[nodiscard]] virtual std::string label() const = 0;
};

using UndoEntry = std::unique_ptr<IUndo>;

// Linear undo stack with a cursor. Entries stay alive after an undo so they can
// be redone; pushing a new entry after an undo discards the redo branch, which
// is what every editor does.
class UndoStack {
public:
    UndoStack() = default;

    // Records an already-applied command. Never re-applies it.
    void push(UndoEntry entry);

    // Undoes the most recent command. False when there is nothing to undo.
    bool undo();
    // Redoes the next command. False when there is nothing to redo.
    bool redo();

    [[nodiscard]] bool canUndo() const noexcept { return cursor_ > 0; }
    [[nodiscard]] bool canRedo() const noexcept { return cursor_ < entries_.size(); }
    // Label of the command undo() / redo() would run, or empty.
    [[nodiscard]] std::string undoLabel() const;
    [[nodiscard]] std::string redoLabel() const;

    void clear() noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] std::size_t cursor() const noexcept { return cursor_; }
    // Number of commands that can still be redone.
    [[nodiscard]] std::size_t redoCount() const noexcept { return entries_.size() - cursor_; }

private:
    std::vector<UndoEntry> entries_;
    std::size_t cursor_{0}; // entries_[0, cursor_) are applied
};

// A group of commands undone and redone as one user-visible action, matching
// Java's UndoMultiEntry. Children are applied in order on redo and in reverse
// order on undo, so nested state changes unwind correctly.
class UndoMultiEntry final : public IUndo {
public:
    explicit UndoMultiEntry(std::string label = "Edit");
    ~UndoMultiEntry() override = default;

    void add(UndoEntry entry);
    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

    void undo() override;
    void redo() override;
    [[nodiscard]] std::string label() const override;

private:
    std::string label_;
    std::vector<UndoEntry> entries_;
};

} // namespace whitehole::edit