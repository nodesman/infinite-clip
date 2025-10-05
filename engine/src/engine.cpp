#include "bullet_engine/types.hpp"
#include "bullet_engine/state_utils.hpp"
#include <algorithm>
#include <cassert>

namespace bullet {

static bool has_prev_sibling(const State& s, const std::string& id) {
    const auto& sibs = siblings_cref(s, id);
    return index_in_siblings(s, id) > 0;
}

static bool has_next_sibling(const State& s, const std::string& id) {
    const auto& sibs = siblings_cref(s, id);
    return index_in_siblings(s, id) + 1 < sibs.size();
}

static std::string prev_sibling_id(const State& s, const std::string& id) {
    const auto& sibs = siblings_cref(s, id);
    auto idx = index_in_siblings(s, id);
    if (idx == 0) return std::string();
    return sibs[idx - 1];
}

static std::string next_sibling_id(const State& s, const std::string& id) {
    const auto& sibs = siblings_cref(s, id);
    auto idx = index_in_siblings(s, id);
    if (idx + 1 >= sibs.size()) return std::string();
    return sibs[idx + 1];
}

static State clone(const State& s) { return s; }

static void ensure_min_one_root(State& s) {
    if (s.rootOrder.empty()) {
        // create a new empty root
        Node root{ make_new_id(s), "", "", {} };
        s.nodes[root.id] = root;
        s.rootOrder.push_back(root.id);
        s.focusedId = root.id;
        s.caret = 0;
    }
}

static void set_focus(State& s, const std::string& id, int caret) {
    s.focusedId = id;
    s.caret = caret < 0 ? 0 : caret;
}

static void insert_empty_sibling_after(State& s, const std::string& id) {
    Node newNode{ make_new_id(s), s.nodes[id].parentId, "", {} };
    s.nodes[newNode.id] = newNode;
    auto& sibs = siblings_ref(s, id);
    insert_after(sibs, id, newNode.id);
    set_focus(s, newNode.id, 0);
}

static void split_at_caret(State& s, const std::string& id, int caret) {
    auto& node = s.nodes[id];
    if (caret < 0) caret = s.caret;
    if (caret < 0) caret = 0;
    if (caret > static_cast<int>(node.text.size())) caret = static_cast<int>(node.text.size());
    Node newNode{ make_new_id(s), node.parentId, node.text.substr(static_cast<size_t>(caret)), {} };
    // second node receives all children; reparent their parentId to new node
    newNode.children = std::move(node.children);
    for (const auto& cid : newNode.children) {
        s.nodes[cid].parentId = newNode.id;
    }
    node.children.clear();
    node.text.erase(static_cast<size_t>(caret));
    s.nodes[newNode.id] = newNode;
    auto& sibs = siblings_ref(s, id);
    insert_after(sibs, id, newNode.id);
    set_focus(s, newNode.id, 0);
}

static void indent(State& s, const std::string& id) {
    if (!has_prev_sibling(s, id)) return; // no previous sibling → no-op
    std::string prevId = prev_sibling_id(s, id);
    // Remove from current siblings
    auto& curSibs = siblings_ref(s, id);
    erase_from(curSibs, id);
    // Reparent under prev sibling
    s.nodes[id].parentId = prevId;
    s.nodes[prevId].children.push_back(id);
}

static void outdent(State& s, const std::string& id) {
    auto& node = s.nodes[id];
    if (node.parentId.empty()) return; // already root
    std::string parentId = node.parentId;
    std::string grandParentId = s.nodes[parentId].parentId; // may be empty
    // remove from parent's children
    auto& pchildren = s.nodes[parentId].children;
    erase_from(pchildren, id);
    // insert as next sibling after parent in grandparent's list (or root)
    if (grandParentId.empty()) {
        insert_after(s.rootOrder, parentId, id);
        node.parentId.clear();
    } else {
        auto& gpsibs = s.nodes[grandParentId].children;
        insert_after(gpsibs, parentId, id);
        node.parentId = grandParentId;
    }
}

static void move_up(State& s, const std::string& id) {
    auto& node = s.nodes[id];
    auto& sibs = siblings_ref(s, id);
    auto idx = index_in_siblings(s, id);
    if (idx > 0) {
        std::swap(sibs[idx - 1], sibs[idx]);
        return;
    }
    // At first position, hoist if possible
    if (node.parentId.empty()) return; // root and first → no-op
    std::string parentId = node.parentId;
    std::string grandParentId = s.nodes[parentId].parentId;
    // remove from parent's children
    erase_from(s.nodes[parentId].children, id);
    if (grandParentId.empty()) {
        // insert before parent in rootOrder
        insert_before(s.rootOrder, parentId, id);
        node.parentId.clear();
    } else {
        auto& gpsibs = s.nodes[grandParentId].children;
        insert_before(gpsibs, parentId, id);
        node.parentId = grandParentId;
    }
}

static void move_down(State& s, const std::string& id) {
    auto& node = s.nodes[id];
    auto& sibs = siblings_ref(s, id);
    auto idx = index_in_siblings(s, id);
    if (idx + 1 < sibs.size()) {
        std::swap(sibs[idx], sibs[idx + 1]);
        return;
    }
    // At last position, sink if possible
    if (node.parentId.empty()) return; // root and last → no-op
    std::string parentId = node.parentId;
    std::string grandParentId = s.nodes[parentId].parentId;
    // remove from parent's children
    erase_from(s.nodes[parentId].children, id);
    if (grandParentId.empty()) {
        // insert after parent in rootOrder
        insert_after(s.rootOrder, parentId, id);
        node.parentId.clear();
    } else {
        auto& gpsibs = s.nodes[grandParentId].children;
        insert_after(gpsibs, parentId, id);
        node.parentId = grandParentId;
    }
}

static void delete_empty_at_id(State& s, const std::string& id) {
    auto it = s.nodes.find(id);
    if (it == s.nodes.end()) return;
    if (!it->second.text.empty()) return; // only delete when text is empty
    if (!it->second.children.empty()) return; // has children → no-op
    // compute preferred new focus before mutation
    std::string prev = prev_visible_id(s, id);
    std::string next = next_visible_id(s, id);
    // if last remaining root and it's root → clear text instead
    bool isRoot = it->second.parentId.empty();
    if (isRoot && s.rootOrder.size() == 1) {
        it->second.text.clear();
        s.focusedId = it->second.id;
        s.caret = 0;
        return;
    }
    // remove from siblings container
    auto& sibs = siblings_ref(s, id);
    erase_from(sibs, id);
    s.nodes.erase(it);
    // clear scope if it pointed to deleted id
    if (s.scopeRootId.has_value() && s.scopeRootId == id) {
        s.scopeRootId = std::nullopt;
    }
    // ensure at least one root remains
    ensure_min_one_root(s);
    // set new focus: prefer previous visible, else next, else first root
    std::string newFocus = !prev.empty() ? prev : (!next.empty() ? next : (s.rootOrder.empty() ? std::string() : s.rootOrder.front()));
    if (!newFocus.empty()) {
        int caret = static_cast<int>(s.nodes[newFocus].text.size());
        set_focus(s, newFocus, caret);
    }
}

static void merge_next_sibling_into_current(State& s, const std::string& id) {
    auto& node = s.nodes[id];
    if (!node.children.empty()) return; // precondition: current has no children
    std::string nextId = next_sibling_id(s, id);
    if (nextId.empty()) return; // no next sibling
    auto& nextNode = s.nodes[nextId];
    // Append text and children
    node.text += nextNode.text;
    node.children.insert(node.children.end(), nextNode.children.begin(), nextNode.children.end());
    for (const auto& cid : nextNode.children) {
        s.nodes[cid].parentId = id;
    }
    // remove next from siblings and nodes
    auto& sibs = siblings_ref(s, id);
    erase_from(sibs, nextId);
    s.nodes.erase(nextId);
    if (s.scopeRootId.has_value() && s.scopeRootId == nextId) {
        s.scopeRootId = std::nullopt;
    }
    // focus remains on current; caret moves to end
    set_focus(s, id, static_cast<int>(node.text.size()));
}

State apply_command(const State& s0, const Command& cmd) {
    State s = clone(s0);
    std::string target = cmd.id.empty() ? s.focusedId : cmd.id;
    if (s.nodes.find(target) == s.nodes.end()) return s; // invalid id → no-op

    switch (cmd.type) {
        case CommandType::InsertEmptySiblingAfter:
            insert_empty_sibling_after(s, target);
            break;
        case CommandType::SplitAtCaret:
            split_at_caret(s, target, cmd.caret);
            break;
        case CommandType::Indent:
            indent(s, target);
            break;
        case CommandType::Outdent:
            outdent(s, target);
            break;
        case CommandType::MoveUp:
            move_up(s, target);
            break;
        case CommandType::MoveDown:
            move_down(s, target);
            break;
        case CommandType::DeleteEmptyAtId:
            delete_empty_at_id(s, target);
            break;
        case CommandType::MergeNextSiblingIntoCurrent:
            merge_next_sibling_into_current(s, target);
            break;
        case CommandType::SetFocus:
            set_focus(s, target, cmd.caret);
            break;
        case CommandType::SetScopeRoot:
            s.scopeRootId = cmd.scopeRootId;
            break;
    }
    return s;
}

} // namespace bullet
