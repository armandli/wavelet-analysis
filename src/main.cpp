#include <cstdlib>

#include <CLI/CLI.hpp>

#include <util/aliases.h>

namespace wa {

int run(int argc, char** argv) {
  CLI::App app{"Wavelet-based text structure analyzer"};
  app.require_subcommand();

  str   analyze_file;
  float min_coeff = 0.3f;
  int   limit     = 50;
  auto* analyze   = app.add_subcommand(
      "analyze", "Find structural positions via CWT peak detection");
  analyze->add_option("file", analyze_file, "Input file")->required();
  analyze->add_option("--min-coeff,-m", min_coeff,
      "Minimum CWT coefficient threshold");
  analyze->add_option("--limit,-l", limit, "Max positions to return");

  str context_file;
  int context_line   = 0;
  int context_radius = 300;
  auto* ctx = app.add_subcommand(
      "context", "Multi-resolution context around a line");
  ctx->add_option("file", context_file, "Input file")->required();
  ctx->add_option("--line,-n", context_line, "Center line (1-based)")
      ->required();
  ctx->add_option("--radius,-r", context_radius, "Context radius in lines");

  str coeff_file;
  int coeff_start = 0;
  int coeff_end   = -1;
  int coeff_scale = 8;
  auto* coefficients = app.add_subcommand(
      "coefficients", "Raw CWT coefficients for a line range");
  coefficients->add_option("file", coeff_file, "Input file")->required();
  coefficients->add_option("--start,-s", coeff_start, "Start line");
  coefficients->add_option("--end,-e", coeff_end, "End line (-1 = all)");
  coefficients->add_option(
      "--scale", coeff_scale, "Wavelet scale index [1,128]");

  str   heatmap_file;
  auto* heatmap = app.add_subcommand(
      "heatmap", "Complexity heatmap via Haar DWT");
  heatmap->add_option("file", heatmap_file, "Input file")->required();

  str  summary_file;
  int  summary_start = 0;
  int  summary_end   = -1;
  auto* summary = app.add_subcommand(
      "summary", "Wavelet-compressed region summary");
  summary->add_option("file", summary_file, "Input file")->required();
  summary->add_option("--start,-s", summary_start, "Start line");
  summary->add_option("--end,-e", summary_end, "End line (-1 = all)");

  CLI11_PARSE(app, argc, argv);
  return EXIT_SUCCESS;
}

}  // namespace wa

int main(int argc, char** argv) {
  return wa::run(argc, argv);
}
