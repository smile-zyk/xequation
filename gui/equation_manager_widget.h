#pragma once

#include "core/equation_manager.h"
#include <QWidget>


namespace xequation
{
namespace gui
{
class EquationManagerWidget : QWidget
{
  public:
    EquationManagerWidget(EquationManager *manager, QWidget *parent) : manager_(manager), QWidget(parent) {}
  private:
    void SetupUI();
    EquationManager *manager_;
};
} // namespace gui
} // namespace xequation