#include "html_report.h"
#include "advice.h"
#include "../core/config.h"
#include "../core/fsutil.h"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string escape_html(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::vector<const core::FileEntry*> sorted_entries(const scanner::FileMap& files) {
    std::vector<const core::FileEntry*> entries;
    entries.reserve(files.size());
    for (const auto& item : files) {
        entries.push_back(&item.second);
    }
    std::sort(entries.begin(), entries.end(),
              [](const core::FileEntry* left, const core::FileEntry* right) {
                  return left->path < right->path;
              });
    return entries;
}

std::tm local_time(std::time_t t) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return tm;
}

std::string format_time(std::time_t t) {
    if (t <= 0) {
        return "-";
    }
    const std::tm tm = local_time(t);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

std::string now_string() {
    return format_time(std::time(nullptr));
}

std::string format_duration(double seconds) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3) << seconds << "s";
    return out.str();
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string risk_css(const std::string& risk_level) {
    const std::string risk = lower_copy(risk_level);
    if (risk == "high") {
        return "risk-high";
    }
    if (risk == "medium") {
        return "risk-medium";
    }
    return "risk-low";
}

void write_change_table(std::ofstream& out,
                        const std::string& title,
                        const std::string& status_label,
                        const std::string& pill_class,
                        const std::vector<const core::FileEntry*>& entries) {
    out << "      <section class='panel'>\n";
    out << "        <div class='panel-head'>\n";
    out << "          <h2>" << escape_html(title) << "</h2>\n";
    out << "          <span class='count'>" << entries.size() << "</span>\n";
    out << "        </div>\n";
    if (entries.empty()) {
        out << "        <p class='empty'>No entries in this category for this scan.</p>\n";
        out << "      </section>\n";
        return;
    }

    out << "        <div class='table-wrap'>\n";
    out << "          <table>\n";
    out << "            <thead>\n";
    out << "              <tr>\n";
    out << "                <th>Status</th>\n";
    out << "                <th>Path</th>\n";
    out << "                <th>Size (bytes)</th>\n";
    out << "                <th>Modified Time</th>\n";
    out << "                <th>SHA-256</th>\n";
    out << "              </tr>\n";
    out << "            </thead>\n";
    out << "            <tbody>\n";
    for (const core::FileEntry* entry : entries) {
        out << "              <tr>\n";
        out << "                <td><span class='pill " << pill_class << "'>"
            << escape_html(status_label) << "</span></td>\n";
        out << "                <td class='path'><code>" << escape_html(entry->path) << "</code></td>\n";
        out << "                <td class='num'>" << entry->size << "</td>\n";
        out << "                <td>" << escape_html(format_time(entry->mtime)) << "</td>\n";
        out << "                <td class='hash'><code>" << escape_html(entry->hash) << "</code></td>\n";
        out << "              </tr>\n";
    }
    out << "            </tbody>\n";
    out << "          </table>\n";
    out << "        </div>\n";
    out << "      </section>\n";
}

void write_advisor_list(std::ofstream& out,
                        const std::string& title,
                        const std::vector<std::string>& lines,
                        const std::string& empty_text) {
    out << "          <article class='advisor-card'>\n";
    out << "            <h3>" << escape_html(title) << "</h3>\n";
    if (lines.empty()) {
        out << "            <p class='muted'>" << escape_html(empty_text) << "</p>\n";
        out << "          </article>\n";
        return;
    }
    out << "            <ul>\n";
    for (const std::string& line : lines) {
        out << "              <li>" << escape_html(line) << "</li>\n";
    }
    out << "            </ul>\n";
    out << "          </article>\n";
}

} // namespace

namespace reports {

std::string write_html(const scanner::ScanResult& result, const std::string& scan_id) {
    const std::string id = scan_id.empty() ? fsutil::timestamp() : scan_id;
    const std::string file = config::REPORT_HTML_DIR + "/scan_" + id + ".html";

    std::ofstream out(file, std::ios::trunc);
    if (!out.is_open()) {
        return "";
    }

    const AdvisorNarrative narrative = advisor_narrative(result);
    const auto added = sorted_entries(result.added);
    const auto modified = sorted_entries(result.modified);
    const auto deleted = sorted_entries(result.deleted);
    const bool clean = advisor_status(result) == "clean";
    const std::string status = clean ? "CLEAN" : "CHANGES_DETECTED";
    const std::string risk_level = narrative.risk_level.empty() ? (clean ? "low" : "medium")
                                                                 : narrative.risk_level;

    out << "<!DOCTYPE html>\n";
    out << "<html lang='en'>\n";
    out << "<head>\n";
    out << "  <meta charset='UTF-8'>\n";
    out << "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>\n";
    out << "  <title>" << escape_html(config::TOOL_NAME) << " Report</title>\n";
    out << "  <style>\n";
    out << "    :root {\n";
    out << "      --bg:#eef3f8;\n";
    out << "      --panel:#ffffff;\n";
    out << "      --panel-alt:#f7fafc;\n";
    out << "      --ink:#172635;\n";
    out << "      --muted:#5d6d7b;\n";
    out << "      --line:#d6e0e9;\n";
    out << "      --brand:#0b5fc4;\n";
    out << "      --brand-2:#1482cc;\n";
    out << "      --ok:#1f9d62;\n";
    out << "      --warn:#c9870a;\n";
    out << "      --danger:#c34231;\n";
    out << "      --shadow:0 10px 24px rgba(12,33,56,0.08);\n";
    out << "    }\n";
    out << "    * { box-sizing:border-box; }\n";
    out << "    body {\n";
    out << "      margin:0;\n";
    out << "      color:var(--ink);\n";
    out << "      font-family:\"IBM Plex Sans\",\"Segoe UI\",\"Noto Sans\",sans-serif;\n";
    out << "      transition:background-color 180ms ease, color 180ms ease;\n";
    out << "      background:\n";
    out << "        radial-gradient(circle at 8% -8%, #d7e9ff 0, rgba(215,233,255,0) 42%),\n";
    out << "        radial-gradient(circle at 100% 0, #ffe8d3 0, rgba(255,232,211,0) 38%),\n";
    out << "        var(--bg);\n";
    out << "    }\n";
    out << "    body.theme-dark {\n";
    out << "      --bg:#0e1621;\n";
    out << "      --panel:#101c29;\n";
    out << "      --panel-alt:#132435;\n";
    out << "      --ink:#dce8f5;\n";
    out << "      --muted:#95aabc;\n";
    out << "      --line:#2b4155;\n";
    out << "      --brand:#2e84dd;\n";
    out << "      --brand-2:#3ca6e0;\n";
    out << "      --ok:#35b47a;\n";
    out << "      --warn:#d4a330;\n";
    out << "      --danger:#d26857;\n";
    out << "      --shadow:0 10px 24px rgba(0,0,0,0.35);\n";
    out << "      background:\n";
    out << "        radial-gradient(circle at 8% -8%, #1a2d43 0, rgba(26,45,67,0) 42%),\n";
    out << "        radial-gradient(circle at 100% 0, #3a2b20 0, rgba(58,43,32,0) 38%),\n";
    out << "        var(--bg);\n";
    out << "    }\n";
    out << "    .page { max-width:1200px; margin:30px auto 40px; padding:0 20px; }\n";
    out << "    .hero {\n";
    out << "      background:linear-gradient(130deg, #104780, #157abf);\n";
    out << "      color:#f4f9ff;\n";
    out << "      border-radius:16px;\n";
    out << "      padding:24px 28px;\n";
    out << "      box-shadow:var(--shadow);\n";
    out << "    }\n";
    out << "    .hero-top { display:flex; justify-content:space-between; gap:14px; align-items:flex-start; flex-wrap:wrap; }\n";
    out << "    .actions { display:flex; align-items:center; justify-content:flex-end; gap:10px; flex-wrap:wrap; }\n";
    out << "    h1 { margin:0; font-size:28px; line-height:1.2; letter-spacing:0.3px; }\n";
    out << "    .subtitle { margin:8px 0 0 0; color:#d6e9ff; font-size:14px; }\n";
    out << "    .theme-toggle {\n";
    out << "      background:rgba(255,255,255,0.16);\n";
    out << "      border:1px solid rgba(255,255,255,0.34);\n";
    out << "      color:#f4f9ff;\n";
    out << "      border-radius:999px;\n";
    out << "      font-size:12px;\n";
    out << "      font-weight:700;\n";
    out << "      letter-spacing:0.4px;\n";
    out << "      padding:7px 12px;\n";
    out << "      cursor:pointer;\n";
    out << "      transition:all 140ms ease;\n";
    out << "    }\n";
    out << "    .theme-toggle:hover { background:rgba(255,255,255,0.25); }\n";
    out << "    .theme-toggle:focus { outline:2px solid rgba(255,255,255,0.5); outline-offset:2px; }\n";
    out << "    .badges { display:flex; gap:8px; flex-wrap:wrap; }\n";
    out << "    .badge { border-radius:999px; padding:7px 12px; font-size:12px; font-weight:700; letter-spacing:0.4px; }\n";
    out << "    .status-clean { background:#e6f7ef; color:#0f6f40; }\n";
    out << "    .status-change { background:#fdeceb; color:#962f22; }\n";
    out << "    .risk-low { background:#e7f8ef; color:#136f42; }\n";
    out << "    .risk-medium { background:#fff4df; color:#9a6200; }\n";
    out << "    .risk-high { background:#fde9e7; color:#972d22; }\n";
    out << "    .meta { margin-top:14px; display:grid; grid-template-columns:repeat(auto-fit,minmax(220px,1fr)); gap:8px; }\n";
    out << "    .meta-item { background:rgba(255,255,255,0.12); border:1px solid rgba(255,255,255,0.18); border-radius:10px; padding:10px 12px; }\n";
    out << "    .meta-item span { display:block; font-size:11px; text-transform:uppercase; letter-spacing:0.6px; color:#d9eafe; }\n";
    out << "    .meta-item strong { display:block; font-size:13px; margin-top:4px; color:#ffffff; word-break:break-word; }\n";
    out << "    .kpis { margin-top:16px; display:grid; grid-template-columns:repeat(auto-fit,minmax(165px,1fr)); gap:10px; }\n";
    out << "    .kpi { background:var(--panel); border:1px solid var(--line); border-radius:12px; padding:14px; box-shadow:var(--shadow); }\n";
    out << "    .kpi span { display:block; font-size:11px; text-transform:uppercase; color:var(--muted); letter-spacing:0.6px; }\n";
    out << "    .kpi strong { display:block; margin-top:6px; font-size:24px; }\n";
    out << "    .kpi.ok strong { color:var(--ok); }\n";
    out << "    .kpi.warn strong { color:var(--warn); }\n";
    out << "    .kpi.danger strong { color:var(--danger); }\n";
    out << "    .panel {\n";
    out << "      margin-top:16px;\n";
    out << "      background:var(--panel);\n";
    out << "      border:1px solid var(--line);\n";
    out << "      border-radius:12px;\n";
    out << "      box-shadow:var(--shadow);\n";
    out << "      padding:16px;\n";
    out << "    }\n";
    out << "    .panel-head { display:flex; align-items:center; justify-content:space-between; gap:8px; }\n";
    out << "    h2 { margin:0; font-size:18px; }\n";
    out << "    .count { background:var(--panel-alt); border:1px solid var(--line); border-radius:999px; padding:4px 10px; font-size:12px; color:var(--muted); }\n";
    out << "    .empty { margin:12px 0 2px; color:var(--muted); }\n";
    out << "    .table-wrap { overflow:auto; margin-top:12px; }\n";
    out << "    table { width:100%; border-collapse:collapse; min-width:760px; }\n";
    out << "    th, td { border-bottom:1px solid var(--line); padding:10px 8px; vertical-align:top; text-align:left; }\n";
    out << "    th { font-size:12px; color:var(--muted); text-transform:uppercase; letter-spacing:0.5px; }\n";
    out << "    td { font-size:13px; }\n";
    out << "    td.num { text-align:right; white-space:nowrap; }\n";
    out << "    td.path code, td.hash code { font-family:\"IBM Plex Mono\",\"Consolas\",\"Menlo\",monospace; font-size:12px; }\n";
    out << "    td.path code { word-break:break-word; }\n";
    out << "    td.hash code { color:#3c5162; }\n";
    out << "    .pill { border-radius:999px; padding:4px 9px; font-size:11px; font-weight:700; letter-spacing:0.45px; display:inline-block; }\n";
    out << "    .pill-new { background:#e8f7ef; color:#15653f; }\n";
    out << "    .pill-mod { background:#fff3de; color:#875100; }\n";
    out << "    .pill-del { background:#fdebea; color:#922f24; }\n";
    out << "    .advisor-summary { margin:10px 0 0; color:#253a4a; line-height:1.5; }\n";
    out << "    .advisor-grid { margin-top:12px; display:grid; grid-template-columns:repeat(auto-fit,minmax(220px,1fr)); gap:10px; }\n";
    out << "    .advisor-card { background:var(--panel-alt); border:1px solid var(--line); border-radius:10px; padding:12px; }\n";
    out << "    .advisor-card h3 { margin:0 0 8px; font-size:14px; color:#22435e; }\n";
    out << "    .advisor-card ul { margin:0; padding-left:18px; }\n";
    out << "    .advisor-card li { margin:6px 0; font-size:13px; line-height:1.45; }\n";
    out << "    .muted { margin:0; color:var(--muted); }\n";
    out << "    .foot { margin:18px 4px 0; color:var(--muted); font-size:12px; text-align:right; }\n";
    out << "    body.theme-dark .hero { background:linear-gradient(130deg, #103252, #15547e); }\n";
    out << "    body.theme-dark .subtitle { color:#b9d0e7; }\n";
    out << "    body.theme-dark .meta-item { background:rgba(255,255,255,0.08); border-color:rgba(255,255,255,0.16); }\n";
    out << "    body.theme-dark .meta-item span { color:#bfd3e7; }\n";
    out << "    body.theme-dark .theme-toggle { background:rgba(0,0,0,0.22); border-color:rgba(255,255,255,0.22); }\n";
    out << "    body.theme-dark .theme-toggle:hover { background:rgba(0,0,0,0.36); }\n";
    out << "    body.theme-dark td.hash code { color:#9eb6cb; }\n";
    out << "    body.theme-dark .advisor-summary { color:#cbdcf0; }\n";
    out << "    @media (max-width: 760px) {\n";
    out << "      .page { margin-top:16px; padding:0 12px; }\n";
    out << "      .hero { padding:16px; }\n";
      out << "      h1 { font-size:22px; }\n";
    out << "      .kpi strong { font-size:20px; }\n";
    out << "      .actions { justify-content:flex-start; }\n";
    out << "    }\n";
    out << "  </style>\n";
    out << "</head>\n";
    out << "<body class='theme-light'>\n";
    out << "  <main class='page'>\n";
    out << "    <header class='hero'>\n";
    out << "      <div class='hero-top'>\n";
    out << "        <div>\n";
    out << "          <h1>" << escape_html(config::TOOL_NAME + " " + config::VERSION) << " Integrity Report</h1>\n";
    out << "          <p class='subtitle'>Structured host integrity evidence for operations and audits.</p>\n";
    out << "        </div>\n";
    out << "        <div class='actions'>\n";
    out << "          <button id='theme-toggle' class='theme-toggle' type='button' aria-label='Toggle report theme'>Switch to Dark</button>\n";
    out << "          <div class='badges'>\n";
    out << "            <span class='badge " << (clean ? "status-clean" : "status-change") << "'>STATUS: " << status << "</span>\n";
    out << "            <span class='badge " << risk_css(risk_level) << "'>RISK: "
        << escape_html(lower_copy(risk_level)) << "</span>\n";
    out << "          </div>\n";
    out << "        </div>\n";
    out << "      </div>\n";
    out << "      <div class='meta'>\n";
    out << "        <div class='meta-item'><span>Scan ID</span><strong>" << escape_html(id) << "</strong></div>\n";
    out << "        <div class='meta-item'><span>Generated</span><strong>" << escape_html(now_string()) << "</strong></div>\n";
    out << "        <div class='meta-item'><span>Tool</span><strong>"
        << escape_html(config::TOOL_NAME + " " + config::VERSION) << "</strong></div>\n";
    out << "      </div>\n";
    out << "    </header>\n";
    out << "    <section class='kpis'>\n";
    out << "      <article class='kpi'><span>Files Scanned</span><strong>" << result.stats.scanned << "</strong></article>\n";
    out << "      <article class='kpi ok'><span>New Files</span><strong>" << result.stats.added << "</strong></article>\n";
    out << "      <article class='kpi warn'><span>Modified Files</span><strong>" << result.stats.modified << "</strong></article>\n";
    out << "      <article class='kpi danger'><span>Deleted Files</span><strong>" << result.stats.deleted << "</strong></article>\n";
    out << "      <article class='kpi'><span>Duration</span><strong>" << escape_html(format_duration(result.stats.duration))
        << "</strong></article>\n";
    out << "    </section>\n";

    write_change_table(out, "New Files", "NEW", "pill-new", added);
    write_change_table(out, "Modified Files", "MODIFIED", "pill-mod", modified);
    write_change_table(out, "Deleted Files", "DELETED", "pill-del", deleted);

    out << "    <section class='panel'>\n";
    out << "      <div class='panel-head'>\n";
    out << "        <h2>Nano Advisor</h2>\n";
    out << "        <span class='count " << risk_css(risk_level) << "'>risk: "
        << escape_html(lower_copy(risk_level)) << "</span>\n";
    out << "      </div>\n";
    out << "      <p class='advisor-summary'>" << escape_html(narrative.summary) << "</p>\n";
    out << "      <div class='advisor-grid'>\n";
    write_advisor_list(out, "Why This Matters", narrative.whys,
                       "No additional risk rationale was required for this scan.");
    write_advisor_list(out, "What Matters Now", narrative.what_matters,
                       "No urgent follow-up items were identified.");
    write_advisor_list(out, "Teaching Notes", narrative.teaching,
                       "No extra teaching notes were added for this scan.");
    write_advisor_list(out, "Suggested Next Steps", narrative.next_steps,
                       "No next-step actions were suggested.");
    out << "      </div>\n";
    out << "    </section>\n";
    out << "    <p class='foot'>Generated by " << escape_html(config::TOOL_NAME + " " + config::VERSION)
        << " &middot; local-first reporting</p>\n";
    out << "  </main>\n";
    out << "  <script>\n";
    out << "    (function () {\n";
    out << "      var key = 'sentinel-c-report-theme';\n";
    out << "      var body = document.body;\n";
    out << "      var button = document.getElementById('theme-toggle');\n";
    out << "      if (!button) { return; }\n";
    out << "      function applyTheme(theme) {\n";
    out << "        var dark = theme === 'dark';\n";
    out << "        body.classList.toggle('theme-dark', dark);\n";
    out << "        body.classList.toggle('theme-light', !dark);\n";
    out << "        button.textContent = dark ? 'Switch to Light' : 'Switch to Dark';\n";
    out << "      }\n";
    out << "      var saved = null;\n";
    out << "      try { saved = window.localStorage.getItem(key); } catch (e) { saved = null; }\n";
    out << "      var theme = saved || ((window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches) ? 'dark' : 'light');\n";
    out << "      applyTheme(theme);\n";
    out << "      button.addEventListener('click', function () {\n";
    out << "        var next = body.classList.contains('theme-dark') ? 'light' : 'dark';\n";
    out << "        applyTheme(next);\n";
    out << "        try { window.localStorage.setItem(key, next); } catch (e) { }\n";
    out << "      });\n";
    out << "    })();\n";
    out << "  </script>\n";
    out << "</body>\n";
    out << "</html>\n";

    return file;
}

} // namespace reports
