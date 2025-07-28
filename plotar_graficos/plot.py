import numpy as np
import matplotlib.pyplot as plt

# --- CONFIGURAÇÕES ---
arquivo_para_analisar = 'dados_MPU.csv'
taxa_de_amostragem = 5.0  # Frequência de 5 Hz
# ---------------------

# Leitura do arquivo com Numpy
dt = 1.0 / taxa_de_amostragem
try:
    data = np.loadtxt(arquivo_para_analisar, delimiter=',', skiprows=1)
except FileNotFoundError:
    print(f"ERRO: Arquivo '{arquivo_para_analisar}' não encontrado.")
    exit()

# Extrai as colunas do array numpy
amostra = data[:, 0]
acel_x  = data[:, 1]
acel_y  = data[:, 2]
acel_z  = data[:, 3]
giro_x  = data[:, 4]
giro_y  = data[:, 5]
giro_z  = data[:, 6]

# Converte giroscópio para graus (ângulo acumulado)
angulo_x = (giro_x * dt).cumsum()
angulo_y = (giro_y * dt).cumsum()
angulo_z = (giro_z * dt).cumsum()

# --- Figura 1: Acelerômetro (4 gráficos) ---
titulo_fig1 = f'Dados MPU - Aceleração Coletada ({arquivo_para_analisar})'

fig1, axs1 = plt.subplots(4, 1, figsize=(15, 14), sharex=True)
fig1.suptitle(titulo_fig1, fontsize=16)

# Gráfico Acel_X
axs1[0].plot(amostra, acel_x, color='r')
axs1[0].set_ylabel('Acel_X ($m/s^2$)')
axs1[0].grid(True)

# Gráfico Acel_Y
axs1[1].plot(amostra, acel_y, color='g')
axs1[1].set_ylabel('Acel_Y ($m/s^2$)')
axs1[1].grid(True)

# Gráfico Acel_Z
axs1[2].plot(amostra, acel_z, color='b')
axs1[2].set_ylabel('Acel_Z ($m/s^2$)')
axs1[2].grid(True)

# Gráfico Comparativo Acel
axs1[3].plot(amostra, acel_x, color='r', label='Acel_X')
axs1[3].plot(amostra, acel_y, color='g', label='Acel_Y')
axs1[3].plot(amostra, acel_z, color='b', label='Acel_Z')
axs1[3].set_ylabel('Comparativo ($m/s^2$)')
axs1[3].set_xlabel('Número da Amostra')
axs1[3].legend()
axs1[3].grid(True)


# --- Figura 2: Giroscópio (4 gráficos) ---
titulo_fig2 = f'Dados MPU - Giroscópio (Ângulo Acumulado) ({arquivo_para_analisar})'

fig2, axs2 = plt.subplots(4, 1, figsize=(15, 14), sharex=True)
fig2.suptitle(titulo_fig2, fontsize=16)

# Gráfico Angulo_X
axs2[0].plot(amostra, angulo_x, color='r')
axs2[0].set_ylabel('Ângulo X (Graus)')
axs2[0].grid(True)

# Gráfico Angulo_Y
axs2[1].plot(amostra, angulo_y, color='g')
axs2[1].set_ylabel('Ângulo Y (Graus)')
axs2[1].grid(True)

# Gráfico Angulo_Z
axs2[2].plot(amostra, angulo_z, color='b')
axs2[2].set_ylabel('Ângulo Z (Graus)')
axs2[2].grid(True)

# Gráfico Comparativo Giro
axs2[3].plot(amostra, angulo_x, color='r', label='Ângulo X')
axs2[3].plot(amostra, angulo_y, color='g', label='Ângulo Y')
axs2[3].plot(amostra, angulo_z, color='b', label='Ângulo Z')
axs2[3].set_ylabel('Comparativo (Graus)')
axs2[3].set_xlabel('Número da Amostra')
axs2[3].legend()
axs2[3].grid(True)


# Exibe os gráficos na tela
plt.tight_layout(rect=[0, 0, 1, 0.97])
plt.show()