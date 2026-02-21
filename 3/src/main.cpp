#include "bignum.hpp"
#include "generator.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <chrono>
#include <format>
#include <fstream>
#include <string>
#include <vector>

using namespace ftxui;
namespace hrc = std::chrono;

// -----------------------------------------------------------------------
// Содержимое файла
// 1. Вспомогательные функции для отображения и форматирования
// 2. Состояние приложения
// 3. Функция do_execute, которая запускается в отдельном потоке,
//   выполняет выбранную операцию и обновляет состояние
// 4. Функция run_ui, которая запускает интерфейс и обрабатывает взаимодействия
// 5. main, который запускает UI
// -----------------------------------------------------------------------



// -----------------------------------------------------------------------
// Вспомогательные функции для измерения и отображения времени
// -----------------------------------------------------------------------

using Clock     = hrc::high_resolution_clock;
using TimePoint = hrc::time_point<Clock>;

static double ms_between(TimePoint a, TimePoint b) {
    return hrc::duration<double, std::milli>(b - a).count();
}

static std::string fmt_ms(double ms) {
    return std::format("{:.3f} мс", ms);
}

// -----------------------------------------------------------------------
// Состояние приложения
// -----------------------------------------------------------------------

// Доступ к состоянию только через мьютекс для упрощения синхронизации между потоками
struct AppState {
    // Редактируемые числа
    std::string input_a        = "";
    std::string input_b        = "";
    // Пути к файлам и размер генерации
    std::string file_a         = "num_a.txt";
    std::string file_b         = "num_b.txt";
    std::string file_out       = "result.txt";
    std::string gen_bytes_str  = "256";  // размер числа в байтах
    // Параметры операций
    std::string exp_input      = "2";   // степень (1-3)

    // Текущая операция: 0=Сложение, 1=Умножение, 2=Деление,
    //   3=Степень, 4=Простота, 5=Сравнение
    int  selected_op = 0;
    int  target_ab   = 0;  // 0=A, 1=B (для степени и простоты)

    // Результат и статус
    std::string result_text  = "";
    std::string status_msg   = "";     // сообщение об ошибке / инфо
    bool        result_stale = false;  // входы изменились после последнего выполнения
    bool        is_working   = false;  // идёт фоновая работа
    int         spinner_idx  = 0;
    TimePoint   spinner_last = Clock::now();

    // Исключение девяток (для сложения)
    bool show_con            = false;
    int  con_ra = 0, con_rb = 0, con_rs = 0;
    bool con_ok              = false;

    // Тайминги: -1.0 = не применимо, -2.0 = кэширован, >= 0 = время в мс
    double t_parse_a = -1.0;
    double t_parse_b = -1.0;
    double t_op      = -1.0;
    double t_to_dec  = -1.0;

    // Кэш спаршенных чисел
    BigNum      cached_bn_a;
    BigNum      cached_bn_b;
    bool        cache_a_valid = false;
    bool        cache_b_valid = false;

    // Блокировка для рабочего треда
    std::mutex mtx;
};

// -----------------------------------------------------------------------
// Выполнение операции
// -----------------------------------------------------------------------
// Выполняется в отдельном треде
// Копирует все данные, затем проверяет входные данные,
// парсит числа, выполняет операцию и обновляет результат
static void do_execute(AppState &st) {

    // Копируем все изменяемые снаружи данные, нужные для выполнения, в тред
    std::string input_a;
    std::string input_b;
    std::string exp_input;
    int         selected_op = 0;
    int         target_ab   = 0;
    std::string file_out_path;
    bool        cache_a_valid;
    bool        cache_b_valid;
    {
        std::lock_guard<std::mutex> lock(st.mtx);
        input_a       = st.input_a;
        input_b       = st.input_b;
        exp_input     = st.exp_input;
        selected_op   = st.selected_op;
        target_ab     = st.target_ab;
        file_out_path = st.file_out;
        cache_a_valid = st.cache_a_valid;
        cache_b_valid = st.cache_b_valid;
        st.show_con   = false;
        // Сбрасываем тайминги, новые будут установлены по мере выполнения
        st.t_parse_a = st.t_parse_b = st.t_op = st.t_to_dec = -1.0;
    }

    // Убирает все лишние символы из строки, оставляя только десятичные цифры
    auto digits_only = [](const std::string &s) -> std::string {
        std::string r;
        r.reserve(s.size());
        for (char c : s)
            if (c >= '0' && c <= '9') r += c;
        return r;
    };

    std::string sa = digits_only(input_a);
    std::string sb = digits_only(input_b);

    bool should_check_a_valid = true;
    bool should_check_b_valid = true;
    // Для операций, где нужно выбрать число, к которому применяется операция
    // (степень и простота), можно проверять только выбранное число
    if (selected_op == 3 || selected_op == 4) {
        should_check_a_valid = (target_ab == 0);
        should_check_b_valid = (target_ab == 1);
    }

    // Валидация входных чисел
    if (should_check_a_valid && sa.empty()) {
        std::lock_guard<std::mutex> lock(st.mtx);
        st.status_msg   = "Ошибка: число A пустое";
        st.is_working   = false;
        return;
    }
    if (should_check_b_valid && sb.empty()) {
        std::lock_guard<std::mutex> lock(st.mtx);
        st.status_msg   = "Ошибка: число B пустое";
        st.is_working   = false;
        return;
    }
    if (should_check_a_valid && !bignum_is_valid_decimal(sa)) {
        std::lock_guard<std::mutex> lock(st.mtx);
        st.status_msg   = "Ошибка: число A содержит недопустимые символы или ведущие нули";
        st.is_working   = false;
        return;
    }
    if (should_check_b_valid && !bignum_is_valid_decimal(sb)) {
        std::lock_guard<std::mutex> lock(st.mtx);
        st.status_msg   = "Ошибка: число B содержит недопустимые символы или ведущие нули";
        st.is_working   = false;
        return;
    }

    // Проверка введённой степени
    int exp_val = 0;
    if (selected_op == 3) {
        try { exp_val = std::stoi(exp_input); } catch (...) { exp_val = 0; }
        if (exp_val < 1 || exp_val > 3) {
            std::lock_guard<std::mutex> lock(st.mtx);
            st.status_msg   = "Ошибка: степень должна быть от 1 до 3";
            st.is_working   = false;
            return;
        }
    }

    // Парсинг больших чисел
    BigNum bn_a, bn_b;
    if (!sa.empty()) {
        if (cache_a_valid) {
            std::lock_guard<std::mutex> lock(st.mtx);
            bn_a = st.cached_bn_a;
            st.t_parse_a = -2.0; // кэшировано
        } else {
            auto t0 = Clock::now();
            bn_a = bignum_from_decimal(sa);
            auto t1 = Clock::now();
            {
                std::lock_guard<std::mutex> lock(st.mtx);
                st.t_parse_a     = ms_between(t0, t1);
                st.cached_bn_a   = bn_a;
                st.cache_a_valid = true;
            }
        }
    }

    if (!sb.empty()) {
        if (cache_b_valid) {
            std::lock_guard<std::mutex> lock(st.mtx);
            bn_b = st.cached_bn_b;
            st.t_parse_b = -2.0; // кэшировано
        } else {
            auto t0 = Clock::now();
            bn_b = bignum_from_decimal(sb);
            auto t1 = Clock::now();
            {
                std::lock_guard<std::mutex> lock(st.mtx);
                st.t_parse_b     = ms_between(t0, t1);
                st.cached_bn_b   = bn_b;
                st.cache_b_valid = true;
            }
        }
    }

    // Локальные переменные для результатов
    // тайминги
    double      local_t_op     = -1.0;
    double      local_t_to_dec = -1.0;
    // проверка сложения (исключение девяток)
    bool        local_show_con = false;
    int         local_con_ra = 0, local_con_rb = 0, local_con_rs = 0;
    bool        local_con_ok   = false;
    std::string op_result_text;

    try {
        auto op_start = Clock::now();

        // Некоторые операции завершаются идентично
        auto finish_bignum = [&](BigNum bn) {
            local_t_op = ms_between(op_start, Clock::now());
            {
                std::lock_guard<std::mutex> lock(st.mtx);
                st.t_op = local_t_op;
            }
            auto t0        = Clock::now();
            op_result_text = bignum_to_decimal(bn);
            auto t1        = Clock::now();
            local_t_to_dec = ms_between(t0, t1);
        };

        switch (selected_op) {
            case 0: { // Сложение
                BigNum sum  = bignum_add(bn_a, bn_b);

                local_con_ra   = bignum_digit_root(bn_a);
                local_con_rb   = bignum_digit_root(bn_b);
                local_con_rs   = bignum_digit_root(sum);
                local_con_ok   = (local_con_ra + local_con_rb) % 9 == local_con_rs % 9;
                local_show_con = true;

                finish_bignum(sum);
                break;
            }
            case 1: { // Умножение
                finish_bignum(bignum_mul(bn_a, bn_b));
                break;
            }
            case 2: { // Деление с остатком
                if (bignum_is_zero(bn_b)) {
                    std::lock_guard<std::mutex> lock(st.mtx);
                    st.status_msg = "Ошибка: деление на ноль";
                    st.is_working = false;
                    return;
                }
                auto [q, r] = bignum_divmod(bn_a, bn_b);
                local_t_op   = ms_between(op_start, Clock::now());
                {
                    std::lock_guard<std::mutex> lock(st.mtx);
                    st.t_op = local_t_op;
                }

                auto t0 = Clock::now();
                std::string qs     = bignum_to_decimal(q);
                std::string rs_str = bignum_to_decimal(r);
                auto t1 = Clock::now();
                local_t_to_dec = ms_between(t0, t1);

                op_result_text = "Частное:\n" + qs + "\n\nОстаток:\n" + rs_str;
                break;
            }
            case 3: { // Степень
                BigNum &base_bn = (target_ab == 0) ? bn_a : bn_b;
                finish_bignum(bignum_pow(base_bn, exp_val));
                break;
            }
            case 4: { // Простота
                BigNum &target = (target_ab == 0) ? bn_a : bn_b;
                auto t0    = Clock::now();
                bool prime = bignum_is_prime(target);
                local_t_op = ms_between(t0, Clock::now());

                op_result_text = prime
                    ? "Число является простым"
                    : "Число является составным (не простым)";
                break;
            }
            case 5: { // Сравнение
                int cmp    = bignum_cmp(bn_a, bn_b);
                local_t_op = ms_between(op_start, Clock::now());


                if      (cmp < 0) op_result_text = "A < B";
                else if (cmp > 0) op_result_text = "A > B";
                else              op_result_text = "A = B";
                break;
            }
        }
    } catch (const std::exception &ex) {
        std::lock_guard<std::mutex> lock(st.mtx);
        st.status_msg = std::string("Ошибка: ") + ex.what();
        st.is_working = false;
        return;
    }

    std::string save_error;
    try {
        std::ofstream ofs(file_out_path);
        if (!ofs) throw std::runtime_error("Не удалось открыть файл для записи: " + file_out_path);
        ofs << op_result_text;
        if (!ofs) throw std::runtime_error("Ошибка записи в файл: " + file_out_path);
    } catch (const std::exception &ex) {
        save_error = std::string("Ошибка записи результата: ") + ex.what();
    }

    // Запись состояния
    {
        std::lock_guard<std::mutex> lock(st.mtx);
        st.result_text  = op_result_text;
        st.t_op         = local_t_op;
        st.t_to_dec     = local_t_to_dec;
        st.show_con     = local_show_con;
        st.con_ra       = local_con_ra;
        st.con_rb       = local_con_rb;
        st.con_rs       = local_con_rs;
        st.con_ok       = local_con_ok;
        st.result_stale = false;
        st.status_msg   = save_error.empty() ? "Готово" : save_error;
        st.is_working   = false;
    }
}

// -----------------------------------------------------------------------
// Графический интерфейс
// -----------------------------------------------------------------------

// Некоторые вспомогательные функции

static const std::vector<std::string> OP_NAMES = {
    "Сложение",
    "Умножение",
    "Деление с остатком",
    "Возведение в степень",
    "Проверка на простоту",
    "Сравнение",
};

// ms < 0  -> н/д
// ms == -2 -> кэширован (не пересчитывался)
static Element timing_row(const std::string &label, double ms) {
    std::string val;
    Color       col = Color::Default;
    if (ms == -2.0) {
        val = "кэширован";
        col = Color::GrayDark;
    } else if (ms < 0) {
        val = "н/д";
    } else {
        val = fmt_ms(ms);
        col = Color::Default;
    }
    return hbox({
        text(label + ": ") | color(Color::GrayLight),
        text(val) | bold | color(col),
        text("   "),
    });
}

// Разбить длинное число на строки по width символов для удобного отображения
static std::string wrap_number(const std::string &s, size_t width = 80) {
    if (s.empty()) return s;
    std::string result;
    result.reserve(s.size() + s.size() / width + 1);
    for (size_t i = 0; i < s.size(); i += width) {
        if (i > 0) result += '\n';
        result += s.substr(i, width);
    }
    return result;
}

// Подсчет десятичных цифр в строке
static size_t count_digits(const std::string &s) {
    size_t cnt = 0;
    for (char c : s) if (c >= '0' && c <= '9') ++cnt;
    return cnt;
}

// цветные кнопочки
static ButtonOption SmallAnimatedButtonOption(Color color) {
  ButtonOption option;
  option.transform = [](const EntryState& s) {
    auto element = text(s.label);
    if (s.focused) {
      element |= bold;
    }
    return element;
  };
  option.animated_colors.foreground.Set(Color::Interpolate(0.10F, color, Color::White), Color::Interpolate(0.85F, color, Color::White));
  option.animated_colors.background.Set(Color::Interpolate(0.85F, color, Color::Black), Color::Interpolate(0.10F, color, Color::Black));
  return option;
}

// Создаёт все компоненты интерфейса, обработчики
static void run_ui() {
    AppState st;
    auto screen = ScreenInteractive::Fullscreen();

    // Поля ввода чисел (A и B)
    auto input_a   = Input(&st.input_a, "Введите число A...");
    auto input_b   = Input(&st.input_b, "Введите число B...");
    // Остальные поля - однострочные
    InputOption single_line;
    single_line.multiline = false;
    auto input_fa  = Input(&st.file_a, "num_a.txt", single_line);
    auto input_fb  = Input(&st.file_b, "num_b.txt", single_line);
    auto input_gb  = Input(&st.gen_bytes_str, "256", single_line);
    auto input_out = Input(&st.file_out, "result.txt", single_line);
    auto input_exp = Input(&st.exp_input, "1-3", single_line);

    // Просмотр результата
    auto readonly_input = CatchEvent([](Event e) {
        if (e.is_character() || e == Event::Backspace || e == Event::Delete || e == Event::Return)
            return true; // блокируем редактирование
        return false;
    });
    std::string result_display_text;
    InputOption result_input_opt;
    result_input_opt.transform = [&st](InputState state) {
        state.element |= color(Color::White) | vscroll_indicator | hscroll_indicator | frame;

        bool empty_result = false;
        {
            std::lock_guard<std::mutex> lock(st.mtx);
            empty_result = st.result_text.empty();
        }
        if (state.is_placeholder || empty_result) {
            state.element |= color(Color::GrayDark) | dim;
        }

        return state.element;
    };
    auto result_input = Input(&result_display_text, "") | readonly_input;

    // Отмечаем результат устаревшим при любом вводе символа
    auto mark_stale = [&st](std::optional<std::reference_wrapper<bool>> cache_value = std::nullopt) {
        return CatchEvent([&st, cache_value](Event e) {
            if (e.is_character() || e == Event::Backspace || e == Event::Delete) {
                std::lock_guard<std::mutex> lock(st.mtx);
                st.result_stale = true;
                if (cache_value.has_value())
                    cache_value->get() = false; // Помечаем кэш недействительным
            }
            return false; // Пропускаем другие события дальше
        });
    };

    auto input_a_tracked   = input_a   | mark_stale(st.cache_a_valid);
    auto input_b_tracked   = input_b   | mark_stale(st.cache_b_valid);
    auto input_exp_tracked = input_exp | mark_stale();

    // Выпадающий список операций
    int prev_selected_op = st.selected_op;
    int prev_target_ab   = st.target_ab;
    auto dropdown    = Dropdown(&OP_NAMES, &st.selected_op);

    // Радиокнопка "Применить к"
    static const std::vector<std::string> TARGET_ENTRIES = {"Число A", "Число B"};
    auto target_radio = Radiobox(&TARGET_ENTRIES, &st.target_ab);

    auto start_spinner = [&]() {
        std::thread([&st, &screen]() {
            while (true) {
                {
                    std::lock_guard<std::mutex> lock(st.mtx);
                    if (!st.is_working) break;
                    auto now = Clock::now();
                    if (ms_between(st.spinner_last, now) >= 80.0) {
                        st.spinner_last = now;
                        st.spinner_idx  = (st.spinner_idx + 1) % 12;
                        screen.PostEvent(Event::Custom);
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(40));
            }
        }).detach();
    };

    enum class GenKind { A, B, AB };

    auto start_generate = [&](GenKind kind) {
        unsigned int gb = 256;
        std::string file_a, file_b;
        {
            std::lock_guard<std::mutex> lock(st.mtx);
            if (st.is_working) return;
            try { gb = static_cast<unsigned int>(std::stoul(st.gen_bytes_str)); } catch (...) {}
            if (gb < 1) gb = 1;
            file_a = st.file_a;
            file_b = st.file_b;
            st.is_working   = true;
            st.status_msg   = "Выполняется...";
            st.spinner_idx  = 0;
            st.spinner_last = Clock::now();
        }
        start_spinner();

        std::thread([&st, &screen, gb, file_a, file_b, kind]() {
            try {
                generate_and_save(file_a, file_b, gb);
                std::string la = load_from_file(file_a);
                std::string lb = load_from_file(file_b);
                std::lock_guard<std::mutex> lock(st.mtx);
                if (kind == GenKind::A || kind == GenKind::AB) {
                    st.input_a       = wrap_number(la);
                    st.cache_a_valid = false;
                }
                if (kind == GenKind::B || kind == GenKind::AB) {
                    st.input_b       = wrap_number(lb);
                    st.cache_b_valid = false;
                }
                st.result_stale = true;
                st.status_msg   = (kind == GenKind::AB)
                    ? "Оба числа сгенерированы"
                    : (kind == GenKind::A ? "A сгенерировано" : "B сгенерировано");
            } catch (const std::exception &ex) {
                std::lock_guard<std::mutex> lock(st.mtx);
                st.status_msg = std::string("Ошибка генерации: ") + ex.what();
            }
            {
                std::lock_guard<std::mutex> lock(st.mtx);
                st.is_working = false;
            }
            // обновляем рендер после получения результата
            screen.PostEvent(Event::Custom);
        }).detach();
    };

    // Кнопка: генерировать A
    auto btn_gen_a = Button(" Генерировать A ", [&] { start_generate(GenKind::A); }, SmallAnimatedButtonOption(Color::Yellow));

    // Кнопка: генерировать B
    auto btn_gen_b = Button(" Генерировать B ", [&] { start_generate(GenKind::B); }, SmallAnimatedButtonOption(Color::Yellow));

    // Кнопка: генерировать A и B
    auto btn_gen_ab = Button(" Генерировать A и B ", [&] { start_generate(GenKind::AB); }, SmallAnimatedButtonOption(Color::Yellow));

    // Кнопка: загрузить A из файла
    auto btn_restore_a = Button(" Загрузить A ", [&] {
        std::string path_a;
        {
            std::lock_guard<std::mutex> lock(st.mtx);
            if (st.is_working) return;
            path_a = st.file_a;
        }
        try {
            std::string loaded = wrap_number(load_from_file(path_a));
            std::lock_guard<std::mutex> lock(st.mtx);
            st.input_a       = loaded;
            st.cache_a_valid = false;
            st.status_msg    = "A загружено из " + path_a;
            st.result_stale  = true;
        } catch (const std::exception &ex) {
            std::lock_guard<std::mutex> lock(st.mtx);
            st.status_msg = std::string("Ошибка: ") + ex.what();
        }
    }, SmallAnimatedButtonOption(Color::Green));

    // Кнопка: загрузить B из файла
    auto btn_restore_b = Button(" Загрузить B ", [&] {
        std::string path_b;
        {
            std::lock_guard<std::mutex> lock(st.mtx);
            if (st.is_working) return;
            path_b = st.file_b;
        }
        try {
            std::string loaded = wrap_number(load_from_file(path_b));
            std::lock_guard<std::mutex> lock(st.mtx);
            st.input_b       = loaded;
            st.cache_b_valid = false;
            st.status_msg    = "B загружено из " + path_b;
            st.result_stale  = true;
        } catch (const std::exception &ex) {
            std::lock_guard<std::mutex> lock(st.mtx);
            st.status_msg = std::string("Ошибка: ") + ex.what();
        }
    }, SmallAnimatedButtonOption(Color::Green));

    auto btn_execute = Button("  > Выполнить  ", [&] {
        {
            std::lock_guard<std::mutex> lock(st.mtx);
            if (st.is_working) return;
            st.is_working   = true;
            st.status_msg   = "Выполняется...";
            st.spinner_idx  = 0;
            st.spinner_last = Clock::now();
        }
        start_spinner();
        std::thread([&st, &screen]() {
            do_execute(st);
            // обновляем рендер после получения результата
            screen.PostEvent(Event::Custom);
        }).detach();
    }, SmallAnimatedButtonOption(Color::Blue));

    auto btn_quit = Button("  Выход  ", screen.ExitLoopClosure(), SmallAnimatedButtonOption(Color::Red));

    // Контейнер всех компонентов
    auto all = Container::Vertical({
        Container::Horizontal({input_fa, input_fb, input_gb}),
        Container::Horizontal({input_a_tracked, input_b_tracked}),
        Container::Horizontal({btn_gen_a, btn_gen_b, btn_gen_ab,
                               btn_restore_a, btn_restore_b}),
        dropdown,
        Container::Horizontal({target_radio, input_exp_tracked}),
        Container::Horizontal({btn_execute, btn_quit}),
        result_input,
    });

    // Основной рендерер
    // Работает почти как реакт - все интерактивные элементы встраиваются в страницу
    // При изменении любого элемента, который влияет на отображение (all), автоматически рендер
    auto renderer = Renderer(all, [&]() -> Element {

        // Снимок состояния для текущего рендера
        std::string status_msg;
        bool        result_stale = false;
        bool        is_working   = false;
        int         spinner_idx  = 0;
        std::string result_text;
        std::string input_a_val;
        std::string input_b_val;
        bool        show_con;
        int  con_ra, con_rb, con_rs;
        bool con_ok;
        double t_parse_a, t_parse_b, t_op, t_to_dec;
        int selected_op_local;
        int target_ab_local;
        {
            std::lock_guard<std::mutex> lock(st.mtx);
            status_msg        = st.status_msg;
            result_stale      = st.result_stale;
            is_working        = st.is_working;
            spinner_idx       = st.spinner_idx;
            result_text       = st.result_text;
            input_a_val       = st.input_a;
            input_b_val       = st.input_b;
            show_con          = st.show_con;
            con_ra            = st.con_ra;
            con_rb            = st.con_rb;
            con_rs            = st.con_rs;
            con_ok            = st.con_ok;
            t_parse_a         = st.t_parse_a;
            t_parse_b         = st.t_parse_b;
            t_op              = st.t_op;
            t_to_dec          = st.t_to_dec;
            selected_op_local = st.selected_op;
            target_ab_local   = st.target_ab;

            result_display_text = result_text.empty()
                ? std::string("(нет результата)")
                : wrap_number(result_text, 80);
        }

        // Отслеживаем изменения для пометки устаревания
        if (selected_op_local != prev_selected_op) {
            prev_selected_op = selected_op_local;
            result_stale     = true;
            std::lock_guard<std::mutex> lock(st.mtx);
            st.result_stale  = true;
        }
        if (target_ab_local != prev_target_ab) {
            prev_target_ab  = target_ab_local;
            result_stale    = true;
            std::lock_guard<std::mutex> lock(st.mtx);
            st.result_stale = true;
        }

        // Индикатор актуальности результата / работы
        Element stale_indicator;
        if (is_working) {
            stale_indicator = hbox({
                text(" * Выполняется "),
                spinner(5, static_cast<size_t>(spinner_idx))
            }) | color(Color::White) | bold;
        } else if (result_stale) {
            stale_indicator = text(" ! Результат устарел") | color(Color::Yellow);
        } else if (!result_text.empty()) {
            stale_indicator = text(" * Результат актуален") | color(Color::Green);
        } else {
            stale_indicator = text(" - Нет результата") | color(Color::GrayDark);
        }

        // Количество цифр для подписей
        size_t digits_a   = count_digits(input_a_val);
        size_t digits_b   = count_digits(input_b_val);
        size_t digits_res = count_digits(result_text);

        // Блок путей к входным файлам
        auto file_row = hbox({
            text("Файл A: ") | color(Color::GrayLight),
            input_fa->Render() | flex,
            text("  Файл B: ") | color(Color::GrayLight),
            input_fb->Render() | flex,
        }) | border | notflex;

        // Блок параметров генерации и выходного файла
        auto params_row = hbox({
            text("Кол-во байт для генерации: ") | color(Color::GrayLight),
            input_gb->Render() | size(WIDTH, EQUAL, 8) | notflex,
            text("  Файл результата: ") | color(Color::GrayLight),
            input_out->Render() | flex,
        }) | border | notflex;

        // Блоки чисел A и B (мультистрочные, со скроллом)
        auto num_box = [](const std::string &label, size_t digits, Component inp) {
            return window(
                text(" " + label + " (" + std::to_string(digits) + " цифр) "),
                inp->Render() | vscroll_indicator | hscroll_indicator | frame |
                size(HEIGHT, LESS_THAN, 10)
            );
        };

        auto numbers_row = hbox({
            num_box("Число A", digits_a, input_a_tracked) | flex,
            text("  "),
            num_box("Число B", digits_b, input_b_tracked) | flex,
        }) | flex;

        // Кнопки генерации и загрузки
        auto btn_row = hbox({
            btn_gen_a->Render(),
            text(" "),
            btn_restore_a->Render(),
            text("    "),
            btn_gen_b->Render(),
            text(" "),
            btn_restore_b->Render(),
            text("    "),
            btn_gen_ab->Render(),
        }) | notflex;

        // Блок выбора операции
        bool show_target = (selected_op_local == 3 || selected_op_local == 4);
        bool show_exp    = (selected_op_local == 3);

        Elements op_elems;
        op_elems.push_back(vbox({
            text("Операция:") | bold,
            dropdown->Render(),
        }));
        op_elems.push_back(text("   "));
        if (show_target) {
            op_elems.push_back(vbox({
                text("Применить к:") | bold,
                target_radio->Render(),
            }));
        }
        op_elems.push_back(text("   "));
        if (show_exp) {
            op_elems.push_back(vbox({
                text("Степень (1-3):") | bold,
                input_exp_tracked->Render() | border | size(WIDTH, EQUAL, 10),
            }));
        }
        Element op_controls = hbox(op_elems) | notflex;

        // Строка выполнения
        auto exec_row = hbox({
            btn_execute->Render(),
            text("  "),
            btn_quit->Render(),
            text("  "),
            stale_indicator,
        }) | notflex;

        // Результат
        auto result_box = window(
            text(" Результат (" + std::to_string(digits_res) + " цифр) "),
            result_input->Render() | flex
        ) | size(HEIGHT, LESS_THAN, 14) | flex;

        // Блок исключения девяток (добавляется в отображение только при сложении)
        Element con_block = text("");
        if (show_con) {
            auto mk_row = [](const std::string &lbl, int val) {
                return hbox({
                    text(lbl) | color(Color::GrayLight),
                    text(std::to_string(val)) | bold,
                });
            };
            // Формируем строку проверки
            Color ck_color;
            std::string check_lbl;
            if (con_ok) {
                ck_color = Color::Green;
                check_lbl = " -> " + std::to_string(con_ra) + " + "
                          + std::to_string(con_rb) + " = "
                          + std::to_string(con_rs) + " (mod 9)  OK";
            } else {
                ck_color = Color::Red;
                check_lbl = " -> ОШИБКА: " + std::to_string(con_ra) + " + "
                          + std::to_string(con_rb) + " != "
                          + std::to_string(con_rs) + " (mod 9)  FAIL";
            }

            con_block = window(text(" Проверка (исключение девяток) "),
                vbox({
                    mk_row("ЦС(A) mod 9 = ", con_ra),
                    mk_row("ЦС(B) mod 9 = ", con_rb),
                    mk_row("ЦС(сумма) mod 9 = ", con_rs),
                    text(check_lbl) | color(ck_color) | bold,
                })
            );
        }

        // Тайминги
        Element timings = hbox({
            timing_row("Парсинг A", t_parse_a),
            timing_row("Парсинг B", t_parse_b),
            timing_row("Операция", t_op),
            timing_row("Конвертация в строку", t_to_dec),
        }) | border | notflex;

        // Строка статуса
        Element status_bar;
        if (!status_msg.empty()) {
            bool is_error = (status_msg.rfind("Ошибка", 0) == 0);
            status_bar = hbox({
                text(" "),
                text(status_msg)
                    | color(is_error ? Color::RedLight : Color::GreenLight)
                    | bold,
                text(" "),
            }) | border;
        } else {
            status_bar = text("");
        }
        status_bar = status_bar | notflex;

        Elements main_elems = {
            text(" Арифметика больших чисел") | bold | color(Color::Cyan) | border,
            file_row,
            params_row,
            numbers_row,
            btn_row,
            separator(),
            op_controls,
            separator(),
            exec_row,
            separator(),
            result_box,
        };

        if(show_con) {
            main_elems.push_back(con_block);
        }

        main_elems.push_back(timings);
        main_elems.push_back(status_bar);
        
        return vbox(std::move(main_elems));
    });

    screen.Loop(renderer);
}

// -----------------------------------------------------------------------
int main() {
    run_ui();
    return 0;
}
