#include "ui.hpp"
#include "converter.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <string>

using namespace ftxui;

namespace ui {

static int parse_base(const std::string &s) {
  try {
    return std::stoi(s);
  } catch (...) {
    return 10;
  }
}

void run(ScreenInteractive &screen) {
  std::string input_number;
  std::string from_base_str = "10";
  std::string to_base_str = "2";

  auto input_num_comp = Input(&input_number, "Введите число...");

  auto valid_base = CatchEvent([](Event e) {
    return e.is_character() &&
           !(e.character()[0] >= '0' && e.character()[0] <= '9');
  });

  auto input_from = Input(&from_base_str, "2–36") | valid_base;
  auto input_to = Input(&to_base_str, "2–36") | valid_base;

  auto quit_btn =
      Button("  Выход  ", screen.ExitLoopClosure(), ButtonOption::Ascii());

  auto all = Container::Vertical({
      input_num_comp,
      input_from,
      input_to,
      quit_btn,
  });

  auto renderer = Renderer(all, [&] {
    int fb = parse_base(from_base_str);
    int tb = parse_base(to_base_str);

    std::string result_str;
    std::string error_str;
    Color output_color = Color::GreenLight;

    if (input_number.empty()) {
      result_str = "(ожидание ввода)";
      output_color = Color::GrayDark;
    } else {
      try {
        result_str = converter::convert(input_number, fb, tb);
      } catch (const std::exception &ex) {
        error_str = ex.what();
        output_color = Color::RedLight;
      }
    }

    auto title = hbox({
        text(" Преобразователь систем счисления ") | bold | color(Color::Cyan),
    });

    auto base_info = [](int b) -> Element {
      static const char *names[] = {"",
                                    "",
                                    "Двоичная",
                                    "",
                                    "",
                                    "",
                                    "",
                                    "",
                                    "Восьмеричная",
                                    "",
                                    "Десятичная",
                                    "",
                                    "",
                                    "",
                                    "",
                                    "",
                                    "Шестнадцатеричная"};
      if (b >= 2 && b <= 16 && names[b][0] != '\0')
        return text(" (" + std::string(names[b]) + ")") |
               color(Color::GrayDark) | dim;
      return text("");
    };

    auto result_elem =
        error_str.empty()
            ? hbox({
                  text("  Результат         : ") | color(Color::Yellow) |
                      size(WIDTH, EQUAL, 22),
                  text(result_str) | color(output_color) | bold,
              })
            : text("  " + error_str) | color(output_color);

    return vbox({
               separatorEmpty(),
               title | hcenter,
               separatorEmpty(),
               separator(),
               separatorEmpty(),
               hbox({text("  Введите число     :") | color(Color::Yellow) |
                         size(WIDTH, EQUAL, 22),
                     input_num_comp->Render(), text("  ")}),
               separatorEmpty(),
               hbox({
                   text("  Из СИ (2–36)      : ") | color(Color::Yellow),
                   input_from->Render() | size(WIDTH, EQUAL, 4),
                   base_info(fb),
               }),
               separatorEmpty(),
               hbox({
                   text("  В СИ  (2–36)      : ") | color(Color::Yellow) |
                       size(WIDTH, EQUAL, 22),
                   input_to->Render() | size(WIDTH, EQUAL, 4),
                   base_info(tb),
               }),
               separatorEmpty(),
               separator(),
               separatorEmpty(),
               result_elem,
               separatorEmpty(),
               separator(),
               separatorEmpty(),
               quit_btn->Render() | hcenter,
               separatorEmpty(),
           }) |
           border | size(WIDTH, EQUAL, 60);
  });

  screen.Loop(renderer);
}

} // namespace ui
