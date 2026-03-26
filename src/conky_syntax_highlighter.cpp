#include "conky_syntax_highlighter.h"

ConkySyntaxHighlighter::ConkySyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    // Keyword format (Conky configuration keywords)
    keywordFormat.setForeground(QColor("#569CD6"));  // Blue
    keywordFormat.setFontWeight(QFont::Bold);
    
    QStringList keywordPatterns = {
        "\\bown_window\\b", "\\bown_window_transparent\\b", "\\bown_window_colour\\b",
        "\\bown_window_hints\\b", "\\bbackground\\b", "\\bupdate_interval\\b",
        "\\btotal_run_times\\b", "\\bminimum_size\\b", "\\bmaximum_width\\b",
        "\\bdefault_bar_size\\b", "\\bdefault_gauge_size\\b", "\\bdefault_graph_size\\b",
        "\\bdefault_shade_color\\b", "\\bdefault_outline_color\\b",
        "\\balignment\\b", "\\bgap_x\\b", "\\bgap_y\\b",
        "\\bborder_inner_margin\\b", "\\bborder_outer_margin\\b", "\\bborder_width\\b",
        "\\bstippled_borders\\b", "\\bdraw_borders\\b", "\\bdraw_graph_borders\\b",
        "\\bdraw_blinds\\b", "\\bdraw_outline\\b", "\\bdraw_shades\\b",
        "\\buse_xft\\b", "\\bxftalpha\\b", "\\bfont\\b",
        "\\buppercase\\b", "\\blowercase\\b", "\\bshort_units\\b",
        "\\bpad_percents\\b", "\\btop_name_width\\b",
        "\\bif_up_strictness\\b", "\\bif_up_speed\\b",
        "\\bcpu_avg_samples\\b", "\\bnet_avg_samples\\b", "\\bdiskio_avg_samples\\b",
        "\\bno_buffers\\b", "\\bout_to_console\\b", "\\bout_to_stderr\\b",
        "\\boverwrite_file\\b", "\\bappend_file\\b", "\\btop_name_verbose\\b",
        "\\bimlib_cache_size\\b"
    };
    
    for (const QString& pattern : keywordPatterns) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }
    
    // Comment format (lines starting with #)
    commentFormat.setForeground(QColor("#6A9955"));  // Green
    commentFormat.setFontItalic(true);
    HighlightingRule commentRule;
    commentRule.pattern = QRegularExpression("#.*$");
    commentRule.format = commentFormat;
    highlightingRules.append(commentRule);
    
    // String format (text in quotes)
    stringFormat.setForeground(QColor("#CE9178"));  // Orange
    HighlightingRule stringRule;
    stringRule.pattern = QRegularExpression("\".*\"");
    stringRule.format = stringFormat;
    highlightingRules.append(stringRule);
    
    // Number format
    numberFormat.setForeground(QColor("#B5CEA8"));  // Light green
    HighlightingRule numberRule;
    numberRule.pattern = QRegularExpression("\\b[0-9]+\\.?[0-9]*\\b");
    numberRule.format = numberFormat;
    highlightingRules.append(numberRule);
    
    // Color format (#RRGGBB or #RGB)
    colorFormat.setForeground(QColor("#D7BA7D"));  // Yellow
    HighlightingRule colorRule;
    colorRule.pattern = QRegularExpression("#[0-9A-Fa-f]{3,6}\\b");
    colorRule.format = colorFormat;
    highlightingRules.append(colorRule);
    
    // Variable format (${...})
    variableFormat.setForeground(QColor("#9CDCFE"));  // Light blue
    HighlightingRule variableRule;
    variableRule.pattern = QRegularExpression("\\$\\{[^}]+\\}");
    variableRule.format = variableFormat;
    highlightingRules.append(variableRule);
}

void ConkySyntaxHighlighter::highlightBlock(const QString& text)
{
    for (const HighlightingRule& rule : highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}