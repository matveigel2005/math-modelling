import numpy as np # для мат вычислений и тд
import matplotlib.pyplot as plt # для отрисовки данных на холст
import matplotlib.animation as animation # для создания мультиков
from abstract_plotter import AbstractPlotter # класс, от которого наследуемся


class HeatEquationPlotter(AbstractPlotter): # наш визуализатор
    def plot(self): # задаем метод plot
        frames = self.data["data"]
        # список слоев, которые будем анимировать
        if not frames:
            return

        sample = frames[0]["data"]["grid"] # сетка значений из 1-го кадра
        N = len(sample) # размер сетки
        x = np.linspace(0, 2, N) # массив на N точек равномерно от 0 до 2
        y = np.linspace(0, 2, N)
        X, Y = np.meshgrid(x, y) # делаем двумерные матрицы с координатами узлов

        grids = [] # массив для температуры в кадре
        times = [] # массив для соответствующих времен
        for fr in frames: # цикл по слоям
            t = fr["time"]
            times.append(t)
            g = np.array([
                [val if val is not None else np.nan for val in row]
                for row in fr["data"]["grid"]
            ]) # преобразуем двемерный JSON в двумерный NumPy
            grids.append(g)

        vmin = np.nanmin([np.nanmin(g) for g in grids])
        vmax = np.nanmax([np.nanmax(g) for g in grids])
        # минимальная и максимальная температура

        fig, ax = plt.subplots(figsize=(6, 6))
        # фигура на холсте и оси для рисования
        ax.set_xlim(0, 2)
        ax.set_ylim(0, 2)
        # ставим пределы осей 0-2, тк у нас L-область в этом квадрате
        ax.set_aspect('equal')
        # одинаковый масштаб по осям
        ax.set_xlabel('x')
        ax.set_ylabel('y')
        # подписываем оси
        cbar = None # цветовая шкала, пока непонятно

        def animate(i): # функция для отрисовки кадра i
            nonlocal cbar # берём общую шкалу
            ax.clear() # очищаем холст
            cont = ax.contourf(X, Y, grids[i], levels=30, cmap='hot',
            vmin=vmin, vmax=vmax)
            # заполняем контуры по данным кадры
            if cbar is None:
                cbar = plt.colorbar(cont, ax=ax, label='u')
            # задаем шкалу, если её пока нет
            ax.set_xlim(0, 2)
            ax.set_ylim(0, 2)
            ax.set_aspect('equal')
            # опять шкалы и соотношения
            ax.set_title(f'Temperature t = {times[i]:.4f}')
            # заголовок сверху со временем
            ax.set_xlabel('x')
            ax.set_ylabel('y')
            # оси
            return cont,

        ani = animation.FuncAnimation(fig, animate, frames=len(grids),
        interval=200, blit=False) # сделали объект-аниматор
        ani.save(self.output_path, writer='ffmpeg', fps=5)
        # сохранили итог анимации по пути, создавая видеофайл с 5 фпс
        plt.close(fig) # выкинули холст, почистили память
