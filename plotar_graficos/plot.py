import pandas as pd
import matplotlib.pyplot as plt

# 1. CONFIGURAÇÕES E LEITURA DO ARQUIVO
# --- Altere o nome do arquivo aqui para analisar um ensaio diferente ---
arquivo_para_analisar = 'nivel0.csv'
# ----------------------------------------------------------------------

# Leitura do arquivo para um DataFrame
try:
    df = pd.read_csv(arquivo_para_analisar)
    print(f"Arquivo '{arquivo_para_analisar}' lido com sucesso.")
except FileNotFoundError:
    print(f"\nERRO: Arquivo '{arquivo_para_analisar}' não encontrado.")
    print("Verifique se o script e os arquivos CSV estão na mesma pasta.")
    exit() # Encerra o script se o arquivo não for encontrado

# 2. GERAÇÃO DOS GRÁFICOS

# --- Figura 1: Gráficos Separados do Acelerômetro ---
fig1, axs1 = plt.subplots(3, 1, figsize=(15, 10), sharex=True)
fig1.canvas.manager.set_window_title('Análise do Acelerômetro (Eixos Separados)')
fig1.suptitle(f'Dados do Acelerômetro (Eixos Separados) - {arquivo_para_analisar}', fontsize=16)

# Gráfico Acel_X
axs1[0].plot(df['Amostra'], df['Acel_X'], color='r')
axs1[0].set_ylabel('Acel_X ($m/s^2$)')
axs1[0].grid(True)

# Gráfico Acel_Y
axs1[1].plot(df['Amostra'], df['Acel_Y'], color='g')
axs1[1].set_ylabel('Acel_Y ($m/s^2$)')
axs1[1].grid(True)

# Gráfico Acel_Z
axs1[2].plot(df['Amostra'], df['Acel_Z'], color='b')
axs1[2].set_ylabel('Acel_Z ($m/s^2$)')
axs1[2].set_xlabel('Número da Amostra')
axs1[2].grid(True)


# --- Figura 2: Gráfico Comparativo do Acelerômetro ---
fig2, ax2 = plt.subplots(figsize=(15, 7))
fig2.canvas.manager.set_window_title('Análise do Acelerômetro (Comparativo)')
ax2.set_title(f'Dados do Acelerômetro (Comparativo) - {arquivo_para_analisar}', fontsize=16)

# Plota os três eixos no mesmo gráfico
ax2.plot(df['Amostra'], df['Acel_X'], color='r', label='Acel_X', alpha=0.9)
ax2.plot(df['Amostra'], df['Acel_Y'], color='g', label='Acel_Y', alpha=0.9)
ax2.plot(df['Amostra'], df['Acel_Z'], color='b', label='Acel_Z', alpha=0.9)

ax2.set_xlabel('Número da Amostra')
ax2.set_ylabel('Aceleração ($m/s^2$)')
ax2.legend() # Adiciona a legenda para identificar as linhas
ax2.grid(True)


# Ajusta o layout e exibe os gráficos
plt.tight_layout(rect=[0, 0, 1, 0.97])
plt.show()