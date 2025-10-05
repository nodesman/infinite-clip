#include "bullet_engine/types.hpp"
#include "bullet_engine/state_utils.hpp"
#include <cassert>
#include <algorithm>

namespace bullet {

std::vector<std::string>& siblings_ref(State& s, const std::string& id) {
    auto it = s.nodes.find(id);
    assert(it != s.nodes.end());
    const std::string& parentId = it->second.parentId;
    if (parentId.empty()) {
        return s.rootOrder;
    }
    return s.nodes[parentId].children;
}

const std::vector<std::string>& siblings_cref(const State& s, const std::string& id) {
    auto it = s.nodes.find(id);
    assert(it != s.nodes.end());
    const std::string& parentId = it->second.parentId;
    if (parentId.empty()) {
        return s.rootOrder;
    }
    return s.nodes.at(parentId).children;
}

size_t index_in_siblings(const State& s, const std::string& id) {
    const auto& sibs = siblings_cref(s, id);
    auto it = std::find(sibs.begin(), sibs.end(), id);
    assert(it != sibs.end());
    return static_cast<size_t>(std::distance(sibs.begin(), it));
}

std::string make_new_id(State& s) {
    ++s.idCounter;
    return std::string("n") + std::to_string(s.idCounter);
}

void insert_after(std::vector<std::string>& vec, const std::string& existing, const std::string& newcomer) {
    auto it = std::find(vec.begin(), vec.end(), existing);
    assert(it != vec.end());
    vec.insert(it + 1, newcomer);
}

void insert_before(std::vector<std::string>& vec, const std::string& existing, const std::string& newcomer) {
    auto it = std::find(vec.begin(), vec.end(), existing);
    assert(it != vec.end());
    vec.insert(it, newcomer);
}

void erase_from(std::vector<std::string>& vec, const std::string& id) {
    auto it = std::find(vec.begin(), vec.end(), id);
    assert(it != vec.end());
    vec.erase(it);
}

static void preorder_collect(const State& s, const std::string& root, std::vector<std::string>& out) {
    out.push_back(root);
    const auto& node = s.nodes.at(root);
    for (const auto& cid : node.children) {
        preorder_collect(s, cid, out);
    }
}

std::vector<std::string> visible_order_ids(const State& s) {
    std::vector<std::string> out;
    if (s.scopeRootId.has_value() && !s.scopeRootId->empty()) {
        if (s.nodes.find(*s.scopeRootId) != s.nodes.end()) {
            preorder_collect(s, *s.scopeRootId, out);
        }
        return out;
    }
    for (const auto& rid : s.rootOrder) {
        preorder_collect(s, rid, out);
    }
    return out;
}

std::string prev_visible_id(const State& s, const std::string& id) {
    auto order = visible_order_ids(s);
    auto it = std::find(order.begin(), order.end(), id);
    if (it == order.end() || it == order.begin()) return std::string();
    return *(it - 1);
}

std::string next_visible_id(const State& s, const std::string& id) {
    auto order = visible_order_ids(s);
    auto it = std::find(order.begin(), order.end(), id);
    if (it == order.end() || it + 1 == order.end()) return std::string();
    return *(it + 1);
}

State initial_state() {
    State s;
    s.idCounter = 0;
    Node root{ "n1", "", "", {} };
    s.nodes[root.id] = root;
    s.rootOrder.push_back(root.id);
    s.focusedId = root.id;
    s.caret = 0;
    s.idCounter = 1;
    return s;
}

std::vector<std::string> ancestors_to_root(const State& s, const std::string& id) {
    std::vector<std::string> rev;
    auto it = s.nodes.find(id);
    if (it == s.nodes.end()) return {};
    std::string cur = id;
    while (!cur.empty()) {
        rev.push_back(cur);
        const auto& node = s.nodes.at(cur);
        if (node.parentId.empty()) break;
        cur = node.parentId;
    }
    // reverse to be root..id
    std::reverse(rev.begin(), rev.end());
    return rev;
}

} // namespace bullet
