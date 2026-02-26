#include "ui.hpp"
#include "converter.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <string>

using namespace ftxui;

namespace ui {

void run(ScreenInteractive &screen) {
    std::string number_decimal;
    std::string number_binary;
    std::string number_binary_grey;

    std::string error_str;

    auto input_number_decimal = Input(&number_decimal, "Введите число...");
    auto input_number_binary = Input(&number_binary, "Введите число...");
    auto input_number_binary_grey = Input(&number_binary_grey, "Введите число...");

    // functools.partial ehehe
    auto get_conversion_func = [&error_str](std::function<std::string(const std::string &)> conversion_func, const std::string &number_from, std::string &number_to) {
        return [conversion_func, &number_from, &number_to, &error_str]() {
            try {
                number_to = conversion_func(number_from);
                error_str.clear();
            } catch (const std::exception &e) {
                number_to.clear();
                error_str = std::string(" Ошибка: ") + e.what();
            }
        };
    };

    auto arrow_row_1 = Components({
        Button(" ⇃ ", get_conversion_func(converter::decimal_to_binary, number_decimal, number_binary), ButtonOption::Animated(Color::DarkSlateGray1)),
        Button(" ↾ ", get_conversion_func(converter::binary_to_decimal, number_binary, number_decimal), ButtonOption::Animated(Color::DarkSlateGray1)),
    });

    auto arrow_row_2 = Components({
        Button(" ⇃ ", get_conversion_func(converter::binary_to_grey, number_binary, number_binary_grey), ButtonOption::Animated(Color::DarkSlateGray1)),
        Button(" ↾ ", get_conversion_func(converter::grey_to_binary, number_binary_grey, number_binary), ButtonOption::Animated(Color::DarkSlateGray1)),
    });

    auto quit_btn =
        Button(" Выход ", screen.ExitLoopClosure(), ButtonOption::Animated(Color::Red1));

    auto all = Container::Vertical({
        input_number_decimal,
        Container::Horizontal(arrow_row_1),
        input_number_binary,
        Container::Horizontal(arrow_row_2),
        input_number_binary_grey,
        quit_btn,
    });

    auto renderer = Renderer(all, [&]() {
        return vbox({
            text(" Преобразователь систем счисления ") | bold | color(Color::Cyan) | hcenter,
            separator(),
            gridbox(
                {
                    {
                        text("  Десятичное число:") | color(Color::Blue) | vcenter | size(WIDTH, EQUAL, 30),
                        hbox({input_number_decimal->Render() | hscroll_indicator | frame | size(WIDTH, ftxui::GREATER_THAN, 30) | size(WIDTH, LESS_THAN, 80), text(" ")}),
                    },
                    {filler(), hbox({arrow_row_1[0]->Render(), text(" "), arrow_row_1[1]->Render()}) | hcenter},
                    {{
                        text("  Двоичное число:  ") | color(Color::Blue) | vcenter | size(WIDTH, EQUAL, 30),
                        hbox({input_number_binary->Render() | hscroll_indicator | frame | size(WIDTH, ftxui::GREATER_THAN, 30) | size(WIDTH, LESS_THAN, 80), text(" ")}),
                    }},
                    {filler(), hbox({arrow_row_2[0]->Render(), text(" "), arrow_row_2[1]->Render()}) | hcenter},
                    {{
                        text("  Двоичное число (код Грея): ") | color(Color::Blue) | vcenter | size(WIDTH, EQUAL, 30),
                        hbox({input_number_binary_grey->Render() | hscroll_indicator | frame | size(WIDTH, ftxui::GREATER_THAN, 30) | size(WIDTH, LESS_THAN, 80), text(" ")}),
                    }},
                }),
            separator(),
            error_str.empty() ? text("  Ошибок нет!") | color(Color::GreenLight) : text(error_str) | color(Color::Red),
            separator(),
            separatorEmpty(),
            quit_btn->Render() | hcenter,
            separatorEmpty(),
        }) | border;
    });

    screen.Loop(renderer);
}

} // namespace ui
