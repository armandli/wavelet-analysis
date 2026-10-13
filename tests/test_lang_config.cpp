#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <lang/lang_config.h>

// All tests live in namespace wa so unqualified names resolve without
// a global `using namespace wa` at file scope.
namespace wa {

// ── all_languages() ───────────────────────────────────────────────────────────

TEST_CASE("all_languages returns exactly 9 configs", "[lang_config]") {
  CHECK(all_languages().size() == 9);
}

TEST_CASE("all_languages entries are non-empty", "[lang_config]") {
  for (const auto& lang : all_languages()) {
    INFO("checking language: " << lang.name);
    CHECK(not lang.name.empty());
    CHECK(not lang.extensions.empty());
    CHECK(lang.indent_weight > 0.0f);
    CHECK(not lang.line_comment.empty());
    // structural_keywords may be empty for a generic fallback, but every
    // named language should declare at least one structural keyword.
    CHECK(not lang.structural_keywords.empty());
  }
}

TEST_CASE("all_languages names are all unique", "[lang_config]") {
  uset<str> seen;
  for (const auto& lang : all_languages()) {
    INFO("duplicate name: " << lang.name);
    CHECK(seen.insert(lang.name).second);
  }
}

TEST_CASE("all_languages has no duplicate extensions across languages",
    "[lang_config]") {
  uset<str> seen;
  for (const auto& lang : all_languages()) {
    for (const auto& ext : lang.extensions) {
      INFO("duplicate extension '" << ext << "' found in " << lang.name);
      CHECK(seen.insert(ext).second);
    }
  }
}

TEST_CASE("all_languages contains all 9 expected language names",
    "[lang_config]") {
  uset<str> names;
  for (const auto& lang : all_languages()) {
    names.insert(lang.name);
  }
  CHECK(names.contains("c++"));
  CHECK(names.contains("python"));
  CHECK(names.contains("typescript"));
  CHECK(names.contains("go"));
  CHECK(names.contains("rust"));
  CHECK(names.contains("java"));
  CHECK(names.contains("scala"));
  CHECK(names.contains("clojure"));
  CHECK(names.contains("haskell"));
}

TEST_CASE("only clojure has no block comment in all_languages",
    "[lang_config]") {
  for (const auto& lang : all_languages()) {
    if (lang.name == "clojure") {
      INFO("clojure must have nullopt block_comment");
      CHECK_FALSE(lang.block_comment.has_value());
    } else {
      INFO("language '" << lang.name << "' must have a block_comment");
      CHECK(lang.block_comment.has_value());
    }
  }
}

// ── detect_language: C++ ──────────────────────────────────────────────────────

TEST_CASE("detect_language C++ — all five extensions recognised",
    "[lang_config][c++]") {
  for (const char* file : {"foo.cpp", "foo.cc", "foo.cxx", "foo.h", "foo.hpp"}) {
    INFO("file: " << file);
    REQUIRE(detect_language(file).has_value());
    CHECK(detect_language(file)->name == "c++");
  }
}

TEST_CASE("detect_language C++ — indent_weight is 0.25", "[lang_config][c++]") {
  auto lang = detect_language("foo.cpp");
  REQUIRE(lang.has_value());
  CHECK(lang->indent_weight == Catch::Approx(0.25f));
}

TEST_CASE("detect_language C++ — comment syntax", "[lang_config][c++]") {
  auto lang = detect_language("foo.cpp");
  REQUIRE(lang.has_value());
  CHECK(lang->line_comment == "//");
  REQUIRE(lang->block_comment.has_value());
  CHECK(lang->block_comment->first  == "/*");
  CHECK(lang->block_comment->second == "*/");
}

TEST_CASE("detect_language C++ — keyword weights", "[lang_config][c++]") {
  auto lang = detect_language("foo.cpp");
  REQUIRE(lang.has_value());
  const auto& kw = lang->structural_keywords;

  CHECK(kw.at("class")     == Catch::Approx(2.0f));
  CHECK(kw.at("struct")    == Catch::Approx(2.0f));
  CHECK(kw.at("namespace") == Catch::Approx(1.3f));
  CHECK(kw.at("if")        == Catch::Approx(0.3f));
  CHECK(kw.at("for")       == Catch::Approx(0.3f));
  CHECK(kw.at("while")     == Catch::Approx(0.3f));
}

TEST_CASE("detect_language C++ — no keywords from other languages",
    "[lang_config][c++]") {
  const auto& kw = detect_language("foo.cpp")->structural_keywords;
  CHECK_FALSE(kw.contains("fn"));        // Rust
  CHECK_FALSE(kw.contains("func"));      // Go
  CHECK_FALSE(kw.contains("defn"));      // Clojure
  CHECK_FALSE(kw.contains("newtype"));   // Haskell
}

// ── detect_language: Python ───────────────────────────────────────────────────

TEST_CASE("detect_language Python — .py recognised", "[lang_config][python]") {
  REQUIRE(detect_language("foo.py").has_value());
  CHECK(detect_language("foo.py")->name == "python");
}

TEST_CASE("detect_language Python — indent_weight is 0.5",
    "[lang_config][python]") {
  auto lang = detect_language("foo.py");
  REQUIRE(lang.has_value());
  CHECK(lang->indent_weight == Catch::Approx(0.5f));
}

TEST_CASE("detect_language Python — comment syntax", "[lang_config][python]") {
  auto lang = detect_language("foo.py");
  REQUIRE(lang.has_value());
  CHECK(lang->line_comment == "#");
  // Triple-quoted strings serve as docstrings / multi-line comments.
  REQUIRE(lang->block_comment.has_value());
  CHECK(lang->block_comment->first  == "\"\"\"");
  CHECK(lang->block_comment->second == "\"\"\"");
}

TEST_CASE("detect_language Python — keyword weights", "[lang_config][python]") {
  auto lang = detect_language("foo.py");
  REQUIRE(lang.has_value());
  const auto& kw = lang->structural_keywords;

  CHECK(kw.at("class")  == Catch::Approx(2.0f));
  CHECK(kw.at("def")    == Catch::Approx(1.5f));
  CHECK(kw.at("import") == Catch::Approx(0.8f));
  CHECK(kw.at("if")     == Catch::Approx(0.3f));
  CHECK(kw.at("for")    == Catch::Approx(0.3f));
  CHECK(kw.at("while")  == Catch::Approx(0.3f));
}

TEST_CASE("detect_language Python — no keywords from other languages",
    "[lang_config][python]") {
  const auto& kw = detect_language("foo.py")->structural_keywords;
  CHECK_FALSE(kw.contains("fn"));
  CHECK_FALSE(kw.contains("func"));
  CHECK_FALSE(kw.contains("defn"));
}

// ── detect_language: TypeScript / JavaScript ──────────────────────────────────

TEST_CASE("detect_language TypeScript/JS — all four extensions recognised",
    "[lang_config][typescript]") {
  for (const char* file : {"foo.ts", "foo.tsx", "foo.js", "foo.jsx"}) {
    INFO("file: " << file);
    REQUIRE(detect_language(file).has_value());
  }
}

TEST_CASE("detect_language TypeScript/JS — .ts and .js share the same config",
    "[lang_config][typescript]") {
  auto ts = detect_language("foo.ts");
  auto js = detect_language("foo.js");
  REQUIRE(ts.has_value());
  REQUIRE(js.has_value());
  CHECK(ts->name == js->name);
  CHECK(ts->indent_weight == Catch::Approx(js->indent_weight));
}

TEST_CASE("detect_language TypeScript/JS — indent_weight is 0.25",
    "[lang_config][typescript]") {
  CHECK(detect_language("foo.ts")->indent_weight == Catch::Approx(0.25f));
}

TEST_CASE("detect_language TypeScript/JS — comment syntax",
    "[lang_config][typescript]") {
  auto lang = detect_language("foo.ts");
  REQUIRE(lang.has_value());
  CHECK(lang->line_comment == "//");
  REQUIRE(lang->block_comment.has_value());
  CHECK(lang->block_comment->first  == "/*");
  CHECK(lang->block_comment->second == "*/");
}

TEST_CASE("detect_language TypeScript/JS — keyword weights",
    "[lang_config][typescript]") {
  auto lang = detect_language("foo.ts");
  REQUIRE(lang.has_value());
  const auto& kw = lang->structural_keywords;

  CHECK(kw.at("class")     == Catch::Approx(2.0f));
  CHECK(kw.at("interface") == Catch::Approx(1.5f));
  CHECK(kw.at("function")  == Catch::Approx(1.5f));
  CHECK(kw.at("import")    == Catch::Approx(0.8f));
}

// ── detect_language: Go ───────────────────────────────────────────────────────

TEST_CASE("detect_language Go — .go recognised", "[lang_config][go]") {
  REQUIRE(detect_language("foo.go").has_value());
  CHECK(detect_language("foo.go")->name == "go");
}

TEST_CASE("detect_language Go — indent_weight is 0.25", "[lang_config][go]") {
  CHECK(detect_language("foo.go")->indent_weight == Catch::Approx(0.25f));
}

TEST_CASE("detect_language Go — comment syntax", "[lang_config][go]") {
  auto lang = detect_language("foo.go");
  REQUIRE(lang.has_value());
  CHECK(lang->line_comment == "//");
  REQUIRE(lang->block_comment.has_value());
  CHECK(lang->block_comment->first  == "/*");
  CHECK(lang->block_comment->second == "*/");
}

TEST_CASE("detect_language Go — keyword weights", "[lang_config][go]") {
  auto lang = detect_language("foo.go");
  REQUIRE(lang.has_value());
  const auto& kw = lang->structural_keywords;

  CHECK(kw.at("func") == Catch::Approx(1.5f));
  CHECK(kw.at("type") == Catch::Approx(1.5f));
  // Go reuses "interface" and "struct" inside type declarations
  CHECK(kw.at("interface") == Catch::Approx(1.5f));
  CHECK(kw.at("package")   == Catch::Approx(0.8f));
  CHECK(kw.at("import")    == Catch::Approx(0.8f));
}

TEST_CASE("detect_language Go — no keywords from other languages",
    "[lang_config][go]") {
  const auto& kw = detect_language("foo.go")->structural_keywords;
  CHECK_FALSE(kw.contains("fn"));       // Rust
  CHECK_FALSE(kw.contains("function")); // JS
  CHECK_FALSE(kw.contains("defn"));     // Clojure
}

// ── detect_language: Rust ─────────────────────────────────────────────────────

TEST_CASE("detect_language Rust — .rs recognised", "[lang_config][rust]") {
  REQUIRE(detect_language("foo.rs").has_value());
  CHECK(detect_language("foo.rs")->name == "rust");
}

TEST_CASE("detect_language Rust — indent_weight is 0.25",
    "[lang_config][rust]") {
  CHECK(detect_language("foo.rs")->indent_weight == Catch::Approx(0.25f));
}

TEST_CASE("detect_language Rust — comment syntax", "[lang_config][rust]") {
  auto lang = detect_language("foo.rs");
  REQUIRE(lang.has_value());
  CHECK(lang->line_comment == "//");
  REQUIRE(lang->block_comment.has_value());
}

TEST_CASE("detect_language Rust — keyword weights", "[lang_config][rust]") {
  auto lang = detect_language("foo.rs");
  REQUIRE(lang.has_value());
  const auto& kw = lang->structural_keywords;

  CHECK(kw.at("fn")     == Catch::Approx(1.5f));
  CHECK(kw.at("struct") == Catch::Approx(2.0f));
  CHECK(kw.at("enum")   == Catch::Approx(2.0f));
  CHECK(kw.at("trait")  == Catch::Approx(1.5f));
  CHECK(kw.at("impl")   == Catch::Approx(1.3f));
  CHECK(kw.at("mod")    == Catch::Approx(1.3f));
  CHECK(kw.at("use")    == Catch::Approx(0.8f));
}

TEST_CASE("detect_language Rust — no keywords from other languages",
    "[lang_config][rust]") {
  const auto& kw = detect_language("foo.rs")->structural_keywords;
  CHECK_FALSE(kw.contains("func"));     // Go
  CHECK_FALSE(kw.contains("function")); // JS
  CHECK_FALSE(kw.contains("defn"));     // Clojure
  CHECK_FALSE(kw.contains("newtype"));  // Haskell
}

// ── detect_language: Java ─────────────────────────────────────────────────────

TEST_CASE("detect_language Java — .java recognised", "[lang_config][java]") {
  REQUIRE(detect_language("foo.java").has_value());
  CHECK(detect_language("foo.java")->name == "java");
}

TEST_CASE("detect_language Java — indent_weight is 0.25",
    "[lang_config][java]") {
  CHECK(detect_language("foo.java")->indent_weight == Catch::Approx(0.25f));
}

TEST_CASE("detect_language Java — comment syntax", "[lang_config][java]") {
  auto lang = detect_language("foo.java");
  REQUIRE(lang.has_value());
  CHECK(lang->line_comment == "//");
  REQUIRE(lang->block_comment.has_value());
  CHECK(lang->block_comment->first  == "/*");
  CHECK(lang->block_comment->second == "*/");
}

TEST_CASE("detect_language Java — keyword weights", "[lang_config][java]") {
  auto lang = detect_language("foo.java");
  REQUIRE(lang.has_value());
  const auto& kw = lang->structural_keywords;

  CHECK(kw.at("class")     == Catch::Approx(2.0f));
  CHECK(kw.at("interface") == Catch::Approx(1.5f));
  CHECK(kw.at("import")    == Catch::Approx(0.8f));
  CHECK(kw.at("package")   == Catch::Approx(0.8f));
}

// ── detect_language: Scala ────────────────────────────────────────────────────

TEST_CASE("detect_language Scala — .scala recognised",
    "[lang_config][scala]") {
  REQUIRE(detect_language("foo.scala").has_value());
  CHECK(detect_language("foo.scala")->name == "scala");
}

TEST_CASE("detect_language Scala — indent_weight is 0.25",
    "[lang_config][scala]") {
  CHECK(detect_language("foo.scala")->indent_weight == Catch::Approx(0.25f));
}

TEST_CASE("detect_language Scala — comment syntax", "[lang_config][scala]") {
  auto lang = detect_language("foo.scala");
  REQUIRE(lang.has_value());
  CHECK(lang->line_comment == "//");
  REQUIRE(lang->block_comment.has_value());
  CHECK(lang->block_comment->first  == "/*");
  CHECK(lang->block_comment->second == "*/");
}

TEST_CASE("detect_language Scala — keyword weights", "[lang_config][scala]") {
  auto lang = detect_language("foo.scala");
  REQUIRE(lang.has_value());
  const auto& kw = lang->structural_keywords;

  CHECK(kw.at("class")  == Catch::Approx(2.0f));
  CHECK(kw.at("object") == Catch::Approx(2.0f));
  CHECK(kw.at("trait")  == Catch::Approx(2.0f));
  CHECK(kw.at("def")    == Catch::Approx(1.5f));
  CHECK(kw.at("type")   == Catch::Approx(1.5f));
  CHECK(kw.at("val")    == Catch::Approx(0.8f));
  CHECK(kw.at("var")    == Catch::Approx(0.8f));
  CHECK(kw.at("import") == Catch::Approx(0.8f));
  CHECK(kw.at("package")== Catch::Approx(0.8f));
}

TEST_CASE("detect_language Scala — no keywords from other languages",
    "[lang_config][scala]") {
  const auto& kw = detect_language("foo.scala")->structural_keywords;
  CHECK_FALSE(kw.contains("fn"));    // Rust
  CHECK_FALSE(kw.contains("func"));  // Go
  CHECK_FALSE(kw.contains("defn"));  // Clojure
}

// ── detect_language: Clojure ──────────────────────────────────────────────────

TEST_CASE("detect_language Clojure — all three extensions recognised",
    "[lang_config][clojure]") {
  for (const char* file : {"foo.clj", "foo.cljs", "foo.cljc"}) {
    INFO("file: " << file);
    REQUIRE(detect_language(file).has_value());
    CHECK(detect_language(file)->name == "clojure");
  }
}

TEST_CASE("detect_language Clojure — indent_weight is 0.25",
    "[lang_config][clojure]") {
  CHECK(detect_language("foo.clj")->indent_weight == Catch::Approx(0.25f));
}

TEST_CASE("detect_language Clojure — line comment is semicolon",
    "[lang_config][clojure]") {
  CHECK(detect_language("foo.clj")->line_comment == ";");
}

TEST_CASE("detect_language Clojure — no block comment (nullopt)",
    "[lang_config][clojure]") {
  // Clojure's #_ is a reader macro, not a delimiter pair.
  auto lang = detect_language("foo.clj");
  REQUIRE(lang.has_value());
  CHECK_FALSE(lang->block_comment.has_value());
}

TEST_CASE("detect_language Clojure — all cljs and cljc share config",
    "[lang_config][clojure]") {
  auto clj  = detect_language("foo.clj");
  auto cljs = detect_language("foo.cljs");
  auto cljc = detect_language("foo.cljc");
  REQUIRE(clj.has_value());
  REQUIRE(cljs.has_value());
  REQUIRE(cljc.has_value());
  CHECK(clj->name  == cljs->name);
  CHECK(cljs->name == cljc->name);
}

TEST_CASE("detect_language Clojure — keyword weights", "[lang_config][clojure]") {
  auto lang = detect_language("foo.clj");
  REQUIRE(lang.has_value());
  const auto& kw = lang->structural_keywords;

  CHECK(kw.at("defn")            == Catch::Approx(2.0f));
  CHECK(kw.at("defmacro")        == Catch::Approx(2.0f));
  CHECK(kw.at("defprotocol")     == Catch::Approx(2.0f));
  CHECK(kw.at("deftype")         == Catch::Approx(2.0f));
  CHECK(kw.at("defrecord")       == Catch::Approx(2.0f));
  CHECK(kw.at("ns")              == Catch::Approx(1.5f));
  CHECK(kw.at("defmulti")        == Catch::Approx(1.5f));
  CHECK(kw.at("defmethod")       == Catch::Approx(1.5f));
  CHECK(kw.at("def")             == Catch::Approx(1.0f));
  CHECK(kw.at("extend-type")     == Catch::Approx(1.3f));
  CHECK(kw.at("extend-protocol") == Catch::Approx(1.3f));
  CHECK(kw.at("let")             == Catch::Approx(0.3f));
}

TEST_CASE("detect_language Clojure — no keywords from other languages",
    "[lang_config][clojure]") {
  const auto& kw = detect_language("foo.clj")->structural_keywords;
  CHECK_FALSE(kw.contains("fn"));       // Rust
  CHECK_FALSE(kw.contains("func"));     // Go
  CHECK_FALSE(kw.contains("class"));    // not a Clojure structural keyword
  CHECK_FALSE(kw.contains("function")); // JS
}

// ── detect_language: Haskell ──────────────────────────────────────────────────

TEST_CASE("detect_language Haskell — both extensions recognised",
    "[lang_config][haskell]") {
  for (const char* file : {"foo.hs", "foo.lhs"}) {
    INFO("file: " << file);
    REQUIRE(detect_language(file).has_value());
    CHECK(detect_language(file)->name == "haskell");
  }
}

TEST_CASE("detect_language Haskell — indent_weight is 0.4",
    "[lang_config][haskell]") {
  auto lang = detect_language("foo.hs");
  REQUIRE(lang.has_value());
  CHECK(lang->indent_weight == Catch::Approx(0.4f));
}

TEST_CASE("detect_language Haskell — comment syntax",
    "[lang_config][haskell]") {
  auto lang = detect_language("foo.hs");
  REQUIRE(lang.has_value());
  CHECK(lang->line_comment == "--");
  REQUIRE(lang->block_comment.has_value());
  CHECK(lang->block_comment->first  == "{-");
  CHECK(lang->block_comment->second == "-}");
}

TEST_CASE("detect_language Haskell — keyword weights",
    "[lang_config][haskell]") {
  auto lang = detect_language("foo.hs");
  REQUIRE(lang.has_value());
  const auto& kw = lang->structural_keywords;

  CHECK(kw.at("data")     == Catch::Approx(2.0f));
  CHECK(kw.at("newtype")  == Catch::Approx(2.0f));
  CHECK(kw.at("class")    == Catch::Approx(2.0f));
  CHECK(kw.at("instance") == Catch::Approx(2.0f));
  CHECK(kw.at("type")     == Catch::Approx(1.5f));
  CHECK(kw.at("module")   == Catch::Approx(0.8f));
  CHECK(kw.at("import")   == Catch::Approx(0.8f));
  CHECK(kw.at("where")    == Catch::Approx(0.5f));
  CHECK(kw.at("do")       == Catch::Approx(0.3f));
  CHECK(kw.at("let")      == Catch::Approx(0.3f));
}

TEST_CASE("detect_language Haskell — no keywords from other languages",
    "[lang_config][haskell]") {
  const auto& kw = detect_language("foo.hs")->structural_keywords;
  CHECK_FALSE(kw.contains("fn"));    // Rust
  CHECK_FALSE(kw.contains("func"));  // Go
  CHECK_FALSE(kw.contains("defn"));  // Clojure
  CHECK_FALSE(kw.contains("def"));   // Python/Clojure
}

// ── cross-language keyword exclusivity ───────────────────────────────────────

TEST_CASE("fn is exclusive to Rust", "[lang_config][cross]") {
  for (const char* file : {
      "foo.cpp", "foo.py", "foo.ts", "foo.go",
      "foo.java", "foo.scala", "foo.clj", "foo.hs"}) {
    auto lang = detect_language(file);
    REQUIRE(lang.has_value());
    INFO("fn should not appear in " << lang->name);
    CHECK_FALSE(lang->structural_keywords.contains("fn"));
  }
  CHECK(detect_language("foo.rs")->structural_keywords.contains("fn"));
}

TEST_CASE("func is exclusive to Go", "[lang_config][cross]") {
  for (const char* file : {
      "foo.cpp", "foo.py", "foo.ts", "foo.rs",
      "foo.java", "foo.scala", "foo.clj", "foo.hs"}) {
    auto lang = detect_language(file);
    REQUIRE(lang.has_value());
    INFO("func should not appear in " << lang->name);
    CHECK_FALSE(lang->structural_keywords.contains("func"));
  }
  CHECK(detect_language("foo.go")->structural_keywords.contains("func"));
}

TEST_CASE("defn is exclusive to Clojure", "[lang_config][cross]") {
  for (const char* file : {
      "foo.cpp", "foo.py", "foo.ts", "foo.go",
      "foo.rs", "foo.java", "foo.scala", "foo.hs"}) {
    auto lang = detect_language(file);
    REQUIRE(lang.has_value());
    INFO("defn should not appear in " << lang->name);
    CHECK_FALSE(lang->structural_keywords.contains("defn"));
  }
  CHECK(detect_language("foo.clj")->structural_keywords.contains("defn"));
}

TEST_CASE("data as structural keyword is exclusive to Haskell",
    "[lang_config][cross]") {
  for (const char* file : {
      "foo.cpp", "foo.py", "foo.ts", "foo.go",
      "foo.rs", "foo.java", "foo.scala", "foo.clj"}) {
    auto lang = detect_language(file);
    REQUIRE(lang.has_value());
    INFO("data should not be structural in " << lang->name);
    CHECK_FALSE(lang->structural_keywords.contains("data"));
  }
  CHECK(detect_language("foo.hs")->structural_keywords.contains("data"));
}

// ── indent_weight coverage ────────────────────────────────────────────────────

TEST_CASE("indent_weight is 0.25 for all C-family and Clojure languages",
    "[lang_config][indent]") {
  for (const char* file : {
      "foo.cpp", "foo.ts", "foo.go", "foo.rs",
      "foo.java", "foo.scala", "foo.clj"}) {
    auto lang = detect_language(file);
    REQUIRE(lang.has_value());
    INFO("language: " << lang->name);
    CHECK(lang->indent_weight == Catch::Approx(0.25f));
  }
}

TEST_CASE("Python indent_weight (0.5) is highest of all languages",
    "[lang_config][indent]") {
  auto py = detect_language("foo.py");
  REQUIRE(py.has_value());
  CHECK(py->indent_weight == Catch::Approx(0.5f));

  for (const auto& lang : all_languages()) {
    if (lang.name != "python") {
      INFO("python indent_weight should exceed " << lang.name);
      CHECK(py->indent_weight > lang.indent_weight);
    }
  }
}

TEST_CASE("Haskell indent_weight (0.4) is between Python and C-family",
    "[lang_config][indent]") {
  auto hs  = detect_language("foo.hs");
  auto py  = detect_language("foo.py");
  auto cpp = detect_language("foo.cpp");
  REQUIRE(hs.has_value());
  REQUIRE(py.has_value());
  REQUIRE(cpp.has_value());
  CHECK(hs->indent_weight == Catch::Approx(0.4f));
  CHECK(hs->indent_weight < py->indent_weight);
  CHECK(hs->indent_weight > cpp->indent_weight);
}

// ── path handling ─────────────────────────────────────────────────────────────

TEST_CASE("detect_language handles absolute paths", "[lang_config][path]") {
  REQUIRE(detect_language("/home/user/project/main.cpp").has_value());
  CHECK(detect_language("/home/user/project/main.cpp")->name == "c++");

  REQUIRE(detect_language("/usr/share/scripts/tool.py").has_value());
  CHECK(detect_language("/usr/share/scripts/tool.py")->name == "python");

  REQUIRE(detect_language("/opt/project/server.go").has_value());
  CHECK(detect_language("/opt/project/server.go")->name == "go");
}

TEST_CASE("detect_language handles relative paths", "[lang_config][path]") {
  REQUIRE(detect_language("src/analysis/main.cpp").has_value());
  CHECK(detect_language("src/analysis/main.cpp")->name == "c++");

  REQUIRE(detect_language("./scripts/build.py").has_value());
  CHECK(detect_language("./scripts/build.py")->name == "python");
}

TEST_CASE("detect_language uses last extension for multi-dot filenames",
    "[lang_config][path]") {
  // foo.test.ts → last extension is .ts → TypeScript
  auto ts = detect_language("foo.test.ts");
  auto base = detect_language("foo.ts");
  REQUIRE(ts.has_value());
  REQUIRE(base.has_value());
  CHECK(ts->name == base->name);

  // module.spec.js → last extension is .js → same as .ts config
  auto js = detect_language("module.spec.js");
  REQUIRE(js.has_value());
  CHECK(js->name == base->name);
}

TEST_CASE("detect_language returns same config for .h and .cpp",
    "[lang_config][path]") {
  auto h   = detect_language("foo.h");
  auto cpp = detect_language("foo.cpp");
  REQUIRE(h.has_value());
  REQUIRE(cpp.has_value());
  CHECK(h->name == cpp->name);
}

// ── unknown / edge cases ──────────────────────────────────────────────────────

TEST_CASE("detect_language returns nullopt for common non-code extensions",
    "[lang_config][unknown]") {
  for (const char* file : {
      "foo.txt", "foo.md", "foo.json", "foo.yaml",
      "foo.xml", "foo.toml", "foo.csv", "foo.html"}) {
    INFO("file: " << file);
    CHECK_FALSE(detect_language(file).has_value());
  }
}

TEST_CASE("detect_language returns nullopt for extensionless files",
    "[lang_config][unknown]") {
  CHECK_FALSE(detect_language("Makefile").has_value());
  CHECK_FALSE(detect_language("Dockerfile").has_value());
  CHECK_FALSE(detect_language("LICENSE").has_value());
  CHECK_FALSE(detect_language("README").has_value());
}

TEST_CASE("detect_language returns nullopt for empty string",
    "[lang_config][unknown]") {
  CHECK_FALSE(detect_language("").has_value());
}

TEST_CASE("detect_language returns nullopt for hidden files with no real ext",
    "[lang_config][unknown]") {
  // .gitignore has no meaningful language extension
  CHECK_FALSE(detect_language(".gitignore").has_value());
  CHECK_FALSE(detect_language(".env").has_value());
}

// ── control flow keyword weights (missing languages) ─────────────────────────

TEST_CASE("detect_language Go — control flow keywords at 0.3",
    "[lang_config][go]") {
  auto lang = detect_language("foo.go");
  REQUIRE(lang.has_value());
  const auto& kw = lang->structural_keywords;
  CHECK(kw.at("if")  == Catch::Approx(0.3f));
  CHECK(kw.at("for") == Catch::Approx(0.3f));
  // Go has no `while` keyword; all loops use `for`.
}

TEST_CASE("detect_language Rust — control flow keywords at 0.3",
    "[lang_config][rust]") {
  auto lang = detect_language("foo.rs");
  REQUIRE(lang.has_value());
  const auto& kw = lang->structural_keywords;
  CHECK(kw.at("if")    == Catch::Approx(0.3f));
  CHECK(kw.at("for")   == Catch::Approx(0.3f));
  CHECK(kw.at("while") == Catch::Approx(0.3f));
}

TEST_CASE("detect_language Java — control flow keywords at 0.3",
    "[lang_config][java]") {
  auto lang = detect_language("foo.java");
  REQUIRE(lang.has_value());
  const auto& kw = lang->structural_keywords;
  CHECK(kw.at("if")    == Catch::Approx(0.3f));
  CHECK(kw.at("for")   == Catch::Approx(0.3f));
  CHECK(kw.at("while") == Catch::Approx(0.3f));
}

TEST_CASE("detect_language TypeScript/JS — control flow keywords at 0.3",
    "[lang_config][typescript]") {
  auto lang = detect_language("foo.ts");
  REQUIRE(lang.has_value());
  const auto& kw = lang->structural_keywords;
  CHECK(kw.at("if")    == Catch::Approx(0.3f));
  CHECK(kw.at("for")   == Catch::Approx(0.3f));
  CHECK(kw.at("while") == Catch::Approx(0.3f));
}

// ── TypeScript/JS: no keywords from other languages ───────────────────────────

TEST_CASE("detect_language TypeScript/JS — no keywords from other languages",
    "[lang_config][typescript]") {
  const auto& kw = detect_language("foo.ts")->structural_keywords;
  CHECK_FALSE(kw.contains("fn"));       // Rust
  CHECK_FALSE(kw.contains("func"));     // Go
  CHECK_FALSE(kw.contains("defn"));     // Clojure
  CHECK_FALSE(kw.contains("newtype"));  // Haskell
}

// ── Scala: case / extends / with at 0.5 ──────────────────────────────────────

TEST_CASE("detect_language Scala — case/extends/with keyword weights at 0.5",
    "[lang_config][scala]") {
  auto lang = detect_language("foo.scala");
  REQUIRE(lang.has_value());
  const auto& kw = lang->structural_keywords;
  CHECK(kw.at("case")    == Catch::Approx(0.5f));
  CHECK(kw.at("extends") == Catch::Approx(0.5f));
  CHECK(kw.at("with")    == Catch::Approx(0.5f));
}

// ── Haskell: `in` keyword at 0.3 ─────────────────────────────────────────────

TEST_CASE("detect_language Haskell — `in` keyword at 0.3",
    "[lang_config][haskell]") {
  auto lang = detect_language("foo.hs");
  REQUIRE(lang.has_value());
  CHECK(lang->structural_keywords.at("in") == Catch::Approx(0.3f));
}

// ── Clojure: `do` keyword at 0.3 ─────────────────────────────────────────────

TEST_CASE("detect_language Clojure — `do` keyword at 0.3",
    "[lang_config][clojure]") {
  auto lang = detect_language("foo.clj");
  REQUIRE(lang.has_value());
  CHECK(lang->structural_keywords.at("do") == Catch::Approx(0.3f));
}

// ── fallback / generic config ─────────────────────────────────────────────────

TEST_CASE("default-constructed LanguageConfig is a valid no-keyword fallback",
    "[lang_config][fallback]") {
  LanguageConfig generic{};
  CHECK(generic.structural_keywords.empty());
  CHECK(generic.extensions.empty());
  CHECK(generic.name.empty());
  // indent_weight of 0 is safe: attenuation formula 1/(1+depth*0) = 1 for all depths.
  CHECK(generic.indent_weight == Catch::Approx(0.0f));
  CHECK(not generic.block_comment.has_value());
}

TEST_CASE("detect_language value_or(LanguageConfig{}) is safe for unknown extension",
    "[lang_config][fallback]") {
  // Callers obtain a no-keyword fallback via value_or — the result must be
  // structurally valid and have an empty keyword map.
  auto cfg = detect_language("foo.txt").value_or(LanguageConfig{});
  CHECK(cfg.structural_keywords.empty());
  CHECK(cfg.name.empty());
}

TEST_CASE("detect_language value_or preserves config for a known language",
    "[lang_config][fallback]") {
  auto cfg = detect_language("foo.py").value_or(LanguageConfig{});
  CHECK(cfg.name == "python");
  CHECK(cfg.indent_weight == Catch::Approx(0.5f));
  CHECK_FALSE(cfg.structural_keywords.empty());
}

}  // namespace wa
