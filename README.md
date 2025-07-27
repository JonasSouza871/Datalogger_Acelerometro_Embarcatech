# FatFS SPI Example - Raspberry Pi Pico

Este projeto demonstra como usar um cartão SD com sistema de arquivos FAT (FatFS) em um **Raspberry Pi Pico**, realizando operações de leitura, escrita e listagem de arquivos via comandos no terminal.  
O código também inclui uma rotina de aquisição de dados do ADC (canal 0, GPIO 26) e registro dos dados em arquivo.

## Funcionalidades

- **Formatação**, montagem e desmontagem do cartão SD (com FatFS).
- **Listagem** de arquivos e diretórios (comando `ls`).
- **Leitura** do conteúdo de arquivos (comando `cat`).
- **Aquisição de dados** do ADC e salvamento automático em arquivo (`adc_data2.txt`).
- **Exibição de espaço livre** no cartão SD.
- **Configuração de data/hora** do RTC integrado.
- **Atalhos de teclado** para comandos rápidos no terminal.

## Hardware Necessário

- Cartão microSD (com adaptador para SPI, está no KIT básico do Embarcatech)

## Comandos Disponíveis



| Comando                               | Descrição                                              | 
|---------------------------------------|--------------------------------------------------------|
| `mount`                               | Monta o cartão SD                                      | 
| `unmount`                             | Desmonta o cartão SD                                   | 
| `format`                              | Formata o cartão SD                                    | 
| `ls`                                  | Lista arquivos/diretórios do cartão SD                 |
| `cat <arquivo>`                       | Mostra o conteúdo de um arquivo                        | 
| `getfree`                             | Exibe o espaço livre no cartão SD                      |
| `setrtc <DD> <MM> <YY> <hh> <mm> <ss>`| Ajusta a data/hora do RTC interno do Pico              |
| `help`                                | Mostra todos os comandos disponíveis                   |

**Atalhos de teclado no terminal (pressione apenas a tecla):**

| Tecla  | Função                                                           |
|--------|------------------------------------------------------------------|
| `a`    | Monta o cartão SD (`mount`)                                      |
| `b`    | Desmonta o cartão SD (`unmount`)                                 |
| `c`    | Lista os arquivos do cartão SD (`ls`)                            |
| `d`    | Lê e exibe o conteúdo do arquivo `adc_data2.txt`                 |
| `e`    | Mostra o espaço livre no cartão SD (`getfree`)                   |
| `f`    | Captura 128 amostras do ADC e salva no arquivo `adc_data2.txt`   |
| `h`    | Exibe os comandos disponíveis (`help`)                           |


## Gera gráficos

Um arquivo em python é disponibilizado para geração dos gráficos. 
A biblioteca matplotlib é usada para produção gráfica.
A IDE do Thonny pode ser utilizada.


---