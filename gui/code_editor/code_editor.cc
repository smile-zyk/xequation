#include "code_editor.h"

#include <QFile>
#include <QKeyEvent>
#include <QListView>
#include <QCompleter>
#include <QScrollBar>
#include <QShortcut>
#include <QSyntaxStyle>
#include <QWheelEvent>

#include <QDebug>

namespace xequation
{
namespace gui
{
QMap<CodeEditor::StyleMode, QString> CodeEditor::language_style_file_map_ = {
    {CodeEditor::StyleMode::kLight, ":/styles/light_style.xml"},
    {CodeEditor::StyleMode::kDark, ":/styles/dark_style.xml"}
};

CodeEditor::CodeEditor(const QString &language, QWidget *parent) : QCodeEditor(parent), style_mode_(StyleMode::kLight)
{
    Q_INIT_RESOURCE(code_editor_resource);
    SetStyleMode(style_mode_);
    base_font_size_ = font().pointSizeF();
    SetupShortcuts();
    completer_popup_view_ = new QListView(this);
    completer_popup_view_->setUniformItemSizes(true);
}

CodeEditor::~CodeEditor() {}

void CodeEditor::setCompleter(QCompleter *completer)
{
    QCodeEditor::setCompleter(completer);
    if(!completer)
    {
        return;
    }
    completer->setPopup(completer_popup_view_);
    UpdateCompleterPopupView();
}

void CodeEditor::SetupShortcuts()
{
    QShortcut *zoom_in_shortcut = new QShortcut(QKeySequence("Ctrl++"), this);
    QShortcut *zoom_out_shortcut = new QShortcut(QKeySequence("Ctrl+-"), this);
    QShortcut *reset_zoom_shortcut = new QShortcut(QKeySequence("Ctrl+0"), this);

    connect(zoom_in_shortcut, &QShortcut::activated, this, &CodeEditor::ZoomIn);
    connect(zoom_out_shortcut, &QShortcut::activated, this, &CodeEditor::ZoomOut);
    connect(reset_zoom_shortcut, &QShortcut::activated, this, &CodeEditor::ResetZoom);
}

void CodeEditor::SetStyleMode(StyleMode mode)
{
    if (style_map_.find(mode) == style_map_.end())
    {
        QSyntaxStyle *style = new QSyntaxStyle(this);
        QString style_file = language_style_file_map_[mode];
        QFile fl(style_file);
        if (fl.open(QIODevice::ReadOnly))
        {
            if (style->load(fl.readAll()))
            {
                style_map_[mode] = style;
            }
            else
            {
                delete style;
                style_map_[mode] = QSyntaxStyle::defaultStyle();
            }
        }
        else
        {
            style_map_[mode] = QSyntaxStyle::defaultStyle();
        }
    }
    setSyntaxStyle(style_map_[mode]);
    style_mode_ = mode;
    UpdateCompleterPopupView();
}

void CodeEditor::ZoomIn()
{
    if (zoom_factor_ < kMaxZoom)
    {
        zoom_factor_ += kZoomStep;
        if (zoom_factor_ > kMaxZoom)
        {
            zoom_factor_ = kMaxZoom;
        }
        ApplyZoom();
        emit ZoomChanged(zoom_factor_);
    }
}

void CodeEditor::ZoomOut()
{
    if (zoom_factor_ > kMinZoom)
    {
        zoom_factor_ -= kZoomStep;
        if (zoom_factor_ < kMinZoom)
        {
            zoom_factor_ = kMinZoom;
        }
        ApplyZoom();
        emit ZoomChanged(zoom_factor_);
    }
}

void CodeEditor::ResetZoom()
{
    zoom_factor_ = 1.0;
    ApplyZoom();
    emit ZoomChanged(zoom_factor_);
}

void CodeEditor::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier)
    {
        QPoint angleDelta = event->angleDelta();

        if (!angleDelta.isNull())
        {
            if (angleDelta.y() > 0)
            {
                ZoomIn();
            }
            else
            {
                ZoomOut();
            }
            event->accept();
            return;
        }
    }
    QCodeEditor::wheelEvent(event);
}

void CodeEditor::ApplyZoom()
{
    qreal newSize = base_font_size_ * zoom_factor_;

    if (newSize < 6)
        newSize = 6;

    QTextCursor cursor = textCursor();
    int scrollValue = verticalScrollBar()->value();

    QFont font = this->font();
    font.setPointSizeF(newSize);
    setFont(font);

    verticalScrollBar()->setValue(scrollValue);

    UpdateCompleterPopupView();
}

bool CodeEditor::HandleZoomShortcut(QKeyEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier)
    {
        switch (event->key())
        {
        case Qt::Key_Plus:
            ZoomIn();
            return true;

        case Qt::Key_Minus:
            ZoomOut();
            return true;

        case Qt::Key_0:
            ResetZoom();
            return true;

        default:
            break;
        }
    }

    return false;
}

void CodeEditor::UpdateCompleterPopupView()
{
    if (completer_popup_view_)
    {
        completer_popup_view_->setFont(font());
        completer_popup_view_->setPalette(palette());
        completer_popup_view_->update();
    }
}
} // namespace gui
} // namespace xequationa