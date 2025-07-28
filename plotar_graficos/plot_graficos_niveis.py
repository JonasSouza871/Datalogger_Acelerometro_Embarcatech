import numpy as np
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec

# 1. CONFIGURAÇÕES E LEITURA DO ARQUIVO
arquivo_para_analisar = 'nivel3.csv'

# Leitura do arquivo usando Numpy
try:
    data = np.loadtxt(arquivo_para_analisar, delimiter=',', skiprows=1)
    print(f"Arquivo '{arquivo_para_analisar}' lido com sucesso.")
except FileNotFoundError:
    print(f"\nERRO: Arquivo '{arquivo_para_analisar}' não encontrado.")
    print("Verifique se o script e os arquivos CSV estão na mesma pasta.")
    exit()

# Separa os dados em colunas individuais
amostra = data[:, 0]
acel_x  = data[:, 1]
acel_y  = data[:, 2]
acel_z  = data[:, 3]

# 2. GERAÇÃO DO GRÁFICO COM LAYOUT PERSONALIZADO

# Cria a figura que conterá todos os gráficos
fig = plt.figure(figsize=(15, 16))
fig.suptitle(f'Análise Completa do Acelerômetro motor - {arquivo_para_analisar}', fontsize=18)

# Cria um grid de 5 linhas. Os 3 primeiros gráficos usarão 1 linha cada,
# e o último usará 2 linhas, ficando maior.
gs = GridSpec(5, 1, figure=fig)

# Adiciona os subplots ao grid
ax1 = fig.add_subplot(gs[0, 0]) # Gráfico do topo
ax2 = fig.add_subplot(gs[1, 0], sharex=ax1) # Compartilha o eixo X com o de cima
ax3 = fig.add_subplot(gs[2, 0], sharex=ax1) # Compartilha o eixo X com o de cima
ax4 = fig.add_subplot(gs[3:, 0], sharex=ax1) # Ocupa da linha 3 até o fim

# Remove as etiquetas do eixo X dos gráficos de cima para não poluir
ax1.tick_params(axis='x', labelbottom=False)
ax2.tick_params(axis='x', labelbottom=False)
ax3.tick_params(axis='x', labelbottom=False)

# --- Plotando os dados ---

# Gráfico Acel_X
ax1.plot(amostra, acel_x, color='r')
ax1.set_ylabel('Acel_X ($m/s^2$)')
ax1.grid(True)
ax1.set_title('Eixos Separados') # Título para a seção

# Gráfico Acel_Y
ax2.plot(amostra, acel_y, color='g')
ax2.set_ylabel('Acel_Y ($m/s^2$)')
ax2.grid(True)

# Gráfico Acel_Z
ax3.plot(amostra, acel_z, color='b')
ax3.set_ylabel('Acel_Z ($m/s^2$)')
ax3.grid(True)

# Gráfico Comparativo (maior)
ax4.plot(amostra, acel_x, color='r', label='Acel_X', alpha=0.9)
ax4.plot(amostra, acel_y, color='g', label='Acel_Y', alpha=0.9)
ax4.plot(amostra, acel_z, color='b', label='Acel_Z', alpha=0.9)
ax4.set_ylabel('Aceleração ($m/s^2$)')
ax4.set_xlabel('Número da Amostra')
ax4.legend()
ax4.grid(True)
ax4.set_title('Gráfico Comparativo') # Título para a seção

# Ajusta o layout e exibe o gráfico
plt.tight_layout(rect=[0, 0.03, 1, 0.96])
plt.show()