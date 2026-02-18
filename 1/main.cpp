#include <iostream>
#include <iomanip>
#include <cmath>

//  Уравнение: e^x = 1/sqrt(x)
//  f(x) = e^x - 1/sqrt(x) = 0
//
//  Для метода простой итерации:
//    e^x = x^{-1/2}  =>  e^{-x} = x^{1/2}  =>  x = e^{-2x}
//    phi(x) = e^{-2x},  |g'(x)| = |-2e^{-2x}| < 1 => сходится к корню

double f(double x) {
    return std::exp(x) - 1.0 / std::sqrt(x);
}

// Итерационная функция phi(x) = e^{-2x}
double phi(double x) {
    return std::exp(-2.0 * x);
}

//  Метод бисекции
//  Возвращает найденный приближенный корень, записывает число итераций
double bisection(double a, double b, double eps, int& iters) {
    iters = 0;
    double с = a;
    // До тех пор, пока точность недостаточна
    while (std::abs(b - a) > eps) {
        ++iters;
        с = (a + b) / 2.0;

        // Если знак функции меняется слева от центра
        if (f(a) * f(с) < 0)
            // Сдвигаем правую границу
            b = с;
        else
            // Сдвигаем левую границу
            a = с;
    }
    return (a + b) / 2.0;
}

//  Метод простой итерации
//  x_{n+1} = g(x_n)
//  Возвращает найденный приближенный корень, записывает число итераций
double simpleIteration(double x0, double eps, int& iters) {
    iters = 0;
    double x = x0;
    while (true) {
        ++iters;
        double xNext = phi(x);
        if (std::abs(xNext - x) < eps) {
            return xNext;
        }
        x = xNext;
    }
}

int main() {
    const double A = 0.3;
    const double B = 0.8;
    const double EPS1 = 1e-2;   // первая точность
    const double EPS2 = 1e-4;   // вторая точность

    std::cout << std::fixed << std::setprecision(8);

    // 1. Метод бисекции, eps = 1e-2
    int iterBisect1 = 0;
    double rootBisect1 = bisection(A, B, EPS1, iterBisect1);
    std::cout << "=== Метод бисекции (eps = 1e-2) ===\n";
    std::cout << "Корень:          x = " << rootBisect1 << "\n";
    std::cout << "              f(x) = " << f(rootBisect1) << "\n";
    std::cout << "Число итераций:  " << iterBisect1 << "\n\n";

    // 2. Метод простой итерации, eps = 1e-2
    double x0_iter = (A + B) / 2.0;
    int iterSimple1 = 0;
    double rootSimple1 = simpleIteration(x0_iter, EPS1, iterSimple1);
    std::cout << "=== Метод простой итерации (eps = 1e-2), x0 = " << x0_iter << " ===\n";
    std::cout << "Корень:          x = " << rootSimple1 << "\n";
    std::cout << "              f(x) = " << f(rootSimple1) << "\n";
    std::cout << "Число итераций:  " << iterSimple1 << "\n\n";

    // 3. Уточнение методом простой итерации, eps = 1e-4
    std::cout << "=== Уточнение методом простой итерации (eps = 1e-4) ===\n";
    std::cout << "Начальное приближение (x0) из метода простой итерации: x0 = " << rootBisect1 << "\n\n";

    int iterSimple2 = 0;
    double rootSimple2 = simpleIteration(rootBisect1, EPS2, iterSimple2);
    std::cout << "Корень:          x = " << rootSimple2 << "\n";
    std::cout << "              f(x) = " << f(rootSimple2) << "\n";
    std::cout << "Число итераций:  " << iterSimple2 << "\n\n";

    // 4. Сравнение
    std::cout << "=== Сравнение методов ===\n";
    std::cout << std::left
              << std::setw(45) << "Метод"
              << std::setw(18) << "Корень"
              << std::setw(20) << "Итерации"
              << "Точность\n";
    std::cout << std::string(80, '-') << "\n";
    std::cout << std::setw(47) << "Бисекция"
              << std::setw(16) << rootBisect1
              << std::setw(10) << iterBisect1
              << "1e-2\n";
    std::cout << std::setw(61) << "Простая итерация (входное x0 = mid)"
              << std::setw(16) << rootSimple1
              << std::setw(10) << iterSimple1
              << "1e-2\n";
    std::cout << std::setw(64) << "Простая итерация (x0 из бисекции)"
              << std::setw(16) << rootSimple2
              << std::setw(10) << iterSimple2
              << "1e-4\n";

    return 0;
}
