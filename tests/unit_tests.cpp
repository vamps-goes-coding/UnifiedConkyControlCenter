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
#include "placement.h"
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

static void test_wayland_output_round_trip(const fs::path& dir) {
    const fs::path file = dir / "wayland-output.conf";
    write_text(file,
               "conky.config = {\n"
               "    gap_x = 40,\n"
               "    alignment = 'top_left',\n"
               "};\n");

    // Optional key: absent reads as "", it does not throw.
    CHECK_EQ(ConfigParser::get_wayland_output(file), std::string(""));

    // Inserted at the top of the conky.config table, alongside the settings.
    CHECK(ConfigParser::set_wayland_output(file, "HDMI-A-1"));
    CHECK_EQ(ConfigParser::get_wayland_output(file), std::string("HDMI-A-1"));
    CHECK(read_text(file).find("wayland_output = 'HDMI-A-1',") != std::string::npos);

    // Replaced in place on a second write - not appended as a duplicate.
    CHECK(ConfigParser::set_wayland_output(file, "DP-1"));
    CHECK_EQ(ConfigParser::get_wayland_output(file), std::string("DP-1"));
    CHECK(read_text(file).find("HDMI-A-1") == std::string::npos);

    // Unrelated keys and the table itself survive every rewrite.
    CHECK(read_text(file).find("gap_x = 40") != std::string::npos);
    CHECK(read_text(file).find("alignment = 'top_left'") != std::string::npos);
    CHECK(read_text(file).find("conky.config") != std::string::npos);

    // An empty output removes the key outright rather than writing "".
    CHECK(ConfigParser::set_wayland_output(file, ""));
    CHECK_EQ(ConfigParser::get_wayland_output(file), std::string(""));
    CHECK(read_text(file).find("wayland_output") == std::string::npos);

    // Re-clearing an already-absent key is a success, not an error.
    CHECK(ConfigParser::set_wayland_output(file, ""));

    // A file with no conky.config table cannot be edited safely.
    const fs::path tableless = dir / "tableless.conf";
    write_text(tableless, "just some prose\n");
    CHECK(!ConfigParser::set_wayland_output(tableless, "DP-1"));

    // A missing file still throws, like every other getter here.
    bool threw = false;
    try {
        ConfigParser::get_wayland_output(dir / "not-here.conf");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw);
}

static void test_alignment_read(const fs::path& dir) {
    const fs::path file = dir / "alignment.conf";
    write_text(file,
               "conky.config = {\n"
               "    alignment = 'bottom_right',\n"
               "};\n");
    CHECK_EQ(ConfigParser::get_alignment(file), std::string("bottom_right"));

    // Absent reports "" so a caller can distinguish "not specified" from an
    // explicit top_left (conky's own default).
    const fs::path bare = dir / "alignment-absent.conf";
    write_text(bare, "conky.config = {\n    gap_x = 1,\n};\n");
    CHECK_EQ(ConfigParser::get_alignment(bare), std::string(""));
}

// ═════════════════════════════════════════════════════════════════════════════
// Placement
// ═════════════════════════════════════════════════════════════════════════════

// A layout matching a real two-output desktop: DP-1 primary at the origin and
// HDMI-A-1 offset right and down, so their x ranges abut at x = 2560 and the
// y ranges overlap. That shape is exactly what made naive row-layout code
// wrong for this machine.
static std::vector<MonitorRect> demo_layout() {
    return {
        MonitorRect{"DP-1", 0, 0, 2560, 1440, true},
        MonitorRect{"HDMI-A-1", 2560, 360, 1920, 1080, false},
    };
}

static void test_monitor_lookup() {
    const auto m = demo_layout();

    CHECK_EQ(monitor_for_point(m, 0, 0)->name, std::string("DP-1"));
    CHECK_EQ(monitor_for_point(m, 2559, 1439)->name, std::string("DP-1"));
    // HDMI-A-1's top-left corner belongs to it alone: DP-1 stops at x = 2560.
    CHECK_EQ(monitor_for_point(m, 2560, 360)->name, std::string("HDMI-A-1"));
    CHECK_EQ(monitor_for_point(m, 4479, 1439)->name, std::string("HDMI-A-1"));

    // Outside every monitor...
    CHECK(monitor_for_point(m, 4480, 1439) == nullptr);
    CHECK(monitor_for_point(m, -10, -10) == nullptr);

    // ... resolved to the nearest by centre distance.
    CHECK_EQ(nearest_monitor(m, 4480, 1439)->name, std::string("HDMI-A-1"));
    CHECK_EQ(nearest_monitor(m, -10, -10)->name, std::string("DP-1"));

    CHECK_EQ(monitor_by_name(m, "HDMI-A-1")->name, std::string("HDMI-A-1"));
    CHECK(monitor_by_name(m, "") == nullptr);
    CHECK(monitor_by_name(m, "DP-99") == nullptr);

    CHECK(primary_monitor(m) != nullptr);
    CHECK_EQ(primary_monitor(m)->name, std::string("DP-1"));

    // An empty layout must be safe at every entry point, not crash.
    const std::vector<MonitorRect> none;
    CHECK(monitor_for_point(none, 10, 10) == nullptr);
    CHECK(nearest_monitor(none, 10, 10) == nullptr);
    CHECK(monitor_by_name(none, "DP-1") == nullptr);
    CHECK(primary_monitor(none) == nullptr);
}

static void test_relative_desktop_conversion() {
    const auto m = demo_layout();
    const MonitorRect& hdmi = *monitor_by_name(m, "HDMI-A-1");

    // Desktop -> output-relative against HDMI-A-1's origin of (2560, 360).
    const auto rel = to_relative(hdmi, 4100, 730);
    CHECK_EQ(rel.first, 1540);
    CHECK_EQ(rel.second, 370);

    const auto desk = to_desktop(hdmi, rel.first, rel.second);
    CHECK_EQ(desk.first, 4100);
    CHECK_EQ(desk.second, 730);

    // Layer-shell accepts negative margins, so a point left of the monitor
    // must survive the round trip instead of clamping to zero. This is the
    // behaviour the old std::max(0, ...) conversion destroyed.
    const auto neg = to_relative(hdmi, 2000, 360);
    CHECK_EQ(neg.first, -560);
    CHECK_EQ(neg.second, 0);
    const auto neg_back = to_desktop(hdmi, neg.first, neg.second);
    CHECK_EQ(neg_back.first, 2000);

    const DesktopBox box = desktop_bounds(m);
    CHECK_EQ(box.x, 0);
    CHECK_EQ(box.y, 0);
    CHECK_EQ(box.w, 4480);
    CHECK_EQ(box.h, 1440);

    const DesktopBox empty = desktop_bounds({});
    CHECK_EQ(empty.w, 0);
    CHECK_EQ(empty.h, 0);
}

static void test_resolve_output_point() {
    const auto m = demo_layout();

    const ResolvedPoint on_hdmi = resolve_output_point(m, 4100, 730);
    CHECK(on_hdmi.monitor != nullptr);
    CHECK_EQ(on_hdmi.monitor->name, std::string("HDMI-A-1"));
    CHECK_EQ(on_hdmi.rel_x, 1540);
    CHECK_EQ(on_hdmi.rel_y, 370);

    const ResolvedPoint on_dp = resolve_output_point(m, 100, 100);
    CHECK(on_dp.monitor != nullptr);
    CHECK_EQ(on_dp.monitor->name, std::string("DP-1"));
    CHECK_EQ(on_dp.rel_x, 100);
    CHECK_EQ(on_dp.rel_y, 100);

    // A drop outside every monitor still resolves to the nearest one, so the
    // map can never produce a panel with no output assigned.
    const ResolvedPoint past_edge = resolve_output_point(m, 4480, 1439);
    CHECK(past_edge.monitor != nullptr);
    CHECK_EQ(past_edge.monitor->name, std::string("HDMI-A-1"));

    const ResolvedPoint no_monitors = resolve_output_point({}, 10, 10);
    CHECK(no_monitors.monitor == nullptr);
    CHECK_EQ(no_monitors.rel_x, 0);
}

static void test_import_placement() {
    const auto m = demo_layout();

    // X11 positions absolutely: the gaps are desktop coordinates already.
    const PanelPlacement x11 = import_placement(100, 200, "DP-1", false, m);
    CHECK_EQ(x11.x, 100);
    CHECK_EQ(x11.y, 200);
    CHECK_EQ(x11.output, std::string("DP-1"));

    // Wayland: margins inside HDMI-A-1 lift back to desktop coordinates.
    const PanelPlacement wl = import_placement(1540, 370, "HDMI-A-1", true, m);
    CHECK_EQ(wl.x, 4100);
    CHECK_EQ(wl.y, 730);
    CHECK_EQ(wl.output, std::string("HDMI-A-1"));

    // No output in the conf: the ambient one decides which origin applies.
    const PanelPlacement ambient = import_placement(100, 100, "", true, m, "DP-1");
    CHECK_EQ(ambient.x, 100);
    CHECK_EQ(ambient.y, 100);
    CHECK_EQ(ambient.output, std::string("DP-1"));

    // Nothing named anywhere: fall back to the primary monitor.
    const PanelPlacement primary = import_placement(10, 20, "", true, m);
    CHECK_EQ(primary.output, std::string("DP-1"));
    CHECK_EQ(primary.x, 10);
    CHECK_EQ(primary.y, 20);

    // The conf names a monitor absent from the layout (unplugged, renamed):
    // resolve to one that exists instead of storing a phantom reference.
    const PanelPlacement phantom = import_placement(10, 20, "DP-99", true, m);
    CHECK_EQ(phantom.output, std::string("DP-1"));

    // A phantom conf output gives the ambient one the next say, ahead of
    // dropping to the primary - and the chosen origin does the conversion.
    const PanelPlacement repaired =
        import_placement(10, 20, "DP-99", true, m, "HDMI-A-1");
    CHECK_EQ(repaired.output, std::string("HDMI-A-1"));
    CHECK_EQ(repaired.x, 2570);
    CHECK_EQ(repaired.y, 380);

    // No layout known at all: take the gaps at face value, invent nothing.
    const std::vector<MonitorRect> none;
    const PanelPlacement blind = import_placement(70, 80, "HDMI-A-1", true, none);
    CHECK_EQ(blind.x, 70);
    CHECK_EQ(blind.y, 80);
    CHECK_EQ(blind.output, std::string("HDMI-A-1"));
}

static void test_panel_placement_round_trip(const fs::path& config_file) {
    auto& cfg = ConfigManager::instance();

    reset_config();
    CHECK(cfg.get_panel_placements().empty());
    CHECK(!cfg.has_placement("timezones"));
    // A missing entry yields a default-constructed placement, never a throw.
    CHECK_EQ(cfg.get_placement("timezones").x, 0);
    CHECK(cfg.get_placement("timezones").launch_env);

    PanelPlacement p;
    p.output = "HDMI-A-1";
    p.x = 4100;
    p.y = 730;
    p.launch_env = false;  // must survive as false, not revert to the default
    cfg.set_placement("timezones", p);
    CHECK(cfg.has_placement("timezones"));

    PanelPlacement other;
    other.output = "DP-1";
    other.x = 120;
    other.y = 340;
    cfg.set_placement("calendar", other);

    CHECK(cfg.save_config());

    // Simulate a restart: blank slate, then read back from disk.
    reset_config();
    CHECK(cfg.get_panel_placements().empty());
    CHECK(cfg.load_config(config_file));
    CHECK_EQ(cfg.get_panel_placements().size(), std::size_t{2});

    const PanelPlacement back = cfg.get_placement("timezones");
    CHECK_EQ(back.output, std::string("HDMI-A-1"));
    CHECK_EQ(back.x, 4100);
    CHECK_EQ(back.y, 730);
    CHECK(!back.launch_env);

    const PanelPlacement cal = cfg.get_placement("calendar");
    CHECK_EQ(cal.output, std::string("DP-1"));
    CHECK_EQ(cal.x, 120);
    CHECK_EQ(cal.y, 340);
    CHECK(cal.launch_env);

    // Removing one entry leaves the other alone.
    cfg.clear_placement("timezones");
    CHECK(!cfg.has_placement("timezones"));
    CHECK(cfg.save_config());
    reset_config();
    CHECK(cfg.load_config(config_file));
    CHECK(!cfg.has_placement("timezones"));
    CHECK(cfg.has_placement("calendar"));

    cfg.clear_placement("calendar");
    reset_config();
}

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
    RUN(test_wayland_output_round_trip(root.path / "parser"));
    RUN(test_alignment_read(root.path / "parser"));

    std::printf("\nPlacement:\n");
    RUN(test_monitor_lookup());
    RUN(test_relative_desktop_conversion());
    RUN(test_resolve_output_point());
    RUN(test_import_placement());

    std::printf("\nConfigManager:\n");
    RUN(test_hardware_prefs_round_trip(config_file));
    RUN(test_themes_override_persists_only_when_custom(config_file));
    RUN(test_defaults_reset_clears_stale_state(config_file));
    RUN(test_panel_placement_round_trip(config_file));

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
