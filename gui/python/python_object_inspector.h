#pragma once

#include <QWidget>
#include <QtTreePropertyBrowser>
#include <QtVariantPropertyManager>

#ifdef slots
#undef slots
#endif
#include "python/python_base.h"
#define slots Q_SLOTS
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
    std::string object_name_;
    pybind11::object object_;
    QtTreePropertyBrowser* property_browser_;
    QtVariantPropertyManager* variant_manager_;
};
} // namespace python
} // namespace gui
} // namespace xequation