#pragma once

#include <QWidget>
#include <QtTreePropertyBrowser>
#include <QtVariantPropertyManager>

#include "python_qt_wrapper.h"

namespace xequation
{
namespace gui
{
namespace python
{
class PythonObjectInspector : public QWidget
{
    Q_OBJECT

  public:
    explicit PythonObjectInspector(QWidget *parent = nullptr);

    

  private:
    struct ObjectProperty
    {
        pybind11::object object;
        QtVariantProperty *property;
    };
    QtTreePropertyBrowser *property_browser_;
    QtVariantPropertyManager *variant_manager_;
    QMap<QString, pybind11::object> object_map_;
};
} // namespace python
} // namespace gui
} // namespace xequation