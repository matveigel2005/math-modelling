#include <memory>
// для точной корректной работы std::make_shared

#include <random>
// для генерации случайных чисел в начальном распределении

#include <string>
// для определения типа начального условия

#include <cmath>
// математ функции для начального условия

#include <nlohmann/json.hpp>
// работа с JSON-файлами

#include "abstract_solver_wrapper.hpp"
// Для использования классов-обёрток FloatAbstractSolverWrapper
// и DoubleAbstractSolverWrapper(мы хотим как раз этот, тк для задачки
// double будет лучше)

#include "heat_equation_solver.hpp"
// для работы с HeatEquationSolver<T>

#include "tasks_queue.hpp"
// для работы с очередью и получения taskId

#include "methods.hpp"  // БББ и отлов изменений синтаксиса

//extern mm::TasksQueue tasksQueue;
// extern -- есть такая переменная, но в другом файле
// mm::TasksQueue -- пространство имён и реализующийся класс
// tasksQueue -- имя той самой внешней переменной

// Вспомогательная функция, задающаяфункцию для определения нач темп
static mm::HeatEquationSolver<double>::InitialFunc
MakeInitial(const std::string& type, size_t M = 0) {
    if (type == "zero" || type.empty()) {
        return nullptr;  // значение по умолчанию -- нуль
    } else if (type == "random") {
        // std::random_device -- источник случайных чисел
        // {} дают временный объект класса, () возвращает случайное число
        // Его мы спользуем как определяющее для нашего генератора
        // случайных чисел std::mt19937. Наконец, std::make_shared<std::mt19937>
        // std::make_shared<T>(args) -- создает объект типа Т и возвращает
        // умный указатель std::shared_ptr<T>
        auto rng = std::make_shared<std::mt19937>(std::random_device{}());
        // std::uniform_real_distribution<double> -- непрерывное равномерное
        // распределение на отрезке
        auto dist =
        std::make_shared<std::uniform_real_distribution<double>>(-5.0, 5.0);
        // возвращаем лямбда-функцию, забирающую эти мные указатели
        return [rng, dist](size_t, size_t) { return (*dist)(*rng); };
        // Тут пришлось использовать сложные указатели, тк наши генераторы
        // должны жить и после функции, чтоб лямбла-функцию корректно отработала
    } else if (type == "sin") {
        if (M == 0) {
            // без знания размеров сетки вернём нули
            return nullptr;
        }
        return [M](size_t i, size_t j) {
            double x = static_cast<double>(j) / static_cast<double>(M);
            double y = static_cast<double>(i) / static_cast<double>(M);
            const double pi = std::acos(-1.0);
            return std::sin(pi * x) * std::sin(pi * y);
        };  // иначе нормальную функцию с синусами
    }
    // неизвестный тип -- по умолчанию
    return nullptr;
}

/**
 * @brief Серверный метод для задачи теплопроводности.
 * @param input Входной JSON с параметрами M, tau, finishTime, exportPeriod
 * и опционально initial.
 * @param output Выходной JSON, в который записывается id задачи.
 * @param tasksQueue Ссылка на очередь задач для добавления расчета.
 * @return Идентификатор созданной задачи.
 */

// Объявление нашей функции из пространства имён mm
int mm::HeatEquationMethod(const nlohmann::json& input,
                           nlohmann::json* output,
                           mm::TasksQueue& tasksQueue) {
  // at -- обращение к JSON файлу и поиск данных по ключу
  // Иначе std::out_of_range, если такого нет
  size_t M = input.at("M").get<size_t>();
  double tau = input.at("tau").get<double>();
  double finishTime = input.at("finishTime").get<double>();
  double exportPeriod = input.at("exportPeriod").get<double>();
  // вытащили из входной JSON-ки все данные

  // Читаем начальный тип (если нет -- "zero" по умолчанию)
  std::string initType = input.value("initial", "zero");
  auto initFunc = MakeInitial(initType, M);  // получаем нач фкнкцию

  auto* solver = new mm::HeatEquationSolver<double>(M, tau,
  finishTime, exportPeriod, initFunc);  // создаём объект-решалку

  auto* wrapper = new mm::DoubleAbstractSolverWrapper(solver);
  // обертка для работы очереди над решалкой

  int taskId = tasksQueue.AddTask(wrapper);
  // ставим обертку в очередь, получаем taskId

  (*output)["id"] = taskId;
  // записываем id нашей задачи в выходной JSON по ключу "id"

  return taskId;
}
