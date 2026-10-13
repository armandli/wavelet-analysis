#ifndef LANG_LANG_CONFIG_H
#define LANG_LANG_CONFIG_H

#include <util/aliases.h>

namespace wa {

struct LanguageConfig {
  str                    name;
  vec<str>               extensions;
  umap<str, float>       structural_keywords;
  float                  indent_weight;
  str                    line_comment;
  // nullopt for languages with no block comment syntax (e.g. Clojure).
  // Python uses {"\"\"\"","\"\"\""} — same open and close delimiter.
  opt<s::pair<str, str>> block_comment;
};

// Returns all built-in language configs.
const vec<LanguageConfig>& all_languages();

// Returns the LanguageConfig whose extensions include the suffix of `path`.
// Extension matching is case-sensitive and uses the last dot-separated segment.
// Returns nullopt for unknown extensions or paths with no extension.
opt<LanguageConfig> detect_language(strv path);

}  // namespace wa

#endif  // LANG_LANG_CONFIG_H
