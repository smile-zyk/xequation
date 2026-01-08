#include "equation_dependency_graph_viewer.h"

#include <QAction>
#include <QGraphicsSvgItem>
#include <QHBoxLayout>
#include <QPushButton>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QTimer>
#include <QResizeEvent>
#include <QSvgRenderer>
#include <QFileDialog>
#include <QMessageBox>
#include <QImage>
#include <QPainter>

namespace xequation
{
namespace gui
{

EquationDependencyGraphViewer::EquationDependencyGraphViewer(QWidget *parent)
    : QWidget(parent), scene_(new QGraphicsScene(this)), view_(new QGraphicsView(scene_, this)), svg_item_(nullptr)
{
    SetupUI();
    SetupConnections();
}

EquationDependencyGraphViewer::~EquationDependencyGraphViewer() {}

void EquationDependencyGraphViewer::SetupUI()
{
    setWindowTitle("Equation Dependency Graph Viewer");
    setWindowFlags(
        Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint
    );
    setMinimumSize(800, 600);

    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);

    // Create toolbar
    auto *toolbar = new QToolBar(this);
    toolbar->setFloatable(false);
    toolbar->setMovable(false);

    // Add zoom actions
    auto *zoom_in_action = toolbar->addAction("Zoom In");
    auto *zoom_out_action = toolbar->addAction("Zoom Out");
    auto *reset_zoom_action = toolbar->addAction("Reset Zoom");
    auto *refresh_action = toolbar->addAction("Refresh");
    toolbar->addSeparator();
    auto *save_image_action = toolbar->addAction("Save Image");

    connect(zoom_in_action, &QAction::triggered, this, &EquationDependencyGraphViewer::ZoomIn);
    connect(zoom_out_action, &QAction::triggered, this, &EquationDependencyGraphViewer::ZoomOut);
    connect(reset_zoom_action, &QAction::triggered, this, &EquationDependencyGraphViewer::ResetZoom);
    connect(refresh_action, &QAction::triggered, this, &EquationDependencyGraphViewer::DependencyGraphImageRequested);
    connect(save_image_action, &QAction::triggered, this, &EquationDependencyGraphViewer::SaveImage);

    main_layout->addWidget(toolbar);

    // Setup graphics view
    view_->setRenderHint(QPainter::Antialiasing);
    view_->setRenderHint(QPainter::SmoothPixmapTransform);
    view_->setDragMode(QGraphicsView::ScrollHandDrag);
    view_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    view_->setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    view_->setBackgroundBrush(QBrush(Qt::white));

    main_layout->addWidget(view_);

    setLayout(main_layout);

    adjustSize();
}

void EquationDependencyGraphViewer::SetupConnections() {}

void EquationDependencyGraphViewer::OnDependencyGraphImageGenerated(const QString &image_path)
{
    current_image_path_ = image_path;
    
    // Create SVG renderer to check if file is valid
    QSvgRenderer *renderer = new QSvgRenderer(image_path);
    if (!renderer->isValid())
    {
        delete renderer;
        return;
    }

    // Remove old SVG item if exists
    if (svg_item_)
    {
        scene_->removeItem(svg_item_);
        delete svg_item_;
        svg_item_ = nullptr;
    }

    // Clear scene and add new SVG item
    scene_->clear();
    svg_item_ = new QGraphicsSvgItem();
    svg_item_->setSharedRenderer(renderer);
    scene_->addItem(svg_item_);

    // Set scene rect to match SVG bounds
    scene_->setSceneRect(svg_item_->boundingRect());

    FitToView();
}

void EquationDependencyGraphViewer::OnEquationGroupAdded(const EquationGroup *group)
{
    // Request to regenerate dependency graph
    emit DependencyGraphImageRequested();
}

void EquationDependencyGraphViewer::OnEquationGroupUpdated(
    const EquationGroup *group, bitmask::bitmask<EquationGroupUpdateFlag> update_flags
)
{
    // Request to regenerate dependency graph
    emit DependencyGraphImageRequested();
}

void EquationDependencyGraphViewer::OnEquationGroupRemoving(const EquationGroup *group)
{
    // Request to regenerate dependency graph
    emit DependencyGraphImageRequested();
}

void EquationDependencyGraphViewer::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier)
    {
        // Zoom with Ctrl + wheel
        if (event->angleDelta().y() > 0)
        {
            ZoomIn();
        }
        else
        {
            ZoomOut();
        }
        event->accept();
    }
    else
    {
        QWidget::wheelEvent(event);
    }
}

void EquationDependencyGraphViewer::ZoomIn()
{
    qreal current_scale = view_->transform().m11();
    
    if (current_scale < GetMaxScale())
    {
        view_->scale(1.2, 1.2);
    }
}

void EquationDependencyGraphViewer::ZoomOut()
{
    qreal current_scale = view_->transform().m11();
    
    if (current_scale > GetMinScale())
    {
        view_->scale(1.0 / 1.2, 1.0 / 1.2);
    }
}

qreal EquationDependencyGraphViewer::GetMaxScale() const
{
    return 10.0; // Maximum zoom is 10x
}

qreal EquationDependencyGraphViewer::GetMinScale() const
{
    return 0.1; // Minimum zoom is 0.1x
}

void EquationDependencyGraphViewer::ResetZoom()
{
    view_->resetTransform();
    FitToView();
}

void EquationDependencyGraphViewer::FitToView()
{
    if (!scene_->sceneRect().isEmpty())
    {
        view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
    }
}

void EquationDependencyGraphViewer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // Refit view when window is resized
    if (!scene_->sceneRect().isEmpty())
    {
        QTimer::singleShot(0, this, &EquationDependencyGraphViewer::FitToView);
    }
}

void EquationDependencyGraphViewer::SaveImage()
{
    if (current_image_path_.isEmpty() || !svg_item_)
    {
        QMessageBox::warning(this, "Save Image", "No graph image to save.");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(
        this, "Save Image", QString(), 
        "PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;SVG Image (*.svg);;All Files (*.*)"
    );

    if (fileName.isEmpty())
        return;

    // Determine file format from extension
    QString format = "PNG";
    if (fileName.toLower().endsWith(".jpg") || fileName.toLower().endsWith(".jpeg"))
        format = "JPEG";
    else if (fileName.toLower().endsWith(".svg"))
        format = "SVG";

    if (format == "SVG")
    {
        // Copy SVG file directly
        QFile::remove(fileName);
        if (QFile::copy(current_image_path_, fileName))
        {
            QMessageBox::information(this, "Save Image", "Image saved successfully.");
        }
        else
        {
            QMessageBox::warning(this, "Save Image", "Failed to save SVG file.");
        }
    }
    else
    {
        // Render SVG to raster image
        QSvgRenderer renderer(current_image_path_);
        if (!renderer.isValid())
        {
            QMessageBox::warning(this, "Save Image", "Invalid SVG file.");
            return;
        }

        // Get default size from SVG, or use a reasonable default
        QSize size = renderer.defaultSize();
        if (size.isEmpty() || size.width() < 100 || size.height() < 100)
        {
            size = QSize(1920, 1080); // Default HD size
        }

        QImage image(size, QImage::Format_ARGB32);
        image.fill(Qt::white);

        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        renderer.render(&painter);
        painter.end();

        if (image.save(fileName, format.toStdString().c_str()))
        {
            QMessageBox::information(this, "Save Image", "Image saved successfully.");
        }
        else
        {
            QMessageBox::warning(this, "Save Image", "Failed to save image.");
        }
    }
}

} // namespace gui
} // namespace xequation
