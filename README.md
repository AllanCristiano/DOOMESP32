# Projeto D.O.M. no ESP32

**Disciplina**: Interface Hardware Software (IHS)  
**Aluno**: Allan Cristiano

## Descrição do Projeto

Este projeto tem como objetivo portar o jogo **D.O.M. (Doom On Microcontrollers)** para rodar em um microcontrolador **ESP32** e transmitir as imagens geradas pelo jogo para um dispositivo móvel via uma requisição **HTTP**. O desenvolvimento integra técnicas de interface direta com o hardware, programação em **C**, além de conceitos de depuração de codigo.

## Funcionalidades

- **Portabilidade do Jogo Doom (D.O.M.)** para o ESP32.
- **Transmissão de imagens via HTTP** para dispositivos móveis.
- Implementação de **interface direta com o hardware** do ESP32 (Wi-Fi, GPIOs, etc).
- **Geração de código de máquina (binário)** ao compilar o projeto para o ESP32.

## Requisitos do Projeto

- **Interface direta com o hardware**: Comunicação com Wi-Fi e GPIOs do ESP32.
- **Código de Montagem (Assembly)**: Implementação de pequenas rotinas em Assembly.
- **Geração de código de máquina (binário)**: Criação do binário a partir da compilação do código.
- **Gerenciamento de dispositivo não suportado**: Implementação de drivers customizados, caso necessário.
- **Engenharia reversa de sistema sem código-fonte**: Técnicas aplicadas à depuração do jogo.
- **Depuração e prototipação virtual**: Uso de ferramentas de depuração e simulação do jogo.
- **Planejamento semanal (Kanban)**: Gestão do progresso no GitHub Projects e versionamento no Git.

## Tecnologias Utilizadas

- **ESP32**: Microcontrolador usado para rodar o jogo.
- **C**: Linguagem de programação principal do projeto.
- **Arduino IDE / ESP-IDF**: Ambientes de desenvolvimento.
- **Git**: Controle de versão.
- **HTTP**: Protocolo para transmissão das imagens do jogo para dispositivos móveis.

## Estrutura do Projeto

- **/src**: Diretório com o código fonte em C/C++.
- **/bin**: Arquivos binários gerados após a compilação.
- **/docs**: Documentação e relatórios do projeto.

## Instruções de Compilação

### Pré-requisitos

- **ESP-IDF** ou **Arduino IDE**: Instale e configure o ambiente de desenvolvimento do ESP32.
- **Git**: Para versionamento do código.

### Passos para Compilação

1. Clone este repositório:
   ```bash
   git clone https://github.com/AllanCristiano/Projeto-DOM-ESP32.git
