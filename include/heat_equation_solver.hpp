#ifndef INCLUDE_HEAT_EQUATION_SOLVER_HPP_
// Проверяем, был ли уже объявлен указанный макрос

#define INCLUDE_HEAT_EQUATION_SOLVER_HPP_
// Если нет, то поехали его определять

/**
 * @file include/heat_equation_solver.hpp
 * @author Глеб М
 *
 * @brief Класс решателя двумерного уравнения теплопроводности в L-области.
 *
 * Реализует явную разностную схему с параллельными вычислениями
 * через std::async. Содержит описание границ Дирихле и Неймана,
 * а также возможность задать начальное распределение температуры.
 */

#include <vector>
// Нужен для std::vector, где мы будем хранить поле температур
// А конкретно u_, u_next_ -- нынешний слой и будущий

#include <future>
// Средства для ассинхронного вычисления
// std::future -- шаблонный класс для доступа к результату ассинхронных операций
// std::async -- инструмент для постановки ассонхронной задачи.
// Даём функцию и её аргументы -> получаем объект типа std::future
// который обещает вернут выполненную задачу как только, так сразу

#include <thread>
// std::thread::hardware_concurrency() -- функция для возвращения количества
// аппаратных потоков, которые нам доступны для выполнения действий параллельно

#include <functional>
// std::function -- обертка, чтоб передавать функции как объекты без конкретики

#include <nlohmann/json.hpp>
// Заголовочный файл для методов работы с JSON файлыми

#include <abstract_solver.hpp>
// Для доступа к базовому классу AbstractSolver<T>, на который мы будем
// делать надстройки и модификации

namespace mm {
// Открываем пространство имён mm(math-modelling), внутри которого и
// будем сидеть, чтоб не было конфликтов с другими именами и тд

/**
 * @brief Явный решатель уравнения теплопроводности в L-образной области.
 *
 * Класс наследует AbstractSolver<T> и предоставляет конкретную реализацию
 * методов MakeStep() и ExportData(). Расчёт ведётся на равномерной сетке
 * с шагом h = 1/M. Граничные условия смешанные: на одних участках задана
 * температура (Дирихле), на других -- нулевая нормальная производная (Нейман).
 *
 * Параллелизм достигается разбиением строк сетки на блоки и обработкой
 * каждого блока в отдельном потоке через std::async. После вычисления
 * внутренних узлов граничные условия применяются последовательно.
 *
 * @tparam T Тип данных для арифметики (float или double).
 */

template<typename T>  // Обобщающий шаблон(у нас double и float)

class HeatEquationSolver : public AbstractSolver<T> {
// Наш класс HeatEquationSolver наследует классу AbstractSolver<T>, где
// модификатор public нужен, чтоб все публичные методы таковыми и остались

 public:
  /**
   * @brief Конструктор решателя.
   *
   * Создаёт сетку размером (2M+1)x(2M+1), вычисляет шаг h = 1/M,
   * инициализирует начальное температурное поле. Если передан
   * init, он используется для задания начальных значений внутри области.
   * Граничные условия Дирихле имеют приоритет и всегда устанавливаются
   * согласно аналитическим выражениям.
   *
   * @param M Число разбиений на единицу длины (определяет шаг сетки).
   * @param tau Шаг по времени.
   * @param finishTime Момент окончания моделирования.
   * @param exportPeriod Интервал сохранения данных (в единицах времени).
   * @param init Функция начального условия (i,j) -> T. По умолчанию nullptr,
   * что соответствует нулевой температуре внутри области.
   */
  using InitialFunc = std::function<T(size_t, size_t)>;
  // Создаём синоним для std::function<T(size_t, size_t)> для удобства
  // Это будет функция, которой мы задаём стартовое распределение температуры
  // Принимаем индексацию (i, j), возвращаем температуру в точке типа T

  HeatEquationSolver(size_t M, T tau, T finishTime, T exportPeriod,
                     InitialFunc init = nullptr)
  // Поехали делать конструктор класса:
  // M -- число разбиений на ед длины(h = 1/M), tau -- шаг по времени,
  // finishTime -- конец времени моделирования,
  // exportPeriod -- типо частоты кадров, те как часто мы хотим записывать инфу,
  // init -- функция начального распределения, по умолчанию всё =0

      : AbstractSolver<T>(tau, finishTime, exportPeriod),
        M_(M),
        h_(static_cast<T>(1.0) / static_cast<T>(M)),
        u_((2 * M + 1) * (2 * M + 1)),
        u_next_((2 * M + 1) * (2 * M + 1)),
        initial_(init) {
    InitializeArrays();
    }
  // Поинициализировали все объекты и сразу выполнили нулевой шаг

  bool MakeStep() override;
  void ExportData(nlohmann::json* output) override;
  // Переопределяем виртуальные функции базового класса

 private:  // приватные параметры
  size_t M_;
  T h_;
  std::vector<T> u_;
  std::vector<T> u_next_;
  InitialFunc initial_;

  size_t Index(size_t i, size_t j) const {
    return i * (2 * M_ + 1) + j;
  }  // перевод записи в де координаты к работе с одномерными данными

  bool IsInside(size_t i, size_t j) const {
    return !(j > M_ && i > M_);
  }  // Проверка на принадлежность L-области

  bool IsDirichlet(size_t i, size_t j) const;  // граница с Дирихле
  bool IsNeumann(size_t i, size_t j) const;  // граница с Нейманом


  T DirichletValue(size_t i, size_t j) const;  // темп. там, где Дирихле
  void InitializeArrays(); //  функция-стартер
};

template<typename T>  // -||-
// Реализация метода InitializeArrays для нашего класса
void HeatEquationSolver<T>::InitializeArrays() {
  size_t N = 2 * M_ + 1; //  к-во узлов сетки в каждую сторону
  for (size_t i = 0; i < N; ++i) {//  y = i * h
    for (size_t j = 0; j < N; ++j) {//  x = j * h
      if (!IsInside(i, j)) continue;
      T value = 0;
      if (IsDirichlet(i, j)) {
        value = DirichletValue(i, j);
      } else if (initial_) {
        value = initial_(i, j);
      }
      u_[Index(i, j)] = value;
    }
  }
}
// Важно, что Дирихле главнее любого прочего, тк четко задано значение

template<typename T>
bool HeatEquationSolver<T>::IsDirichlet(size_t i, size_t j) const {
  if (i == 2 * M_ && j <= M_) return true;  // (0,2)-(1,2)
  if (j == M_ && i >= M_ && i <= 2 * M_) return true;  // (1,1)-(1,2)
  if (i == 0) return true;  // (0,0)-(2,0)
  if (j == 2 * M_ && i <= M_) return true;  // (2,0)-(2,1)
  return false;
}

template<typename T>
bool HeatEquationSolver<T>::IsNeumann(size_t i, size_t j) const {
  if (j == 0 && i > 0 && i < 2 * M_) return true;  // (0,0+)-(0,2-)
  if (i == M_ && j > M_ && j < 2 * M_) return true;  // (1,1+)-(1,2-)
  return false;
}

template<typename T>
T HeatEquationSolver<T>::DirichletValue(size_t i, size_t j) const {
  T fy = static_cast<T>(i) / static_cast<T>(M_);  // коорд y
  if (i == 2 * M_ && j <= M_) return 2;
  if (j == M_ && i >= M_ && i <= 2 * M_) return 4 - fy;
  if (i == 0) return 1;
  if (j == 2 * M_ && i <= M_) return 1 + fy;
  return 0;
}

/**
 * @brief Выполняет один шаг по времени по явной разностной схеме.
 *
 * Обновляет внутренние узлы сетки по формуле:
 * @f[
 * u_{i,j}^{n+1} = u_{i,j}^n + \frac{\tau}{h^2}
 * \left( u_{i-1,j}^n + u_{i+1,j}^n + u_{i,j-1}^n + u_{i,j+1}^n
 * - 4 u_{i,j}^n \right)
 * @f]
 *
 * Вычисление внутренних узлов распараллелено: строки разбиваются на блоки
 * и обрабатываются в отдельных потоках. После завершения всех потоков
 * применяются граничные условия Дирихле (принудительные значения) и Неймана
 * (приравнивание к соседнему узлу). Затем массивы текущего и следующего
 * слоёв меняются местами.
 *
 * @return Всегда возвращает true.
 */

template<typename T>
bool HeatEquationSolver<T>::MakeStep() {
  T h2 = h_ * h_;
  T tau = this->tau;  // берём из базового класса
  T coeff = tau / h2;
  size_t N = 2 * M_ + 1;

  unsigned int nthreads = std::thread::hardware_concurrency();
  // запрашиваем у системы к-во доступных потоков процессора
  if (nthreads < 1) nthreads = 4;
  // 4 -- значеие по умолчанию
  if (nthreads > N) nthreads = static_cast<unsigned int>(N);
  // не даем создавать больше потоков, чем имеется строк для работы

  std::vector<std::future<void>> futures;
  // вектор для хранения результатов будущих операций
  size_t rows_per_thread = (N + nthreads - 1) / nthreads;
  // сколько строк на поток с округлением вверх

  for (unsigned int t = 0; t < nthreads; ++t) {  // цикл по потокам
    size_t start = t * rows_per_thread;  // начальный индекс строки для потока
    if (start >= N) break;  // дошли до конца, выходим
    size_t end = start + rows_per_thread;  // конечный индекс
    if (end > N) end = N;  // для почледнего потока, чтоб не ушёл за массив

    futures.push_back(std::async(std::launch::async,
      [this, start, end, coeff, N]() {
        for (size_t i = start; i < end; ++i) {
          for (size_t j = 0; j < N; ++j) {
            if (!IsInside(i, j)) continue;
            if (IsDirichlet(i, j) || IsNeumann(i, j)) continue;
            size_t idx = Index(i, j);
            T u_ij = u_[idx];
            u_next_[idx] = u_ij + coeff * (
                u_[Index(i - 1, j)] + u_[Index(i + 1, j)] +
                u_[Index(i, j - 1)] + u_[Index(i, j + 1)] -
                4 * u_ij);  // аппроксимативная формула для lap(u)=du/dt
          }
        }
      }));
    // Запускаем ассинхроную задачу через std::async
    // std::launch::async -- гарантированный немедленный запуск в от дельном
    // потоке
    // Лямбда-выражение, пересчитывающее по формуле внутреннюю температуру
  }

  for (auto& f : futures) f.get();
  // цикл, который ждёт конца всех ассинъронныъ задач в futures и получает
  // их результат

  for (size_t i = 0; i < N; ++i) {
    for (size_t j = 0; j < N; ++j) {
      if (!IsInside(i, j)) continue;
      if (IsDirichlet(i, j)) {
        u_next_[Index(i, j)] = DirichletValue(i, j);
      }
    }
  }
  // проставляем Дирихле, тк он фиксирован навсегда

  for (size_t i = 1; i < N - 1; ++i) {
    if (IsInside(i, 0) && IsNeumann(i, 0))
      u_next_[Index(i, 0)] = u_next_[Index(i, 1)];
  }
  for (size_t j = M_ + 1; j < N - 1; ++j) {
    if (IsInside(M_, j) && IsNeumann(M_, j))
      u_next_[Index(M_, j)] = u_next_[Index(M_ + 1, j)];
  }
  // простейшая аппроксимация Неймана через равенство с соседней ячейкой

  u_.swap(u_next_);
  // получаем новый основной слой

  return true;
}

/**
 * @brief Сохраняет текущее температурное поле в JSON-объект.
 *
 * Создаёт двумерный массив "grid" размером (2M+1)x(2M+1).
 * Для узлов внутри L-области записывается текущая температура,
 * для точек вне области -- null. Результат помещается в выходной JSON
 * по ключу "grid".
 *
 * @param output Указатель на JSON-объект, в который будет записана сетка.
 */

template<typename T>
void HeatEquationSolver<T>::ExportData(nlohmann::json* output) {
  size_t N = 2 * M_ + 1;
  nlohmann::json grid = nlohmann::json::array();
  // создаем JSON-массив с именем grid -- вся наша сетка

  for (size_t i = 0; i < N; ++i) {  // цикл по строкам
    nlohmann::json row = nlohmann::json::array();  // JSON-массив для строк
    for (size_t j = 0; j < N; ++j) {  // цикл по столбцам
      if (IsInside(i, j))
        row.push_back(u_[Index(i, j)]);
      else
        row.push_back(nullptr);  // внешник точки, станут null в JSON
    }
    grid.push_back(row);
  }
  (*output)["grid"] = grid;
  // разыменованный указатель на объект nlohmann::json, образаемся по ключу
  // "grid"(такого нет -- значит создаём сначала) и присваиваем содержимое grid
}

}
// закрываем пространство имён namespace mm

#endif  // Закончили определять макрос INCLUDE_HEAT_EQUATION_SOLVER_HPP_
