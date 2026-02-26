#include "ui.hpp"
#include <ftxui/component/screen_interactive.hpp>

int main() {
    auto screen = ftxui::ScreenInteractive::FitComponent();
    ui::run(screen);
    return 0;
}
