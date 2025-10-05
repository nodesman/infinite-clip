#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
#include "bullet_engine/types.hpp"
#include "bullet_engine/state_utils.hpp"

using namespace emscripten;
using namespace bullet;

// A thin wrapper that owns a State and applies commands.
class EngineWasm {
public:
  EngineWasm() : s_(initial_state()) {}

  std::string focusedId() const { return s_.focusedId; }
  int caret() const { return s_.caret; }

  // Apply a command by components; id can be empty to target current focus.
  void applyCommand(int type, std::string id, int caret, std::string scopeRoot) {
    Command cmd;
    cmd.type = static_cast<CommandType>(type);
    cmd.id = std::move(id);
    cmd.caret = caret;
    if (!scopeRoot.empty()) cmd.scopeRootId = scopeRoot; else cmd.scopeRootId = std::nullopt;
    s_ = apply_command(s_, cmd);
  }

  // Minimal accessors for UI to read/update text when needed
  std::string getText(const std::string& id) const {
    auto it = s_.nodes.find(id);
    return it == s_.nodes.end() ? std::string() : it->second.text;
  }
  void setText(const std::string& id, const std::string& text) {
    auto it = s_.nodes.find(id);
    if (it != s_.nodes.end()) it->second.text = text;
  }

  // Navigation helpers
  std::string prevVisible(const std::string& id) const { return prev_visible_id(s_, id); }
  std::string nextVisible(const std::string& id) const { return next_visible_id(s_, id); }
  val ancestorsToRoot(const std::string& id) const {
    val arr = val::array();
    auto chain = ancestors_to_root(s_, id);
    for (size_t i = 0; i < chain.size(); ++i) arr.set(i, chain[i]);
    return arr;
  }

  // Root order snapshot for rendering
  val rootOrder() const {
    val arr = val::array();
    for (size_t i = 0; i < s_.rootOrder.size(); ++i) arr.set(i, s_.rootOrder[i]);
    return arr;
  }
  // Children of id
  val children(const std::string& id) const {
    val arr = val::array();
    auto it = s_.nodes.find(id);
    if (it == s_.nodes.end()) return arr;
    const auto& ch = it->second.children;
    for (size_t i = 0; i < ch.size(); ++i) arr.set(i, ch[i]);
    return arr;
  }

private:
  State s_;
};

EMSCRIPTEN_BINDINGS(bullet_engine_module) {
  class_<EngineWasm>("Engine")
      .constructor<>()
      .function("applyCommand", &EngineWasm::applyCommand)
      .function("focusedId", &EngineWasm::focusedId)
      .function("caret", &EngineWasm::caret)
      .function("getText", &EngineWasm::getText)
      .function("setText", &EngineWasm::setText)
      .function("prevVisible", &EngineWasm::prevVisible)
      .function("nextVisible", &EngineWasm::nextVisible)
      .function("ancestorsToRoot", &EngineWasm::ancestorsToRoot)
      .function("rootOrder", &EngineWasm::rootOrder)
      .function("children", &EngineWasm::children);
}

#endif // __EMSCRIPTEN__
