#pragma once

#include "bullet_engine/types.hpp"
#include <string>
#include <vector>

namespace bullet {

// Sibling container helpers
std::vector<std::string>& siblings_ref(State& s, const std::string& id);
const std::vector<std::string>& siblings_cref(const State& s, const std::string& id);
size_t index_in_siblings(const State& s, const std::string& id);

// ID + container editing helpers
std::string make_new_id(State& s);
void insert_after(std::vector<std::string>& vec, const std::string& existing, const std::string& newcomer);
void insert_before(std::vector<std::string>& vec, const std::string& existing, const std::string& newcomer);
void erase_from(std::vector<std::string>& vec, const std::string& id);

// Visibility and ancestry helpers
std::vector<std::string> visible_order_ids(const State& s);
std::vector<std::string> ancestors_to_root(const State& s, const std::string& id);

} // namespace bullet
