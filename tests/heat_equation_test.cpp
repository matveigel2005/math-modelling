/**
 * @file tests/heat_equation_test.cpp
 * @author Глеб М
 *
 * @brief Модульные тесты для решателя теплопроводности.
 *
 * Проверяют граничные условия, устойчивость, принцип максимума
 * и работу со случайным начальным распределением.
 */

#include <cmath>
// для стандартных функций

#include <random>
// для теста со случайной температурой

// делаем сетевой тест
#include <thread>
// чтоб делать паузу между запросами. Можно и нет,
// но вроде так получше делать
#include <chrono>
// чтоб задать длительность паузы в миллисекундах
#include <httplib.h>
// для отправки запросов серверу

#include <nlohmann/json.hpp>

#include "heat_equation_solver.hpp"

#include "test_core.hpp"
// создание и проверка тестов

// тест на условия Дирихле на границе
static void TestBoundaryConditions() {
  const size_t M = 10;
  const double h = 1.0 / M;
  const double tau = h * h / 4.0;
  const double finishTime = tau;

  mm::HeatEquationSolver<double> solver(M, tau, finishTime, tau);
  // делаем решалку
  nlohmann::json result;
  bool ok = solver.Solve(&result);  // запускаем решалку
  REQUIRE(ok);  // проверка, чем закончилось

  auto grid = result["data"][0]["data"]["grid"];
  // вытаскиваем первый временной слой
  // и проверяем, что у нас вышло с условиями Дирихле в разных местах
  REQUIRE_CLOSE(grid[0][0].get<double>(), 1.0, 1e-10);
  REQUIRE_CLOSE(grid[0][2 * M].get<double>(), 1.0, 1e-10);
  REQUIRE_CLOSE(grid[2 * M][0].get<double>(), 2.0, 1e-10);
  REQUIRE_CLOSE(grid[2 * M][M].get<double>(), 2.0, 1e-10);

  int i_half = static_cast<int>(0.5 * M + 0.5);
  REQUIRE_CLOSE(grid[i_half][2 * M].get<double>(), 1.5, 1e-10);
}

static void TestStability() {  // отсутствие NaN или Inf
  const size_t M = 8;
  const double h = 1.0 / M;
  const double tau = h * h / 4.0;
  const double finishTime = 0.1;

  mm::HeatEquationSolver<double> solver(M, tau, finishTime, finishTime * 2);
  nlohmann::json result;
  bool ok = solver.Solve(&result);
  REQUIRE(ok);

  for (auto& frame : result["data"]) {  // цикл по слоям
    auto grid = frame["data"]["grid"];  // берем сетку из кадра
    for (auto& row : grid) {  // по строкам
      for (auto& val : row) {  // по элементам строк
        if (!val.is_null()) {  // не null, те внутри сетки
          double v = val.get<double>();
          REQUIRE(!std::isnan(v));  // не Nan
          REQUIRE(!std::isinf(v));  // не Inf
        }
      }
    }
  }
}

static void TestMaximumPrinciple() {  // принцип максимума
  const size_t M = 6;
  const double h = 1.0 / M;
  const double tau = h * h / 4.0;
  const double finishTime = 0.1;

  mm::HeatEquationSolver<double> solver(M, tau, finishTime, finishTime * 2);
  nlohmann::json result;
  bool ok = solver.Solve(&result);
  REQUIRE(ok);

  double max_val = -1e9, min_val = 1e9;
  for (auto& frame : result["data"]) {
    auto grid = frame["data"]["grid"];
    for (auto& row : grid) {
      for (auto& val : row) {
        if (!val.is_null()) {
          double v = val.get<double>();
          if (v > max_val) max_val = v;
          if (v < min_val) min_val = v;
        }
      }
    }
  }
  REQUIRE(max_val <= 4.0 + 1e-10);
  REQUIRE(min_val >= 0.0 - 1e-10);
}

static void TestRandomInitial() {  // тест со случйной температурой
  const size_t M = 8;
  const double h = 1.0 / M;
  const double tau = h * h / 4.0;
  const double finishTime = 0.05;

  std::mt19937 rng(19042005);
  // генератор псевдослучайных чисел, который каждый раз будет выдавать
  // одни и те же числа, чтоб можно было понять, а с чего у нас что-то упало
  std::uniform_real_distribution<double> dist(-5.0, 5.0);
  // равномерная генерация от -5.0 до 5.0

  auto init_func = [&](size_t, size_t) {
    return dist(rng);
  };  // лямбда-функция для удобства

  mm::HeatEquationSolver<double> solver(M, tau, finishTime,
  finishTime, init_func);  // решалка со случайной стартовой температурой
  nlohmann::json result;
  bool ok = solver.Solve(&result);
  REQUIRE(ok);

  for (auto& frame : result["data"]) {
    auto grid = frame["data"]["grid"];
    for (auto& row : grid) {
      for (auto& val : row) {
        if (!val.is_null()) {
          double v = val.get<double>();
          REQUIRE(!std::isnan(v));
          REQUIRE(!std::isinf(v));
          REQUIRE(std::abs(v) < 1e6);
        }
      }
    }
  }
}

static void TestHttpWorkflow() {  // добавляем сетевой тест
    using namespace httplib;  // для удобства
    Client cli("http://localhost:8080");
    // создаем объект HTTP-клиента, который будет обращаться
    // к серверу по адресу localhost на порт 8080

    // отправляем POST-запрос на маршрут /CheckTaskStatu с пустым
    // телом и HTTP-заголовком application/json
    auto chk = cli.Post("/CheckTaskStatus", "{}", "application/json");
    if (!chk) {  //проверяем, есть ли ответ от сервера
        std::cerr << "Skipping HTTP test (server not reachable)\n";
        return;
    }

    nlohmann::json req;  //JSON-объект для тела запроса
    // дальше остальные параметры
    req["M"] = 10;
    req["tau"] = 0.01;
    req["finishTime"] = 0.02;
    req["exportPeriod"] = 0.02;

    // тут вроде ясно, смысл тот же, но телоне пустое, а с нашими
    // данными, которые писали выше
    auto post = cli.Post("/HeatEquation", req.dump(), "application/json");
    REQUIRE(post != nullptr);  // есть ответ от сервера
    REQUIRE(post->status == 200);  // проверяем, что задача принята

    auto json = nlohmann::json::parse(post->body);
    // собираем ответ в json-ку
    REQUIRE(json.contains("id"));
    int task_id = json["id"];  // получили id, если есть

    for (int i = 0; i < 100; ++i) {
        auto st = cli.Post("/CheckTaskStatus",
                           nlohmann::json{{"id", task_id}}.dump(),
                           "application/json");
        // отправляем запрос для проверки статуса задачи с нашей id
        REQUIRE(st != nullptr);  // проверяем, что ответ есть
        auto st_json = nlohmann::json::parse(st->body);
        // собираем ответ в формат json
        if (st_json.value("status", "") == "finished") break;
        // если задачу завершили, то ок, на выход
        // подождём, чтоб не бить по серверу постоянными запросами
        // и не промотать быстро весь наш цикл на проверку
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // опять качаем данные
    auto down = cli.Post("/DownloadTaskData",
                         nlohmann::json{{"id", task_id}}.dump(),
                         "application/json");
    REQUIRE(down != nullptr);
    auto result = nlohmann::json::parse(down->body);
    //и дальшепроверяем, что реально получаем то, что надо по уму
    REQUIRE(result.contains("data"));
    REQUIRE(!result["data"].empty());
    REQUIRE(result["data"][0]["data"].contains("grid"));
}


/**
 * @brief Главная тестовая функция.
 *
 * Создаёт набор тестов "Heat Equation" и последовательно
 * запускает все проверки.
 */

void TestHeatEquation() {  // главная тестовая функция
  TestSuite suite("Heat Equation");  // объект suite класса TestSuite,
  // который вкурсе, как наш набор тестов называется
  RUN_TEST(suite, TestBoundaryConditions);
  RUN_TEST(suite, TestStability);
  RUN_TEST(suite, TestMaximumPrinciple);
  RUN_TEST(suite, TestRandomInitial);
  RUN_TEST(suite, TestHttpWorkflow);
}
