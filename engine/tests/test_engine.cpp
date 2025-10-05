#include "bullet_engine/types.hpp"
#include "bullet_engine/state_utils.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <random>
#include <chrono>

using namespace bullet;

static void assert_eq(const std::string& a, const std::string& b, const char* msg) {
    if (a != b) {
        std::cerr << "Assertion failed: " << msg << " ('" << a << "' != '" << b << "')\n";
        std::abort();
    }
}
static void assert_eq_size(size_t a, size_t b, const char* msg) {
    if (a != b) {
        std::cerr << "Assertion failed: " << msg << " (" << a << " != " << b << ")\n";
        std::abort();
    }
}
static void assert_true(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << "Assertion failed: " << msg << "\n";
        std::abort();
    }
}

static void reset(State& s) { s = initial_state(); }

// Invariant checks: no orphans, correct parent/children linkage, roots have empty parentId, no duplicates
static void verify_invariants(const State& s) {
    // at least one root
    assert_true(!s.rootOrder.empty(), "at least one root");
    // roots exist and have empty parent
    std::unordered_set<std::string> seen;
    std::unordered_set<std::string> roots_set;
    roots_set.insert(s.rootOrder.begin(), s.rootOrder.end());
    assert_true(roots_set.size() == s.rootOrder.size(), "no duplicate roots");
    for (const auto& rid : s.rootOrder) {
        auto it = s.nodes.find(rid);
        assert_true(it != s.nodes.end(), "root id exists");
        assert_true(it->second.parentId.empty(), "root parentId empty");
    }
    // traverse and check children linkage
    std::function<void(const std::string&)> dfs = [&](const std::string& id){
        assert_true(seen.insert(id).second, "no duplicate visit");
        const auto& node = s.nodes.at(id);
        std::unordered_set<std::string> childset;
        for (const auto& cid : node.children) {
            assert_true(childset.insert(cid).second, "no duplicate children");
            auto itc = s.nodes.find(cid);
            assert_true(itc != s.nodes.end(), "child exists");
            assert_eq(itc->second.parentId, node.id, "child parent link");
            dfs(cid);
        }
    };
    // also track containment counts
    std::unordered_map<std::string, int> contain_count;
    for (const auto& rid : s.rootOrder) {
        dfs(rid);
        contain_count[rid] += 1;
    }
    for (const auto& kv : s.nodes) {
        const auto& node = kv.second;
        for (const auto& cid : node.children) {
            contain_count[cid] += 1;
        }
    }
    // ensure all nodes are reachable from roots
    assert_true(seen.size() == s.nodes.size(), "no orphans reachable from roots");
    // ensure every node appears in exactly one container (rootOrder or exactly one parent's children)
    for (const auto& kv : s.nodes) {
        auto it = contain_count.find(kv.first);
        int c = (it == contain_count.end()) ? 0 : it->second;
        if (kv.second.parentId.empty()) {
            assert_true(c == 1, "root appears exactly once in rootOrder");
        } else {
            assert_true(c == 1, "child appears exactly once under a parent");
        }
    }
    // focusedId exists and caret bounds
    assert_true(s.nodes.find(s.focusedId) != s.nodes.end(), "focusedId exists");
    const auto& fnode = s.nodes.at(s.focusedId);
    assert_true(s.caret >= 0 && s.caret <= (int)fnode.text.size(), "caret within bounds");
    // scopeRootId validity if set
    if (s.scopeRootId.has_value() && !s.scopeRootId->empty()) {
        assert_true(s.nodes.find(*s.scopeRootId) != s.nodes.end(), "scopeRootId exists");
    }
}

static State apply_and_check(State s, const Command& cmd, std::optional<int> expectDelta = std::nullopt) {
    size_t before = s.nodes.size();
    s = apply_command(s, cmd);
    verify_invariants(s);
    if (expectDelta.has_value()) {
        long delta = static_cast<long>(s.nodes.size()) - static_cast<long>(before);
        assert_true(delta == *expectDelta, "node count delta matches expectation");
    }
    return s;
}

int main() {
    // 1) Initial state
    State s = initial_state();
    assert_eq(s.focusedId, "n1", "initial focused id");
    assert_true(s.nodes.size() == 1 && s.rootOrder.size() == 1, "one root node");

    // 2) Enter split mid-text: second gets children; focus/caret
    s.nodes[s.focusedId].text = "Hello";
    s.caret = 5;
    s = apply_and_check(s, Command{ CommandType::SplitAtCaret, s.focusedId, 2 }, +1);
    assert_eq(s.nodes["n1"].text, "He", "n1 text after split");
    assert_true(s.nodes.find("n2") != s.nodes.end(), "n2 exists");
    assert_eq(s.nodes["n2"].text, "llo", "n2 text after split");
    assert_eq(s.focusedId, "n2", "focus moved to second after split");
    assert_true(s.caret == 0, "caret at start after split");

    // Split when original has children: second should receive them
    s = apply_and_check(s, Command{ CommandType::Indent, "n2" });
    s.nodes["n1"].text = "AB";
    s = apply_and_check(s, Command{ CommandType::SplitAtCaret, "n1", 1 }, +1);
    // Now n3 is second half, and gets children, n1 children empty
    assert_true(s.nodes["n1"].children.empty(), "first segment lost children");
    assert_true(s.nodes["n3"].children.size() == 1 && s.nodes["n3"].children[0] == "n2", "second segment got children");
    assert_eq(s.nodes["n1"].text, "A", "n1 text after split 2");
    assert_eq(s.nodes["n3"].text, "B", "n3 text after split 2");

    // 3) End-of-text create empty sibling at same level
    s = apply_and_check(s, Command{ CommandType::InsertEmptySiblingAfter, "n3" }, +1);
    std::string n4 = s.focusedId;
    assert_true(s.nodes[n4].text.empty(), "new sibling empty");

    // 4) Empty indented Enter (simulate outdent via command) until root; then create empty root sibling
    // Structure: create two roots then indent second under first, then outdent twice
    reset(s);
    s.nodes[s.focusedId].text = "P"; // n1
    s = apply_and_check(s, Command{ CommandType::InsertEmptySiblingAfter, "n1" }, +1); // n2
    s.nodes[s.focusedId].text = "C";
    s = apply_command(s, Command{ CommandType::Indent, "n2" }); // n2 under n1
    // Simulate Enter on empty indented: outdent — set n2 empty and outdent repeatedly
    s.nodes["n2"].text.clear();
    s = apply_and_check(s, Command{ CommandType::Outdent, "n2" }); // becomes sibling of n1
    assert_true(s.nodes["n2"].parentId.empty(), "n2 outdented to root");
    // At root, Enter creates empty root sibling → simulate by insert after
    s = apply_and_check(s, Command{ CommandType::InsertEmptySiblingAfter, "n2" }, +1);
    assert_true(s.rootOrder.size() == 3, "root sibling added after outdent");

    // 5) Tab/Shift+Tab
    reset(s);
    // Two roots
    s.nodes["n1"].text = "A";
    s = apply_and_check(s, Command{ CommandType::InsertEmptySiblingAfter, "n1" }, +1); // n2
    s.nodes["n2"].text = "B";
    // Tab on first root (no previous sibling) → no-op
    s = apply_command(s, Command{ CommandType::Indent, "n1" });
    assert_true(s.nodes["n1"].parentId.empty() && s.rootOrder.size() == 2, "Tab on first root no-op");
    // Tab on second root → becomes child of first
    s = apply_and_check(s, Command{ CommandType::Indent, "n2" });
    assert_true(s.nodes["n1"].children.size() == 1 && s.nodes["n1"].children[0] == "n2", "Tab indents under prev sibling");
    // Shift+Tab on n2 → outdent to become next sibling of n1
    s = apply_and_check(s, Command{ CommandType::Outdent, "n2" });
    assert_true(s.nodes["n2"].parentId.empty(), "outdent to root");
    assert_true(s.rootOrder.size() == 2 && s.rootOrder[1] == "n2", "n2 after n1 at root");
    // Shift+Tab on root → no-op
    auto s_before = s;
    s = apply_and_check(s, Command{ CommandType::Outdent, "n2" });
    assert_true(s.rootOrder == s_before.rootOrder, "outdent at root no-op");

    // 6) Reorder within siblings; hoist/sink at bounds; subtree preserved
    reset(s);
    // roots: n1, n2, n3
    s.nodes["n1"].text = "R1";
    s = apply_and_check(s, Command{ CommandType::InsertEmptySiblingAfter, "n1" }, +1); // n2
    s.nodes["n2"].text = "R2";
    s = apply_and_check(s, Command{ CommandType::InsertEmptySiblingAfter, "n2" }, +1); // n3
    s.nodes["n3"].text = "R3";
    // moveDown n1 -> n1, n2 swap -> n2, n1, n3
    s = apply_and_check(s, Command{ CommandType::MoveDown, "n1" });
    assert_true(s.rootOrder[0] == "n2" && s.rootOrder[1] == "n1", "moveDown swap within siblings");
    // moveUp n1 -> swap with n2 back to n1, n2, n3
    s = apply_and_check(s, Command{ CommandType::MoveUp, "n1" });
    assert_true(s.rootOrder[0] == "n1", "moveUp swap back");
    // Bounds: moveUp at first root is no-op
    auto ro_before = s.rootOrder;
    s = apply_and_check(s, Command{ CommandType::MoveUp, "n1" });
    assert_true(s.rootOrder == ro_before, "moveUp at first root no-op");
    // Create child under n2 and test hoist/sink across levels
    s = apply_and_check(s, Command{ CommandType::Indent, "n3" }); // n3 under n2
    // moveUp n3 at first child position -> hoist before parent (n2)
    s = apply_and_check(s, Command{ CommandType::MoveUp, "n3" });
    assert_true(s.nodes["n3"].parentId.empty(), "n3 hoisted to root");
    assert_true(s.rootOrder[1] == "n3" && s.rootOrder[2] == "n2", "n3 before former parent");
    // Sink: moveDown n3 at last root position -> after parent (n2)
    // Ensure n3 is just before n2, then sink
    s = apply_and_check(s, Command{ CommandType::MoveDown, "n3" });
    assert_true(s.rootOrder[1] == "n2" && s.rootOrder[2] == "n3", "n3 sunk after parent");

    // 7) Backspace/Delete behaviors via commands
    reset(s);
    // Create two roots, second has children
    s.nodes["n1"].text = "A";
    s = apply_command(s, Command{ CommandType::InsertEmptySiblingAfter, "n1" }); // n2
    s.nodes["n2"].text = "B";
    // Delete-empty on non-empty should be no-op (engine only deletes empty w/o children)
    auto count_before = s.nodes.size();
    s = apply_and_check(s, Command{ CommandType::DeleteEmptyAtId, "n2" });
    assert_true(s.nodes.size() == count_before, "deleteEmpty no-op on non-empty");
    // Make n2 empty and childless then delete
    s.nodes["n2"].text.clear();
    s = apply_and_check(s, Command{ CommandType::DeleteEmptyAtId, "n2" }, -1);
    assert_true(s.nodes.find("n2") == s.nodes.end(), "empty childless deleted");
    // Guard last root: deleting last root clears text instead
    reset(s);
    s.nodes["n1"].text = "X";
    s.nodes["n1"].text.clear();
    s = apply_and_check(s, Command{ CommandType::DeleteEmptyAtId, "n1" });
    assert_true(s.nodes.find("n1") != s.nodes.end(), "last root not deleted");
    assert_true(s.nodes["n1"].text.empty(), "last root text cleared");

    // Delete key at end merge: only when next is sibling and current has no children
    reset(s);
    s.nodes["n1"].text = "A";
    s = apply_and_check(s, Command{ CommandType::InsertEmptySiblingAfter, "n1" }, +1); // n2
    s.nodes["n2"].text = "B";
    // current has no children → merge next sibling
    s = apply_and_check(s, Command{ CommandType::MergeNextSiblingIntoCurrent, "n1" }, -1);
    assert_true(s.nodes.find("n2") == s.nodes.end(), "merged sibling removed");
    assert_eq(s.nodes["n1"].text, "AB", "merged text AB");
    assert_true(s.caret == (int)s.nodes["n1"].text.size(), "caret at end after merge");
    // If current has children → no merge
    s = apply_and_check(s, Command{ CommandType::InsertEmptySiblingAfter, "n1" }, +1); // n3
    s.nodes["n3"].text = "C";
    s = apply_and_check(s, Command{ CommandType::Indent, "n3" });
    auto nodes_before = s.nodes.size();
    s = apply_and_check(s, Command{ CommandType::MergeNextSiblingIntoCurrent, "n1" });
    assert_true(s.nodes.size() == nodes_before, "no merge when current has children");

    // 8) Navigation helpers prev/next visible
    reset(s);
    // n1, n2, with n2 having child n3
    s.nodes["n1"].text = "A";
    s = apply_and_check(s, Command{ CommandType::InsertEmptySiblingAfter, "n1" }, +1); // n2
    s.nodes["n2"].text = "B";
    s = apply_and_check(s, Command{ CommandType::InsertEmptySiblingAfter, "n2" }, +1); // n3
    std::string n3_id = s.focusedId;
    // indent n3 under n2
    s = apply_and_check(s, Command{ CommandType::Indent, n3_id });
    // Visible order: n1, n2, n3
    assert_eq(prev_visible_id(s, "n2"), "n1", "prev visible of n2 is n1");
    assert_eq(next_visible_id(s, "n2"), n3_id, "next visible of n2 is n3");

    // 9) Paste composition: simulate multi-line by repeated insert-after
    reset(s);
    s.nodes["n1"].text = "Line 1";
    std::vector<std::string> lines = {"Line 2", "Line 3", "Line 4"};
    std::string after = "n1";
    for (const auto& line : lines) {
    s = apply_and_check(s, Command{ CommandType::InsertEmptySiblingAfter, after }, +1);
        std::string nid = s.focusedId;
        s.nodes[nid].text = line;
        after = nid;
    }
    assert_eq_size(s.rootOrder.size(), 4, "paste produced 3 new roots");
    assert_eq(s.nodes[s.rootOrder[1]].text, "Line 2", "paste line 2");
    assert_eq(s.nodes[s.rootOrder[3]].text, "Line 4", "paste line 4");

    // 10) Scope/drill-down: visible_order_ids and ancestors_to_root
    reset(s);
    // Build tree: n1(root), n2(sibling), n3(child of n2), n4(child of n3)
    s.nodes["n1"].text = "Root1";
    s = apply_and_check(s, Command{ CommandType::InsertEmptySiblingAfter, "n1" }, +1); // n2
    s.nodes["n2"].text = "Root2";
    s = apply_and_check(s, Command{ CommandType::InsertEmptySiblingAfter, "n2" }, +1); // n3
    std::string n3id = s.focusedId;
    s.nodes[n3id].text = "ChildOfRoot2";
    s = apply_and_check(s, Command{ CommandType::Indent, n3id }); // n3 under n2
    s = apply_and_check(s, Command{ CommandType::InsertEmptySiblingAfter, n3id }, +1); // n4 as sibling of n3 under n2
    std::string n4id = s.focusedId;
    s.nodes[n4id].text = "GrandChild";
    s = apply_and_check(s, Command{ CommandType::Indent, n4id }); // n4 under n3
    // Without scope: order should be [n1, n2, n3, n4]
    auto vis = visible_order_ids(s);
    assert_eq_size(vis.size(), 4, "visible order full size");
    assert_eq(vis[0], "n1", "vis[0]=n1");
    assert_eq(vis[1], "n2", "vis[1]=n2");
    assert_eq(vis[2], n3id, "vis[2]=n3");
    assert_eq(vis[3], n4id, "vis[3]=n4");
    // With scope at n2: order should be [n2, n3, n4]
    s = apply_and_check(s, Command{ CommandType::SetScopeRoot, "", -1, std::optional<std::string>("n2") });
    auto vis2 = visible_order_ids(s);
    assert_eq_size(vis2.size(), 3, "visible order scoped size");
    assert_eq(vis2[0], "n2", "scope[0]=n2");
    assert_eq(vis2[1], n3id, "scope[1]=n3");
    assert_eq(vis2[2], n4id, "scope[2]=n4");
    // Breadcrumb for n4: [n2, n3, n4] (root..id order, but since n2 is root-level, chain starts with n2)
    auto bc = ancestors_to_root(s, n4id);
    assert_eq_size(bc.size(), 3, "breadcrumb length");
    assert_eq(bc[0], "n2", "bc[0]=n2");
    assert_eq(bc[1], n3id, "bc[1]=n3");
    assert_eq(bc[2], n4id, "bc[2]=n4");

    // 11) Property-ish fuzz: random operations maintain invariants
    {
        reset(s);
        // seed random
        std::mt19937 rng(static_cast<unsigned>(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
        auto random_id = [&](const State& st) {
            std::vector<std::string> ids; ids.reserve(st.nodes.size());
            for (auto& kv : st.nodes) ids.push_back(kv.first);
            std::uniform_int_distribution<size_t> dist(0, ids.size()-1);
            return ids[dist(rng)];
        };
        auto maybe_text = [&](State& st, const std::string& id){
            // randomly set short text or clear
            std::uniform_int_distribution<int> coin(0, 3);
            int c = coin(rng);
            if (c == 0) st.nodes[id].text.clear();
            else if (c == 1) st.nodes[id].text = "x";
            else if (c == 2) st.nodes[id].text = "xy";
            else st.nodes[id].text = ""; // treat as empty
        };
        auto check_nonfatal = [&](const State& st, const char* label){
            // copy of verify_invariants but non-fatal and with debug
            bool ok = true;
            if (st.rootOrder.empty()) { std::cerr << "[fuzz] fail: no roots after " << label << "\n"; return false; }
            for (const auto& rid : st.rootOrder) {
                auto it = st.nodes.find(rid);
                if (it == st.nodes.end() || !it->second.parentId.empty()) { std::cerr << "[fuzz] invalid root id="<<rid<<" after "<<label<<"\n"; return false; }
            }
            std::unordered_set<std::string> seen;
            std::function<void(const std::string&)> dfs = [&](const std::string& id){
                if (!seen.insert(id).second) return; // already seen
                const auto& node = st.nodes.at(id);
                for (const auto& cid : node.children) {
                    auto itc = st.nodes.find(cid);
                    if (itc == st.nodes.end()) { std::cerr << "[fuzz] missing child node id="<<cid<<"\n"; ok=false; continue; }
                    if (itc->second.parentId != node.id) { std::cerr << "[fuzz] bad parent link child="<<cid<<" parent="<<itc->second.parentId<<" expected="<<node.id<<"\n"; ok=false; }
                    dfs(cid);
                }
            };
            for (const auto& rid : st.rootOrder) dfs(rid);
            if (seen.size() != st.nodes.size()) {
                std::cerr << "[fuzz] orphans: seen="<<seen.size()<<" nodes="<<st.nodes.size()<<" after "<<label<<"\n";
                for (const auto& kv : st.nodes) {
                    if (!seen.count(kv.first)) {
                        std::cerr << "  orphan id="<<kv.first<<" parentId="<<kv.second.parentId<<" text='"<<kv.second.text<<"'\n";
                    }
                }
                ok = false;
            }
            // containment counts
            std::unordered_map<std::string,int> contain_count;
            for (const auto& rid : st.rootOrder) contain_count[rid] += 1;
            for (const auto& kv : st.nodes) for (const auto& cid : kv.second.children) contain_count[cid] += 1;
            for (const auto& kv : st.nodes) {
                int c = contain_count[kv.first];
                if (c != 1) { std::cerr << "[fuzz] bad contain count id="<<kv.first<<" count="<<c<<"\n"; ok=false; }
            }
            return ok;
        };

        for (int i = 0; i < 1000; ++i) {
            // choose a command
            std::uniform_int_distribution<int> cmdDist(0, 8);
            int c = cmdDist(rng);
            std::string id = random_id(s);
            switch (c) {
                case 0: // SplitAtCaret
                    s.caret = std::min<int>(1, (int)s.nodes[id].text.size());
                    s = apply_command(s, Command{ CommandType::SplitAtCaret, id, s.caret });
                    if (!check_nonfatal(s, "SplitAtCaret")) return 1;
                    break;
                case 1: // InsertEmptySiblingAfter
                    s = apply_command(s, Command{ CommandType::InsertEmptySiblingAfter, id });
                    if (!check_nonfatal(s, "InsertEmptySiblingAfter")) return 1;
                    break;
                case 2: // Indent
                    s = apply_command(s, Command{ CommandType::Indent, id });
                    if (!check_nonfatal(s, "Indent")) return 1;
                    break;
                case 3: // Outdent
                    s = apply_command(s, Command{ CommandType::Outdent, id });
                    if (!check_nonfatal(s, "Outdent")) return 1;
                    break;
                case 4: // MoveUp
                    s = apply_command(s, Command{ CommandType::MoveUp, id });
                    if (!check_nonfatal(s, "MoveUp")) return 1;
                    break;
                case 5: // MoveDown
                    s = apply_command(s, Command{ CommandType::MoveDown, id });
                    if (!check_nonfatal(s, "MoveDown")) return 1;
                    break;
                case 6: // DeleteEmptyAtId (randomize text emptiness)
                    maybe_text(s, id);
                    s = apply_command(s, Command{ CommandType::DeleteEmptyAtId, id });
                    if (!check_nonfatal(s, "DeleteEmptyAtId")) return 1;
                    break;
                case 7: // MergeNextSiblingIntoCurrent
                    if (!s.nodes[id].children.empty()) {
                        // precondition not met; skip
                        continue;
                    }
                    s = apply_command(s, Command{ CommandType::MergeNextSiblingIntoCurrent, id });
                    if (!check_nonfatal(s, "MergeNextSiblingIntoCurrent")) return 1;
                    break;
                case 8: // SetScopeRoot
                    s = apply_command(s, Command{ CommandType::SetScopeRoot, "", -1, std::optional<std::string>(id) });
                    if (!check_nonfatal(s, "SetScopeRoot")) return 1;
                    break;
            }
        }
    }

    // 12) Deep hoist/sink across multiple levels
    {
        reset(s);
        // Build: roots n1, n2; under n2 -> c1 (n3); under c1 -> g1 (n4)
        s.nodes["n1"].text = "R1";
        s = apply_and_check(s, Command{ CommandType::InsertEmptySiblingAfter, "n1" }, +1); // n2
        s.nodes["n2"].text = "R2";
        s = apply_and_check(s, Command{ CommandType::InsertEmptySiblingAfter, "n2" }, +1); // n3
        std::string n3id2 = s.focusedId;
        s.nodes[n3id2].text = "C1";
        s = apply_and_check(s, Command{ CommandType::Indent, n3id2 }); // n3 under n2
        s = apply_and_check(s, Command{ CommandType::InsertEmptySiblingAfter, n3id2 }, +1); // n4
        std::string n4id2 = s.focusedId;
        s.nodes[n4id2].text = "G1";
        s = apply_and_check(s, Command{ CommandType::Indent, n4id2 }); // n4 under n3
        // Sanity: n2.children = [n3]; n3.children = [n4]
        assert_true(s.nodes["n2"].children.size() == 1 && s.nodes["n2"].children[0] == n3id2, "n3 under n2");
        assert_true(s.nodes[n3id2].children.size() == 1 && s.nodes[n3id2].children[0] == n4id2, "n4 under n3");

        // Deep hoist n4 to before n3 (under n2)
        s = apply_and_check(s, Command{ CommandType::MoveUp, n4id2 });
        assert_true(s.nodes[n4id2].parentId == "n2", "n4 hoisted to parent=n2");
        assert_true(s.nodes["n2"].children.size() == 2 && s.nodes["n2"].children[0] == n4id2 && s.nodes["n2"].children[1] == n3id2, "n4 before n3 under n2");
        // Hoist n4 again to root before n2
        s = apply_and_check(s, Command{ CommandType::MoveUp, n4id2 });
        assert_true(s.nodes[n4id2].parentId.empty(), "n4 hoisted to root");
        // n4 inserted before n2 in rootOrder
        auto itn2 = std::find(s.rootOrder.begin(), s.rootOrder.end(), std::string("n2"));
        auto itn4 = std::find(s.rootOrder.begin(), s.rootOrder.end(), n4id2);
        assert_true(itn4 < itn2, "n4 appears before n2 at root");
        // MoveUp n4 again swaps with previous root until first
        s = apply_and_check(s, Command{ CommandType::MoveUp, n4id2 });
        assert_true(s.rootOrder.front() == n4id2, "n4 moved to first root");

        // Deep sink: Build a separate chain under n1 then sink twice
        reset(s);
        // roots n1, n2
        s.nodes["n1"].text = "R1";
        s = apply_and_check(s, Command{ CommandType::InsertEmptySiblingAfter, "n1" }, +1); // n2
        s.nodes["n2"].text = "R2";
        // under n1 -> a (n3) -> b (n4)
        s = apply_and_check(s, Command{ CommandType::InsertEmptySiblingAfter, "n1" }, +1); // n3
        std::string n3b = s.focusedId; s.nodes[n3b].text = "A"; s = apply_and_check(s, Command{ CommandType::Indent, n3b }); // under n1
        s = apply_and_check(s, Command{ CommandType::InsertEmptySiblingAfter, n3b }, +1); // n4
        std::string n4b = s.focusedId; s.nodes[n4b].text = "B"; s = apply_and_check(s, Command{ CommandType::Indent, n4b }); // under n3b
        // Make n1 last root to enable second sink step
        // Current rootOrder likely [n1, n2]; moveDown n1 -> [n2, n1]
        s = apply_and_check(s, Command{ CommandType::MoveDown, "n1" });
        assert_true(s.rootOrder.back() == "n1", "n1 is last root");
        // Sink n4b: last in its siblings -> becomes next sibling of its parent (n3b) under n1
        s = apply_and_check(s, Command{ CommandType::MoveDown, n4b });
        auto& ch = s.nodes["n1"].children;
        assert_true(ch.size() == 2 && ch[0] == n3b && ch[1] == n4b, "n4b sunk after n3b under n1");
        // Sink n4b again: last under n1 and n1 is last root -> becomes next sibling of n1 at root
        s = apply_and_check(s, Command{ CommandType::MoveDown, n4b });
        assert_true(s.nodes[n4b].parentId.empty(), "n4b sunk to root after n1");
        assert_true(s.rootOrder.back() == n4b, "n4b at end of roots after sink");
    }

    std::cout << "All engine tests passed.\n";
    return 0;
}
