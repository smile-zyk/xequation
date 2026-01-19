#pragma once
#include <QCodeEditor>
#include <QListView>
#include <qcompleter.h>

namespace xequation 
{
namespace gui 
{
    class CodeEditor : public QCodeEditor 
    {
        Q_OBJECT
    public:
        static constexpr double kZoomStep = 0.1;
        static constexpr double kMinZoom = 0.2;
        static constexpr double kMaxZoom = 5.0;
        enum class StyleMode
        {
            kLight,
            kDark
        };
        CodeEditor(const QString& language, QWidget* parent = nullptr);
        ~CodeEditor() override;

        void setCompleter(QCompleter *completer) override;
        void SetupShortcuts();
        void SetStyleMode(StyleMode mode);
        void SetZoomFactor(double factor);
        void ZoomIn();
        void ZoomOut();
        void ResetZoom();
        StyleMode style_mode() const { return style_mode_; }
        double zoom_factor() const { return zoom_factor_; }
        static double GetMinZoom() { return kMinZoom; }
        static double GetMaxZoom() { return kMaxZoom; }
    signals:
        void ZoomChanged(double zoom_factor);
    protected:
        void wheelEvent(QWheelEvent *event) override;
        void ApplyZoom();
        bool HandleZoomShortcut(QKeyEvent* event);
        void UpdateCompleterPopupView();
    private:
        static QMap<StyleMode, QString> language_style_file_map_;
        QCompleter* completer_{};
        QListView* completer_popup_view_{};
        StyleMode style_mode_;
        QMap<StyleMode, QSyntaxStyle*> style_map_;
        double zoom_factor_ = 1.0;
        double base_font_size_ = 10.0;
    };
}
}