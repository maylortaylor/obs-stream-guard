#pragma once

#include <QWidget>

namespace sg {

class Controller;

// Builds the dock panel. OBS owns the widget after it is docked.
QWidget *createDock(Controller *controller);

} // namespace sg
