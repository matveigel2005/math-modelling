#!/usr/bin/env python3

# это исполняемый питонячий скрипт, который мы хотим
# запускать через интерпритатор python3

import http.client
# для HTTP-запросов
import json
# для работы с JSON
import time
# для пауз
import subprocess
# для запуска рисовалки и сервера
import sys
# для sys.executable и sys.platform
import os
# для путей и удаления файлов

# В функциях первыми строками даны документативные комменты
# в тройных кавычках для создания внешней документации проги
# По идее вообще не надо, но в сишных файлах такое делали,
# так что идем до конца

# вспомогательная диалоговая функция
def ask(prompt, default):
    """Задать вопрос с значением по умолчанию."""
    user = input(f"{prompt} (по умолчанию {default}): ").strip()
    return user if user != "" else default

# проверяем доступность сервера
def check_server(host, port):
    """Проверить, отвечает ли сервер."""
    try:
        # делаем объект HTTP-соединения с сервером
        conn = http.client.HTTPConnection(host, port, timeout=2)
        # отправляем запрос-проверку
        conn.request("POST", "/CheckTaskStatus", "{}",
                     {"Content-Type": "application/json"})
        resp = conn.getresponse()
        return resp.status == 200
    except Exception:
        return False
    finally:
        # в любом случае вырубаем соединение
        conn.close()

# универсальная обращалка к серверу
def send_request(host, port, path, body):
    """Отправить POST-запрос и вернуть JSON-ответ."""
    conn = http.client.HTTPConnection(host, port, timeout=10)
    headers = {"Content-Type": "application/json"}
    conn.request("POST", path, json.dumps(body), headers)
    resp = conn.getresponse()
    # получили ответ и привели в JSON-строку
    data = resp.read().decode()
    conn.close()
    return json.loads(data)


def main():
    print("=== Интерактивный генератор анимации теплопроводности ===")

    # параметры с умолчаниями
    M = int(ask("Число разбиений M", "30"))
    tau = float(ask("Шаг по времени tau", "0.00025"))
    finish_time = float(ask("Конечное время finishTime", "0.05"))
    export_period = float(ask("Период сохранения exportPeriod", "0.005"))
    initial = ask("Начальное условие (zero/random/sin)", "zero")
    output_file = ask("Имя выходного видеофайла", "animation.mp4")

    host = "localhost"
    port = 8080

    # запуск сервера, если он ещё не работает
    server_process = None
    if not check_server(host, port):
        print("Запускаем сервер...")
        # формируем путь к исполняемому файлу сервера
        server_exec = os.path.join("build", "math_modelling_server")
        # отдельно доабатываем для винды
        if sys.platform == "win32":
            server_exec += ".exe"
        # запускаем серверный исполняющий файл, перенаправляя
        # все выводы и ошибки в никуда, чтоб не захламлять вывод
        server_process = subprocess.Popen([server_exec],
                                          stdout=subprocess.DEVNULL,
                                          stderr=subprocess.DEVNULL)
        time.sleep(2)   # даём время на старт
    else:
        print("Сервер уже запущен.")

    # отправляем задачу
    print("Отправляем задачу...")
    req_body = {
        "M": M,
        "tau": tau,
        "finishTime": finish_time,
        "exportPeriod": export_period,
        "initial": initial
    }
    resp = send_request(host, port, "/HeatEquation", req_body)
    task_id = resp["id"]
    print(f"Задача зарегистрирована, ID = {task_id}")

    # ожидаем завершения
    print("Ожидаем завершения расчёта...")
    while True:
        status_resp = send_request(host, port, "/CheckTaskStatus",
                                   {"id": task_id})
        if status_resp.get("status") == "finished":
            print("Расчёт завершён.")
            break
        time.sleep(0.5)

    # скачиваем данные
    print("Скачиваем данные...")
    result = send_request(host, port, "/DownloadTaskData",
                          {"id": task_id})
    # сохраняем во временный файл
    tmp_json = "temp_heat_result.json"
    with open(tmp_json, "w") as f:
        json.dump(result, f)

    print("Строим анимацию...")
    # формируем списки строк для запуска рисовалки
    plotter_cmd = [
        sys.executable,
        os.path.join("python", "plot.py"),
        "heat_equation",
        tmp_json,
        output_file
    ]
    subprocess.run(plotter_cmd, check=True)
    # выполняем внешний процее анимации

    # удаляем временный файл
    os.remove(tmp_json)

    # останавливаем сервер, если запускали сами
    if server_process is not None:
        print("Останавливаем сервер...")
        try:
            send_request(host, port, "/stop", {})
        except Exception:
            pass
        server_process.terminate()
        server_process.wait()

    print(f"Готово! Видео сохранено как {output_file}")


if __name__ == "__main__":
    main()
