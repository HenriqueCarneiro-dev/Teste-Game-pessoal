# VolTerra

Jogo de gerenciamento de uma rede elétrica nacional, feito em C com [raylib](https://www.raylib.com/).

## Visão Geral

Em VolTerra você opera a rede elétrica do Brasil. Cada região do país tem uma bateria que sobe e desce conforme a produção e o consumo de energia. Seu trabalho é manter todas as regiões numa faixa segura enquanto eventos climáticos e picos de demanda tentam desequilibrar o sistema.

Se uma bateria chega ao limite (apagão ou sobrecarga), você precisa vencer um minigame para estabilizar a região. Falhar custa uma vida. A dificuldade cresce a cada fase, com mais regiões, eventos mais frequentes e alertas mais rápidos.

## Objetivo

Sobreviver às **5 fases** sem ficar sem vidas, somando o máximo de pontos possível.

- Cada fase tem **4 dias de 30 segundos**.
- Você começa com **3 vidas**.
- Mantenha todas as baterias entre **30% e 80%**.
- Quem completa as 5 fases **sem perder nenhuma vida** desbloqueia algo especial.

## Como Jogar

### Estados da bateria

| Bateria | Situação |
|---|---|
| 30% a 80% | Estável. Você ganha pontos e o bônus de combo sobe. |
| Acima de 80% | **Alerta de sobrecarga.** Em 110%, um minigame de sobrecarga dispara. |
| Abaixo de 30% | **Alerta de apagão.** Em 0%, um minigame de apagão dispara. |

Um cronômetro no mapa mostra quantos segundos faltam para o estouro. Setas verdes indicam sobra de energia na região e setas vermelhas indicam falta.

### Controles

| Ação | Como fazer |
|---|---|
| Selecionar uma região | Clicar nela no mapa ou na lista à direita |
| Aumentar a produção | `W`, seta para cima, roda do mouse ou botão `+` |
| Diminuir a produção | `S`, seta para baixo, roda do mouse ou botão `-` |
| Voltar a produção a 100% | `R` ou botão Reset |
| Apoiar outra região | Segurar o botão do mouse numa região e arrastar até a outra |
| Pausar | `P` ou `ESC` (a pausa tem o ajuste de volume) |
| Novo jogo no menu | `ENTER` |
| Tela cheia / janela | Botão de engrenagem no canto inferior esquerdo |

A produção de cada região pode ser ajustada de 20% a 150%.

**Apoio entre regiões:** o apoio tira 10% da bateria da origem (que precisa ter pelo menos 25%). A região vizinha recebe 8% e uma região distante recebe 5%. Existe um tempo de espera entre um apoio e outro.

### Regiões por fase

| Fase | Regiões ativas |
|---|---|
| 1 | Sudeste, Nordeste, Centro-Oeste (3) |
| 2 | + Sul (4) |
| 3 a 5 | + Norte (5) |

### Eventos

Eventos climáticos e de demanda alteram a produção e o consumo de regiões específicas por um tempo. Quanto mais avançada a fase, mais eventos podem acontecer ao mesmo tempo.

Chuvas fortes, Seca severa, Onda de calor, Ventos fortes, Frente fria, Jogo decisivo, Manutenção nuclear, Sol forte e Pico industrial.

### Minigames

| Tipo | Minigame | O que fazer |
|---|---|---|
| Apagão | Fios | Arrastar cada fio da esquerda até a ponta da mesma cor |
| Apagão | Disjuntores | Clicar nos disjuntores em ordem de 1 a 6 (errar reinicia a sequência) |
| Apagão | Gerador | Clicar no botão ou apertar `ESPAÇO` rápido até encher 100% |
| Sobrecarga | Turbinas | Clicar nas turbinas para esfriá-las, sem deixar nenhuma chegar a 100% |
| Sobrecarga | Sincronia | Parar o marcador na zona verde (`ESPAÇO` ou botão), acertando 3 vezes |
| Sobrecarga | Alvos | Desligar o disjuntor aceso antes que ele apague, com 8 acertos |

O tempo de cada minigame diminui conforme a fase avança. Vencer estabiliza a região e dá pontos. Falhar custa uma vida.

## Pontuação

| Fonte | Pontos |
|---|---|
| Rede estável (todas as regiões entre 30% e 80%) | `5 × fase × combo` por segundo |
| Minigame vencido | `100 × fase` |
| Fim de cada fase | `500 × fase + 200 × vidas restantes` |

**Combo:** a cada 12 segundos com a rede estável o bônus sobe de x1 até x5. Enquanto alguma região estiver em alerta o combo para de crescer, e cair num minigame zera o bônus de volta para x1.

## Fase Secreta

Quem termina a fase 5 sem perder nenhuma vida pode jogar a **Fase Secreta**, com o mapa dos **27 estados** do Brasil.

- 5 vidas;
- 3 dias de 40 segundos;
- +5000 pontos ao concluir;
- a conquista fica registrada no menu.

## Ranque

Ao fim de cada partida você digita seu nome e a pontuação entra no **Ranque da fase** (top 10 por fase, com uma aba para a fase secreta). Os recordes ficam salvos no arquivo `ranking_fases.txt` e a conquista secreta em `segredo.txt`, ambos criados na pasta do jogo.

## Requisitos

- Windows 64 bits;
- Para jogar: `VolTerra.exe`, `libraylib.dll` e `glfw3.dll` na mesma pasta;
- Para compilar: GCC (MinGW-w64) e a biblioteca raylib instalados.

## Compilação e Execução

```bash
gcc src/main.c -o VolTerra.exe -std=c99 -Wall -lraylib -lopengl32 -lgdi32 -lwinmm -lm
./VolTerra.exe
```

A janela abre em 1280x720 a 60 FPS.

### Áudio

A música e os efeitos sonoros são **gerados por código**, então o jogo não precisa de nenhum arquivo de som. Se quiser trocar as músicas, coloque arquivos `.wav` com estes nomes em uma pasta `assets/` ao lado do executável:

`musica_menu.wav`, `musica_fase1.wav` a `musica_fase5.wav` e `musica_secreta.wav`.

## Estrutura do Projeto

```
VolTerra/
├── Makefile
├── README.md
└── src/
    └── main.c
```

O `src/main.c` concentra o jogo inteiro, organizado em seções:

| Seção | Conteúdo |
|---|---|
| Dados | Regiões, estados, eventos, vizinhança e trilhas musicais |
| Estado global | Variáveis da partida e botões |
| Áudio | Síntese de efeitos e composição da música |
| Ranking | Lista encadeada por fase e leitura/gravação em arquivo |
| Eventos e Minigames | Sorteio de eventos e os seis minigames |
| Lógica do jogo | Simulação das baterias, apoio, combo e fases |
| Desenho | Mapa, cena e todas as telas |
| Atualização das telas | Entrada do jogador e troca de telas |

## Requisitos da Disciplina

| Conceito | Onde |
|---|---|
| Structs | `Regiao`, `Estado`, `EvDef`, `EvAtivo`, `Minigame`, `Trilha` e `No` |
| Ponteiros | `Regiao *g`, `Estado *e` e `No *prox` percorridos nas simulações e no ranking |
| Alocação dinâmica | `malloc` dos nós do ranking e `calloc`/`malloc` dos buffers de áudio, com `free` correspondente |
| Vetores | `r[N]` (regiões), `est[NE]` (estados) e `evs[MAX_EV]` (eventos ativos) |
| Matrizes | `vizinho[N][N]` (fronteiras entre regiões) e `gridEst[GH][GW]` (mapa dos estados) |
| Listas encadeadas | `rankingLista[]`, uma lista de `No` por fase, ordenada por pontuação |
| Arquivos | Leitura e gravação de `ranking_fases.txt` e `segredo.txt` |

## Vídeo de Demonstração

<link do vídeo>

## Equipe

- Jose Henrique Carneiro ([@HenriqueCarneiro-dev](https://github.com/HenriqueCarneiro-dev))
- Caio Henrique de Sena Belmont ([@BelmontBeta](https://github.com/BelmontBeta))
- Gustavo Cavalcanti Neves ([@GustavoCavalcantiN](https://github.com/GustavoCavalcantiN))

Projeto desenvolvido na disciplina de Programação Imperativa e Funcional 2026.2, CESAR School.

## Créditos

Feito com [raylib](https://www.raylib.com/), biblioteca de jogos em C distribuída sob a licença zlib.