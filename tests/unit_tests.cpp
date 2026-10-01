// Unit tests for the non-Qt core: config parsing, config persistence, panel
// discovery and display-server resolution.
//
// Dependency-free on purpose - no gtest, so `ctest` runs anywhere the app
// builds. Everything runs against a temporary directory with
// CONKY_CONTROL_CENTER_CONFIG pointed at it, so a developer's real config
// file is never read or written.
//
// Run with:  ctest --test-dir build --output-on-failure

#include "config_parser.h"
#include "config_manager.h"
#include "utils.h"
#include "logger.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;

// ── Tiny assertion framework ─────────────────────────────────────────────────
static int g_checks = 0;
static int g_failures = 0;
static int g_tests_failed = 0;

static std::string describe(const std::string& v) { return "\"" + v + "\""; }
static std::string describe(const std::vector<std::string>& v) {
    std::string out = "[";
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) out += ", ";
        out += v[i];
    }
    return out + "]";
}
template <typename T>
static std::string describe(const T& v) {
    std::ostringstream os;
    os << v;
    return os.str();
}

static void check(bool ok, const std::string& what, int line) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::printf("      FAIL (line %d): %s\n", line, what.c_str());
    }
}

template <typename A, typename B>
static void check_eq(const A& actual, const B& expected, const char* expr, int line) {
    ++g_checks;
    if (!(actual == expected)) {
        ++g_failures;
        std::printf("      FAIL (line %d): %s\n         actual:   %s\n         expected: %s\n",
                    line, expr, describe(actual).c_str(), describe(expected).c_str());
    }
}

#define CHECK(cond) check(static_cast<bool>(cond), #cond, __LINE__)
#define CHECK_EQ(actual, expected) \
    check_eq((actual), (expected), #actual " == " #expected, __LINE__)

#define RUN(call)                                                  \
    do {                                                           \
        const int before = g_failures;                             \
        std::printf("  %s\n", #call);                              \
        call;                                                      \
        if (g_failures != before) {                                \
            ++g_tests_failed;                                      \
            std::printf("    ^ FAILED\n");                         \
        }                                                          \
    } while (0)

// ── Helpers ──────────────────────────────────────────────────────────────────
struct TempDir {
    fs::path path;
    explicit TempDir(const std::string& tag) {
        path = fs::temp_directory_path() /
               ("uccc-test-" + tag + "-" + std::to_string(static_cast<long>(::getpid())));
        std::error_code ec;
        fs::remove_all(path, ec);
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

static void write_text(const fs::path& p, const std::string& content) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary);
    out << content;
}

static std::string read_text(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream os;
    os << in.rdbuf();
    return os.str();
}

// Loading a path that cannot exist resets ConfigManager to compiled defaults.
// (set_defaults() is private, so this is the supported way to get a clean slate.)
static bool reset_config() {
    return ConfigManager::instance().load_config(
        fs::path("/uccc-tests/definitely-not-present.json"));
}

// ═════════════════════════════════════════════════════════════════════════════
// ConfigParser
// ═════════════════════════════════════════════════════════════════════════════

static void test_gap_values_round_trip(const fs::path& dir) {
    const fs::path file = dir / "gaps.conf";
    write_text(file,
               "conky.config = {\n"
               "    gap_x = 40,\n"
               "    gap_y = 60,\n"
               "    alignment = 'top_right',\n"
               "};\n");

    CHECK_EQ(ConfigParser::get_gap_x(file), 40);
    CHECK_EQ(ConfigParser::get_gap_y(file), 60);

    CHECK(ConfigParser::set_gap_values(file, 120, 340));
    CHECK_EQ(ConfigParser::get_gap_x(file), 120);
    CHECK_EQ(ConfigParser::get_gap_y(file), 340);

    // Re-applying values that are already correct must still report success.
    // It used to return false, which made the Gap editor show a spurious
    // "Could not write gap values" warning.
    CHECK(ConfigParser::set_gap_values(file, 120, 340));
    CHECK_EQ(ConfigParser::get_gap_x(file), 120);

    // Unrelated keys survive the rewrite.
    CHECK(read_text(file).find("alignment = 'top_right'") != std::string::npos);
}

static void test_gap_absent_is_a_failure(const fs::path& dir) {
    const fs::path file = dir / "no-gaps.conf";
    write_text(file, "conky.config = {\n    alignment = 'top_left',\n};\n");

    CHECK(!ConfigParser::set_gap_values(file, 10, 10));

    bool threw = false;
    try {
        ConfigParser::get_gap_x(file);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw);
}

static void test_theme_colors_round_trip(const fs::path& dir) {
    const fs::path file = dir / "theme.lua";
    write_text(file,
               "theme = {\n"
               "    name = \"Original\",\n"
               "    color1 = '#00E5FF',\n"
               "    color2 = '#00FF88',\n"
               "    color3 = '#7C4DFF',\n"
               "    color4 = '#FFEA00',\n"
               "}\n");

    auto colors = ConfigParser::extract_colors_from_theme(file);
    CHECK_EQ(colors.size(), std::size_t(4));
    // extract_colors_from_theme keeps the leading '#' - the same form
    // ConfigParser::is_valid_color() expects.
    CHECK_EQ(colors[0], std::string("#00E5FF"));

    const std::vector<std::string> updated = {"#111111", "#222222", "#333333", "#444444"};
    CHECK(ConfigParser::update_theme_colors(file, updated));

    auto after = ConfigParser::extract_colors_from_theme(file);
    CHECK_EQ(after, updated);

    // Metadata survives a colour rewrite.
    const auto meta = ConfigParser::extract_theme_metadata(file);
    CHECK_EQ(meta.at("name"), std::string("Original"));

    // Fewer than four colours is a caller bug, not a silent partial write.
    bool threw = false;
    try {
        ConfigParser::update_theme_colors(file, {"#111111"});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
    CHECK_EQ(ConfigParser::extract_colors_from_theme(file), updated);
}

static void test_csv_round_trip(const fs::path& dir) {
    const fs::path file = dir / "data.csv";
    const std::vector<std::string> headers = {"name", "interface"};
    const std::vector<std::map<std::string, std::string>> rows = {
        {{"name", "Ethernet"}, {"interface", "eth0"}},
        {{"name", "Wi-Fi"}, {"interface", "wlan0"}},
    };

    CHECK(ConfigParser::write_csv_file(file, rows, headers));
    const auto parsed = ConfigParser::parse_csv_file(file);

    CHECK_EQ(parsed.size(), std::size_t(2));
    CHECK_EQ(parsed[0].at("interface"), std::string("eth0"));
    CHECK_EQ(parsed[1].at("name"), std::string("Wi-Fi"));
}

static void test_hex_color_validation() {
    CHECK(ConfigParser::is_valid_color("#00E5FF"));
    CHECK(ConfigParser::is_valid_color("#fff"));
    CHECK(!ConfigParser::is_valid_color("#FFFF"));      // 4 digits
    CHECK(!ConfigParser::is_valid_color("00E5FF"));     // missing '#'
    CHECK(!ConfigParser::is_valid_color("#gg0000"));    // non-hex
    CHECK(!ConfigParser::is_valid_color(""));
}

static void test_conky_config_key_value_parsing(const fs::path& dir) {
    const fs::path file = dir / "kv.conf";
    write_text(file, "gap_x = 40\nown_window = true\n  alignment = 'top_right' \n");

    const auto cfg = ConfigParser::parse_conky_config(file);
    CHECK_EQ(cfg.at("gap_x"), std::string("40"));
    CHECK_EQ(cfg.at("own_window"), std::string("true"));
    CHECK_EQ(cfg.at("alignment"), std::string("'top_right'"));

    bool threw = false;
    try {
        ConfigParser::parse_conky_config(dir / "missing.conf");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw);
}

// ═════════════════════════════════════════════════════════════════════════════
// ConfigManager persistence
// ═════════════════════════════════════════════════════════════════════════════

static void test_hardware_prefs_round_trip(const fs::path& config_file) {
    auto& cfg = ConfigManager::instance();

    reset_config();
    CHECK(cfg.get_hardware_prefs().empty());
    CHECK_EQ(cfg.get_hardware_pref("gpu", "no-gpu"), std::string("no-gpu"));

    cfg.set_hardware_pref("gpu", "NVIDIA GeForce RTX 4090");
    cfg.set_hardware_pref("network", "wlan0");
    cfg.set_hardware_pref("audio", "alsa_output.pci-0000");
    cfg.set_hardware_pref("empty", "");   // must not be stored as ""

    CHECK(cfg.get_hardware_prefs().count("empty") == 0);
    CHECK(cfg.save_config());

    // Simulate a restart: blank slate, then read back from disk.
    reset_config();
    CHECK(cfg.get_hardware_prefs().empty());

    CHECK(cfg.load_config(config_file));
    CHECK_EQ(cfg.get_hardware_pref("gpu", ""), std::string("NVIDIA GeForce RTX 4090"));
    CHECK_EQ(cfg.get_hardware_pref("network", ""), std::string("wlan0"));
    CHECK_EQ(cfg.get_hardware_pref("audio", ""), std::string("alsa_output.pci-0000"));
    CHECK(cfg.get_hardware_prefs().count("empty") == 0);

    // Clearing a preference removes it rather than persisting a blank value.
    cfg.set_hardware_pref("gpu", "");
    CHECK(cfg.get_hardware_prefs().count("gpu") == 0);
    CHECK(cfg.save_config());
    reset_config();
    CHECK(cfg.load_config(config_file));
    CHECK(cfg.get_hardware_prefs().count("gpu") == 0);
}

static void test_themes_override_persists_only_when_custom(const fs::path& config_file) {
    auto& cfg = ConfigManager::instance();

    // A genuinely custom themes folder must survive a restart. First Run Setup
    // used to collect this and then drop it on the floor (session-only env var).
    reset_config();
    cfg.set_themes_path("/custom/themes-location");
    CHECK_EQ(cfg.get_themes_directory().string(), std::string("/custom/themes-location"));
    CHECK(cfg.save_config());

    // Restart: CONKY_THEMES_DIR is process-local, so it would not be set again.
    ::unsetenv("CONKY_THEMES_DIR");
    reset_config();
    CHECK(cfg.load_config(config_file));
    CHECK_EQ(cfg.get_themes_directory().string(), std::string("/custom/themes-location"));

    // A themes path equal to the derived default must NOT be pinned, so that
    // moving the Conky folder later still moves themes with it.
    const std::string derived = (cfg.get_conky_wayland_directory() / "themes").string();
    cfg.set_themes_path(derived + "/");
    CHECK(cfg.save_config());
    ::unsetenv("CONKY_THEMES_DIR");
    reset_config();
    CHECK(cfg.load_config(config_file));
    CHECK_EQ(cfg.get_themes_directory().string(),
             (cfg.get_conky_wayland_directory() / "themes").string());
}

static void test_defaults_reset_clears_stale_state(const fs::path& config_file) {
    auto& cfg = ConfigManager::instance();

    // Use a path inside the scratch dir so the existence check in
    // get_conky_wayland_directory() can never hit a real directory.
    const std::string fake_root = (config_file.parent_path() / "fake-conky-root").string();
    write_text(config_file,
               std::string(R"({"paths": {"display_server": "x11",)")
               + R"("conky_root_override": ")" + fake_root + R"("},)"
               + R"( "panel_discovery": {"config_prefix": "my-prefix-"},)"
               + R"( "hardware": {"gpu": "Intel"}})");
    CHECK(cfg.load_config(config_file));
    CHECK_EQ(cfg.get_display_server(), std::string("x11"));
    CHECK_EQ(cfg.get_hardware_pref("gpu", ""), std::string("Intel"));
    CHECK_EQ(cfg.get_conky_wayland_directory().string(), fake_root);
    CHECK_EQ(cfg.get_config_prefix(), std::string("my-prefix-"));

    // Loading a missing file must genuinely go back to defaults. Previously
    // set_defaults() only reset app_themes_/editors_, so every other member
    // kept the previous load's values.
    reset_config();
    CHECK_EQ(cfg.get_display_server(), std::string("auto"));
    CHECK(cfg.get_hardware_prefs().empty());
    CHECK_EQ(cfg.get_config_prefix(), std::string("conky-wayland-"));
    CHECK(cfg.get_conky_wayland_directory().string() != fake_root);
}

// ═════════════════════════════════════════════════════════════════════════════
// Display-server resolution
// ═════════════════════════════════════════════════════════════════════════════

static void test_display_server_prefix_resolution(const fs::path& config_file) {
    auto& cfg = ConfigManager::instance();

    // An explicit "x11" with no per-server overrides must not fall through to
    // the Wayland-flavoured inline struct defaults.
    write_text(config_file, R"({"paths": {"display_server": "x11"}})");
    CHECK(cfg.load_config(config_file));
    CHECK_EQ(cfg.get_active_display_server_key(), std::string("x11"));
    CHECK_EQ(cfg.get_active_config_prefix(), std::string("conky-x11-"));
    CHECK_EQ(cfg.get_active_config_subdir(), std::string("conky-x11"));
    CHECK_EQ(cfg.get_active_config_extension(), std::string(".conf"));

    // An explicit per-server entry in the file wins over the static fallback.
    write_text(config_file,
               R"({"paths": {"display_server": "x11",
                             "display_servers": {"x11": {"config_prefix": "my-x11-",
                                                         "config_subdir": "mine"}}}})");
    CHECK(cfg.load_config(config_file));
    CHECK_EQ(cfg.get_active_config_prefix(), std::string("my-x11-"));
    CHECK_EQ(cfg.get_active_config_subdir(), std::string("mine"));

    // A blank prefix is normalised back to the compiled default rather than
    // matching nothing and hiding every panel.
    write_text(config_file,
               R"({"paths": {"display_server": "x11",
                             "display_servers": {"x11": {"config_prefix": "",
                                                         "config_extension": ""}}}})");
    CHECK(cfg.load_config(config_file));
    CHECK_EQ(cfg.get_active_config_prefix(), std::string("conky-x11-"));
    CHECK_EQ(cfg.get_active_config_extension(), std::string(".conf"));

    reset_config();
    CHECK_EQ(cfg.get_display_server(), std::string("auto"));
}

// ═════════════════════════════════════════════════════════════════════════════
// Panel discovery
// ═════════════════════════════════════════════════════════════════════════════

static void test_panel_discovery(const fs::path& dir, const fs::path& config_file) {
    auto& cfg = ConfigManager::instance();
    const fs::path panels = dir / "panels";
    fs::create_directories(panels);

    write_text(panels / "conky-wayland-zulu.conf", "conky.config = {};\n");
    write_text(panels / "conky-wayland-alpha.conf", "conky.config = {};\n");
    write_text(panels / "conky-wayland-skip.conf", "conky.config = {};\n");
    write_text(panels / "conky-wayland-README.txt", "not a panel\n");
    write_text(panels / "theme-notes.lua", "-- not a panel either\n");
    fs::create_directories(panels / "conky-wayland-folder.conf");   // not a file

    write_text(config_file,
               R"({
                 "paths": {"display_server": "wayland"},
                 "panel_discovery": {"excluded_files": ["conky-wayland-skip"]}
               })");

    // The env override points discovery at our scratch directory only.
    ::setenv("CONKY_WAYLAND_DIR", panels.c_str(), 1);
    CHECK(cfg.load_config(config_file));

    const auto found = Utils::discover_panels();
    CHECK_EQ(found, std::vector<std::string>({"alpha", "zulu"}));

    // Path resolution round-trips the panel id back to a file name.
    CHECK_EQ(Utils::get_conky_config_path("alpha").string(),
             (panels / "conky-wayland-alpha.conf").string());

    ::unsetenv("CONKY_WAYLAND_DIR");
}

static void test_discovery_on_missing_directory(const fs::path& dir) {
    auto& cfg = ConfigManager::instance();
    reset_config();

    ::setenv("CONKY_WAYLAND_DIR", (dir / "does-not-exist").c_str(), 1);
    CHECK(Utils::discover_panels().empty());
    ::unsetenv("CONKY_WAYLAND_DIR");
}

// ═════════════════════════════════════════════════════════════════════════════

int main() {
    TempDir root("root");
    const fs::path config_file = root.path / "app_config.json";

    // Isolate from (and never write to) the user's real config file.
    ::setenv("CONKY_CONTROL_CENTER_CONFIG", config_file.c_str(), 1);
    ::setenv("XDG_DATA_HOME", (root.path / "data").c_str(), 1);

    Logger::instance().initialize((root.path / "logs").string());

    std::printf("Unified Conky Control Center - unit tests\n\n");

    std::printf("ConfigParser:\n");
    RUN(test_gap_values_round_trip(root.path / "parser"));
    RUN(test_gap_absent_is_a_failure(root.path / "parser"));
    RUN(test_theme_colors_round_trip(root.path / "parser"));
    RUN(test_csv_round_trip(root.path / "parser"));
    RUN(test_hex_color_validation());
    RUN(test_conky_config_key_value_parsing(root.path / "parser"));

    std::printf("\nConfigManager:\n");
    RUN(test_hardware_prefs_round_trip(config_file));
    RUN(test_themes_override_persists_only_when_custom(config_file));
    RUN(test_defaults_reset_clears_stale_state(config_file));

    std::printf("\nDisplay server:\n");
    RUN(test_display_server_prefix_resolution(config_file));

    std::printf("\nPanel discovery:\n");
    RUN(test_panel_discovery(root.path, config_file));
    RUN(test_discovery_on_missing_directory(root.path));

    // Leave no stray state behind for anything running after us.
    ::unsetenv("CONKY_CONTROL_CENTER_CONFIG");
    ::unsetenv("XDG_DATA_HOME");
    reset_config();
    Logger::instance().flush();

    std::printf("\n%d checks, %d failed, %d test(s) failed\n",
                g_checks, g_failures, g_tests_failed);
    return g_failures == 0 ? 0 : 1;
}
