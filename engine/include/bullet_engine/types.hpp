#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace bullet {

struct Node {
    std::string id;
    std::string parentId; // empty string denotes root
    std::string text;
    std::vector<std::string> children; // ordered
};

struct State {
    std::unordered_map<std::string, Node> nodes;
    std::vector<std::string> rootOrder; // ordered root ids
    std::string focusedId;
    int caret = 0; // caret offset within focused node text
    std::optional<std::string> scopeRootId; // nullopt means full tree
    unsigned long long idCounter = 0; // for deterministic id gen: n1, n2, ...
};

// Commands
enum class CommandType {
    InsertEmptySiblingAfter,
    SplitAtCaret,
    Indent,
    Outdent,
    MoveUp,
    MoveDown,
    DeleteEmptyAtId,
    MergeNextSiblingIntoCurrent,
    SetFocus,
    SetScopeRoot
};

struct Command {
    CommandType type;
    // target node id (defaults to state.focusedId if empty)
    std::string id;
    // Additional fields used by specific commands
    int caret = -1; // used by SplitAtCaret/SetFocus
    std::optional<std::string> scopeRootId; // used by SetScopeRoot
};

// Engine API
State apply_command(const State& s, const Command& cmd);

// Utilities useful to UIs
// Return the previous/next visible node id in preorder under current scope, or empty if none.
std::string prev_visible_id(const State& s, const std::string& id);
std::string next_visible_id(const State& s, const std::string& id);

// Helper: create an initial state with a single empty root node focused.
State initial_state();

} // namespace bullet

