#include <lang/lang_config.h>

namespace wa {

const vec<LanguageConfig>& all_languages() {
  // TODO(step2): populate with all 9 language configs.
  // Each entry needs: name, extensions, structural_keywords (with weights
  // from TODO.md §Step 2), indent_weight, line_comment, block_comment.
  //
  // Language  | indent_weight | line_comment | block_comment
  // ----------|---------------|--------------|------------------------
  // c++       | 0.25          | "//"         | {"/*","*/"}
  // python    | 0.50          | "#"          | {"\"\"\"","\"\"\""}
  // typescript| 0.25          | "//"         | {"/*","*/"}
  // go        | 0.25          | "//"         | {"/*","*/"}
  // rust      | 0.25          | "//"         | {"/*","*/"}
  // java      | 0.25          | "//"         | {"/*","*/"}
  // scala     | 0.25          | "//"         | {"/*","*/"}
  // clojure   | 0.25          | ";"          | nullopt
  // haskell   | 0.40          | "--"         | {"{-","-}"}
  static const vec<LanguageConfig> langs = {};
  return langs;
}

opt<LanguageConfig> detect_language(strv path) {
  // TODO(step2): extract the extension from `path` (last dot-separated
  // segment of the filename component), then search all_languages() for
  // a config whose extensions vec contains it.
  // Return nullopt if path is empty, has no extension, or no match found.
  return s::nullopt;
}

}  // namespace wa
