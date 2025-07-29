text
# 🚀 Pico MPU6050 Datalogger – Registrador de Dados de 6 Eixos

![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)
![Python](https://img.shields.io/badge/Python-3776AB?style=for-the-badge&logo=python&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-064F8C?style=for-the-badge&logo=cmake&logoColor=white)
![Raspberry Pi Pico](https://img.shields.io/badge/Raspberry%20Pico-A22846?style=for-the-badge&logo=raspberrypi&logoColor=white)


Um datalogger de 6 eixos completo, baseado no Raspberry Pi Pico, com armazenamento em cartão SD, display OLED e interface de controle física.

---

### 📝 Descrição Breve

Este projeto transforma um Raspberry Pi Pico em um poderoso Datalogger para o sensor MPU6050. Ele é projetado para capturar dados de aceleração e giroscópio em 6 eixos, armazenando-os de forma confiável em um arquivo CSV em um cartão SD. O sistema conta com uma interface de usuário rica, exibida em um display OLED, que permite o monitoramento de status, a visualização de dados em tempo real (numéricos e em gráfico de barras) e o controle total das operações através de botões físicos. O feedback ao usuário é aprimorado com um LED RGB e um buzzer, que fornecem indicações claras sobre o estado do sistema.

---

### ✨ Funcionalidades Principais

-   **✅ Coleta de Dados de 6 Eixos:** Leitura contínua dos dados do acelerômetro (3 eixos) e do giroscópio (3 eixos) do sensor MPU6050.
-   **✅ Armazenamento em Cartão SD:** Salva as amostras coletadas em um arquivo `dados_MPU2.csv`, com cabeçalho e formato adequados para fácil análise.
-   **✅ Interface Visual OLED:** Display com múltiplas telas para monitoramento:
    -   **Tela Principal:** Exibe o status do sistema (Pronto, Gravando, Pausado, Erro), o número de amostras coletadas e mensagens de status.
    -   **Tela de Valores:** Mostra os valores numéricos dos 6 eixos em tempo real.
    -   **Tela de Gráfico:** Apresenta um gráfico de barras horizontais para visualizar a aceleração nos eixos X, Y e Z.
-   **✅ Controle por Botões Físicos:** Três botões dedicados para:
    -   Conectar/Desconectar o cartão SD de forma segura.
    -   Iniciar/Parar a gravação dos dados.
    -   Alternar entre as diferentes telas do display.
-   **✅ Feedback Multimodal:** Utiliza um LED RGB e um buzzer para fornecer feedback claro sobre as operações:
    -   **LED Verde:** Sistema pronto/pausado.
    -   **LED Vermelho:** Gravando dados.
    -   **LED Azul:** Acesso ao cartão SD.
    -   **LED Roxo (piscando):** Erro crítico.
    -   **Buzzer:** Emite beeps distintos para confirmação de ações (iniciar/parar gravação, sistema pronto) e alertas de erro.
-   **✅ Operação Não-Bloqueante:** O sistema de buzzer e a lógica de gravação são projetados para não travar o loop principal, garantindo a coleta de dados consistente.

---

### 🖼 Galeria do Projeto

| Hardware em Operação | Exemplo de Gráfico Gerado |
| :------------------: | :-----------------------: |
| *[INSERIR FOTO DO CIRCUITO AQUI]* | *[INSERIR IMAGEM DO GRÁFICO AQUI]* |
| Visão geral do Datalogger montado. | Gráfico de aceleração gerado a partir dos dados coletados. |

---

### ⚙ Hardware Necessário

| Componente | Quant. | Observações |
| :--- | :---: | :--- |
| Raspberry Pi Pico | 1 | O cérebro do projeto. |
| Sensor MPU6050 | 1 | Medição de aceleração e giroscópio (I2C). |
| Display OLED 128x64 | 1 | Para a interface visual (I2C, SSD1306). |
| Módulo de Cartão MicroSD | 1 | Para armazenamento dos dados (SPI). |
| Botões Momentâneos | 3 | Para controle do usuário. |
| LED RGB (Catodo Comum) | 1 | Para indicação de status por cores. |
| Buzzer Passivo | 1 | Para alertas sonoros. |
| Protoboard e Jumpers | - | Para montagem do circuito. |

---

### 🔌 Conexões e Configuração

#### Pinagem Resumida

**Barramento I2C 0 (Sensor):**
-   `MPU6050 SDA` -> `GPIO 0`
-   `MPU6050 SCL` -> `GPIO 1`

**Barramento I2C 1 (Display):**
-   `OLED SDA` -> `GPIO 14`
-   `OLED SCL` -> `GPIO 15`

**Controles e Alertas:**
-   `Botão Cartão SD` -> `GPIO 5`
-   `Botão Gravação` -> `GPIO 6`
-   `Botão Telas` -> `GPIO 22`
-   `LED Vermelho` -> `GPIO 13`
-   `LED Verde` -> `GPIO 11`
-   `LED Azul` -> `GPIO 12`
-   `Buzzer` -> `GPIO 10 (PWM)`

*(A pinagem do cartão SD é definida no arquivo `hw_config.c`)*

> **⚠ Importante:** Garanta um `GND` comum entre todos os componentes. A maioria dos módulos opera em 3.3V, fornecidos pelo pino `3V3(OUT)` do Pico.

---

### 🚀 Começando

#### Pré-requisitos de Software

-   **SDK:** Raspberry Pi Pico SDK
-   **Linguagem:** C/C++
-   **IDE Recomendada:** VS Code com a extensão "CMake Tools"
-   **Toolchain:** ARM GNU Toolchain
-   **Build System:** CMake

#### Configuração e Compilação

Siga estes passos para configurar, compilar e carregar o firmware no seu Pico.

```bash
# 1. Clone o repositório do projeto
git clone [https://github.com/JonasSouza871/Datalogger_Acelerometro_Embarcatech.git]
cd [Datalogger_Acelerometro_Embarcatech]

# 2. Configure o ambiente de build com CMake
# (Certifique-se de que o PICO_SDK_PATH está definido como variável de ambiente)
mkdir build
cd build
cmake ..

# 3. Compile o projeto (use -j para acelerar)
make -j$(nproc)

# 4. Carregue o firmware
# Pressione e segure o botão BOOTSEL no Pico enquanto o conecta ao USB.
# O Pico será montado como um drive USB (RPI-RP2).
# Copie o arquivo .uf2 gerado para o drive:
cp nome_do_projeto.uf2 /media/user/RPI-RP2
```

#### Analisando os Dados

Após coletar os dados, você pode usar o script Python fornecido para gerar gráficos a partir do arquivo CSV.

```bash
# 1. Navegue até a pasta de scripts de plotagem
cd plotar_graficos

# 2. (Opcional) Instale as dependências, caso ainda não as tenha
pip install matplotlib pandas

# 3. Execute o script de plotagem
# O script irá ler o arquivo 'dados_MPU.csv' e gerar os gráficos
python plot.py
```

---

### 📁 Estrutura do Projeto

```
.
├── Imagens_Graficos/   # Imagens e gráficos gerados a partir dos dados
├── lib/                # Bibliotecas de hardware e de terceiros
│   ├── Display_Bibliotecas/
│   ├── FatFs_SPI/
│   ├── hw_config.c
│   ├── mpu6050.c
│   └── mpu6050.h
├── plotar_graficos/    # Scripts Python para análise e visualização dos dados
│   ├── dados_MPU.csv
│   └── plot.py
├── .gitignore
├── CMakeLists.txt      # Script de build principal do CMake
├── main.c              # Ponto de entrada e lógica principal do Datalogger
└── README.md
```
*(Estrutura baseada na imagem fornecida, simplificada para clareza.)*

---

### 🐛 Solução de Problemas

-   **Sistema trava com LED Roxo piscando:**
    -   Indica um erro crítico na inicialização ou operação. Verifique a conexão do cartão SD. Certifique-se de que ele está formatado corretamente (FAT32) e inserido no módulo.
-   **Cartão SD não é reconhecido:**
    -   Confirme a pinagem do módulo SD (MISO, MOSI, SCK, CS) no arquivo `hw_config.c`.
    -   Verifique se o cartão está bem encaixado e se não está corrompido.
-   **Display OLED não mostra nada:**
    -   Verifique as conexões SDA (GPIO 14) e SCL (GPIO 15).
    -   Confirme se o endereço I2C do display (`0x3C`) está correto.
-   **Botões não respondem:**
    -   Certifique-se de que os botões estão conectados corretamente aos pinos GPIO e ao GND, e que os pull-ups internos estão funcionando.

---

### 👤 Autor

**Jonas Souza Pinto**

-   **E-mail:** `Jonassouza871@hotmail.com`
-   **GitHub:** `Jonassouza871`
