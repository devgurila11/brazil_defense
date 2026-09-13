# Brazil Defense — comandos de console

Atualizado: 13/09/2026

Todos na aba **Cmd** do Output Log (não Python).
Marcados com 🔧 rodam **só no editor**, com o Play parado.

---

## 0. Antes de tudo — layout do mapa 🔧

Só precisa refazer se mudar os pontos de entrada.
O layout atual já está salvo no nível.

```
BD.Grid.Debug 2
```
Liga o debug com os índices `X,Y` nas células perto da câmera.
Use para ler coordenadas antes de marcar qualquer coisa.

```
BD.Grid.SetCells 0 4 0 6 Spawn
BD.Grid.SetCells 0 16 0 18 Spawn
BD.Grid.SetCells 5 0 7 0 Spawn
BD.Grid.SetCells 21 0 23 0 Spawn
BD.Grid.SetCells 9 21 11 21 Spawn
BD.Grid.SetCells 25 21 27 21 Spawn
```
As seis bocas de entrada, 3 células cada.

```
BD.Grid.SaveLayout
```
Grava no `ABDGridLayoutActor`. **Depois: Ctrl+S no nível**, senão se perde.

Não marcar Goal: a urna é quem define a célula de goal agora.

---

## 1. Começar uma sessão de teste

Dá Play e roda, nesta ordem:

```
BD.Match.FreezeTimer 1
```
Congela o cronômetro da fase de montagem. Sem isso a onda sai aos 60s
e o tabuleiro fica todo vermelho (`MatchRefused`).

```
BD.Grid.Debug 1
```
Linhas do grid e cores por estado. Use `2` para ver os índices também.

```
BD.Path.ShowRoutes 1
```
Desenha as rotas coloridas, uma cor por boca. Só funciona com a urna já
posicionada. Antes da primeira onda, todas as bocas; a partir dela, só
as bocas da onda atual: a rota mais curta fina e, por cima, mais grossa,
o que falta da rota que cada creep vivo está andando de verdade (rota
por creep, `RouteCostVariance` em Project Settings > Brazil Defense -
Waves; 1.0 põe todos na mesma linha).

```
BD.HUD.Debug 1
```
Placar de teste no canto superior esquerdo: votos azul/vermelho, onda e
contagem, bocas da onda e creeps vivos. Os mesmos números de
`BD.Votes.Status` e `BD.Wave.Status`. O autossetup liga sozinho.

```
BD.Debug.AutoSetup 1
```
Defesa aleatória completa (urna, plataformas, defensores, cercas) ao
começar um mundo de jogo. **Desligado por padrão** — o fluxo real
começa com o tabuleiro vazio e a urna na mão. Ligue antes do Play numa
sessão de balanceamento; `BD.Debug.AutoSetup.Seed N` fixa a seed, e
`BD.Debug.AutoSetup.Run` roda na hora. Ele também liga o FreezeTimer e
os debugs. Linha de comando: `-BDAutoSetupSeed=N` (já liga).

---

## 2. Posicionar a urna — sempre primeiro

Enquanto não houver urna, toda outra peça é recusada
(`ObjectiveMissing`).

```
BD.Place.Select DA_Objective
```
Gesto real: preview com a mesh, zona válida pintada no chão.
Fora da zona (X 40-47, Y 6-15) recusa com `ObjectiveOutOfZone`.

```
BD.Objective.PlaceAt 43 10
```
Atalho por console, pula gesto e orçamento.

```
BD.Objective.Status
```
Célula, posição do ator e a rota de cada spawn.

---

## 3. Posicionar peças

```
BD.Place.Select DA_Divider
```
Cerca em aresta. Roda do mouse alterna o eixo; o cursor escolhe entre
as duas arestas daquele eixo. Duas cópias da mesh por aresta.

```
BD.Place.Select DA_Truck
BD.Place.Select DA_Bleachers
BD.Place.Select DA_Palanque
```
Plataformas. Roda gira em quatro passos de 90°.

Mouse: **esquerdo** posiciona, **direito** remove, **Esc** cancela.
Remover funciona com qualquer peça selecionada.

```
BD.Place.At <x> <y>
BD.Place.AtEdge <x> <y> <dir>
BD.Place.Sell <x> <y>
BD.Place.SellAtEdge <x> <y> <dir>
```
Colocação e venda por console. `dir`: 0 = +X, 1 = +Y.
(`BD.Place.RemoveAt` / `RemoveAtEdge` continuam valendo, são a mesma coisa.)

Nada no tabuleiro é permanente: qualquer peça (menos a urna) pode ser
vendida em qualquer fase. Vender devolve a peça ao orçamento e paga
**votos azuis**: 100% do BuildCost antes da onda 1, 50% a partir dela
(`SellRefundRatio`); divisória mantém 100% até a onda 3
(`DividerRemovalGraceWave`). Mover entre ondas cobra a MoveTax de
sempre. Vender uma plataforma paga a plataforma e os personagens
montados voltam ao orçamento sem pagar nada. Plataforma e divisória
continuam sem poder ser **colocadas** de novo depois da onda 1 — só
vendidas ou movidas. A linha `Sold '...' for N blue vote(s)` sai em
`LogBDMatch`.

```
BD.Place.SelectTransient <fx> <fy> <Estado>
```
Peça temporária sem mesh, para teste rápido.
Estados: `Tower`, `Platform`, `Blocked`, `Edge`.

---

## 4. Soltar a horda

```
BD.Wave.SpawnAll
```
Um creep de cada uma das seis bocas.

```
BD.Wave.Spawn <índice>
```
Só de um ponto. Índices 0 a 5 — veja a ordem em `BD.Wave.Status`.

```
BD.Wave.KillAll
```
Mata todos em campo. Conta como morte (soma voto azul).

```
BD.Wave.Status
```
Pontos de entrada, creeps vivos, posição da urna e o tamanho de cada
rota em células.

---

## 5. Controlar a partida

```
BD.Match.Status
```
Fase, onda, tempo até a próxima, votos dos dois lados.

```
BD.Match.SetPhase Building
```
Volta à montagem e rebobina a onda para 0. É o que destrava o
tabuleiro quando tudo fica vermelho.

```
BD.Match.SetPhase WaveActive
BD.Match.ClearWave
BD.Match.Speed 1
BD.Match.Speed 2
BD.Match.Speed 4
```

```
BD.Votes.Status
BD.Votes.AddBlue <n>
BD.Votes.AddRed <n>
```
No fim de cada onda o log traz `Wave N cleared. Votes: blue B, red R.`
em `LogBDMatch` — é por essa linha que uma partida se audita depois.

---

## 5b. Candidato vermelho

Surge sozinho quando **vermelho > azul** (e já houve pelo menos um voto
vermelho). Checado a cada mudança de voto. Cubo vermelho 3×3×4, lento,
mirado por todo defensor que o tiver no alcance. Barra de HP em cima
dele via debug. Nunca há dois ao mesmo tempo.

- **Chega na urna** → `Defeat`, tabuleiro congelado (time dilation 0).
  `BD.Match.SetPhase Building` destrava e rebobina.
- **Morre** → nenhum voto azul; saída de ondas pausa por 30 s, a
  contagem regressiva segura, o contador vermelho congela (chegadas
  nesse intervalo não contam). Se o placar seguir invertido, volta na
  próxima onda.

```
BD.Votes.AddRed 50
```
Jeito rápido de inverter o placar e vê-lo surgir.

```
BD.Candidate.Spawn
```
Força a saída, sem consultar o placar.

```
BD.Candidate.Status
```
Candidato em campo (boca, HP, célula, tempo vivo), pausa restante,
placar e se está invertido, quantos já saíram na partida.

Log em `LogBDCandidate`: `CANDIDATE out on wave N from mouth M with
H health`, `CANDIDATE killed on wave N after Ts alive`, `Pause started` /
`Pause over`, `CANDIDATE reached the urn ... DEFEAT`.

Overlay (`BD.HUD.Debug 1`): `CANDIDATO  HP x/y` enquanto vivo e
`APURAÇÃO CONGELADA  Ns` durante a pausa.

Parâmetros em Project Settings > Brazil Defense - Balance > Candidate:
`CandidateHealthMultiplier` (40 × o HP do creep da onda),
`CandidateSpeed` (0.5 células/s), `CandidateKillPauseSeconds` (30).
O asset é `DA_Candidate`, apontado em Brazil Defense - Waves >
`CandidateEnemy`.

---

## 5c. Interface (esqueleto cru, UMG por código)

Fluxo ao abrir o jogo (`-game` ou Standalone): mapa `MainMenu` →
Splash "Gurila Games" (2 s, qualquer tecla pula) → Loading → Menu
(Play / Options / Quit). Play faz fade e abre a Esplanada com o HUD.
No **PIE da Esplanada** o HUD sobe direto (o `BDGameMode` chama).

HUD: placar azul/vermelho no topo, onda e cronômetro, botões 1x/2x/4x,
bocas da onda. Clique num defensor: painel à direita com stats,
**Upgrade (custo)** e **Vender (+reembolso)**, cada um com o placar
resultante — em vermelho quando o gasto inverte a liderança. Peça na
mão: nome, custo, área e o motivo da recusa; ao mover, o custo da taxa
e o placar resultante. Candidato: barra de HP no topo, aviso ao surgir
e `APURAÇÃO CONGELADA Ns` na pausa. O overlay `BD.HUD.Debug` continua
existindo por cima, independente.

Textos: `Content/BD/Text/BD_en.csv` e `BD_pt.csv` (`Key,SourceString`),
uma string table por idioma. Chave faltando aparece na tela como a
própria chave. Idioma troca em runtime e todo texto refaz na hora.

Opções (gráficos / áudio / idioma) persistem em `Saved/SaveGames/
BDSettings.sav`, aplicadas e salvas a cada mudança. Resolução e modo
de janela não são aplicados dentro do editor (só em janela própria).
Os sliders de volume passam por `SMix_Settings` sobre `SC_Master` /
`SC_Music` / `SC_Effects` (`Content/BD/Audio`) — ainda não há som no
projeto; qualquer som posto nessas classes já obedece.

```
BD.UI.Status
BD.UI.Language <en|pt>
BD.UI.Options
BD.UI.Pause
BD.UI.Volume <music|effects|mute> <valor>
BD.UI.Play
BD.UI.Menu
```
Mesmas ações dos botões, para testar sem mouse. `BD.UI.Menu` volta ao
mapa do menu a partir do jogo.

Menu de pausa no jogo: botão **Menu** (canto superior esquerdo do HUD)
ou **Esc** sem nada na mão (o primeiro Esc cancela a peça/seleção, o
segundo abre o menu). Partida pausada enquanto aberto: Continuar /
Options / Menu principal / Sair.

Câmera da partida: fixa, derivada do grid (fica atrás do lado longo,
olhando o centro do tabuleiro). Altura, inclinação, FOV e lado em
Project Settings > Brazil Defense - Camera. Não há mais pawn voador;
no PIE a câmera também é essa.

Parâmetros em Project Settings > Brazil Defense - Interface: mapas,
duração do splash / loading / fade, aviso do candidato, classes de som
e os CSVs de texto.

---

## 6. Obstáculos gerados

```
BD.Obstacles.Generate <seed>
```
Mesma seed = mesmo layout. Sem seed usa a da partida.

Se der `0 obstacle(s)` com Error, é `MinPathLength` nas settings
contra o layout — hoje está em 25.

---

## 7. Consultas e diagnóstico

```
BD.Grid.CellState <x> <y>
```

```
BD.Path.Test <x1> <y1> <x2> <y2>
```
Roda uma busca e loga células, visitadas e tempo em µs.

```
BD.Path.WouldBlock <x> <y> <fx> <fy>
BD.Path.WouldBlockEdges <x> <y> <dir> <len>
```
Testa se uma peça bloquearia o caminho, sem colocar.

```
BD.Grid.SetEdge <x> <y> <dir> <0|1>
```
Pinta aresta à mão.

```
BD.Day.SetAlpha <0..1>
```
Varre o ciclo dia/noite. 0 = amanhecer, 0.5 = meio, 0.75 = noite.

```
BD.Grid.Debug 0
BD.Path.ShowRoutes 0
BD.HUD.Debug 0
```
Desliga o debug.

---

## Receita: testar desvio de obstáculo

1. Play, `BD.Match.FreezeTimer 1`
2. `BD.Grid.Debug 1` e `BD.Path.ShowRoutes 1`
3. `BD.Objective.PlaceAt 43 10`
4. Posicionar as peças que quer testar
5. `BD.Wave.SpawnAll` e observar

Para testar **repath em movimento**: solte a horda, espere ela andar,
e coloque uma cerca à frente de um creep. A rota deve recalcular e
ele desviar.

Para testar **corte de quina**: ponha uma cerca numa curva do caminho
e veja se o creep atravessa a peça visualmente ao virar.

---

## Referência do tabuleiro

| Item | Valor |
|---|---|
| Grid | 48 × 22 células, 700 cm cada |
| Origem | X -39100, Y 327.93, Z -11 |
| Bocas de spawn | 0,4-0,6 · 0,16-0,18 · 5,0-7,0 · 21,0-23,0 · 9,21-11,21 · 25,21-27,21 |
| Zona da urna | X 40-47, Y 6-15 |
| Footprint caminhão | 2×1 |
| Footprint arquibancada | 2×1 |
| Footprint palanque | 1×1 |
| Divisória | aresta, 2 instâncias de 3,5 m |
| Budget Normal | 28 divisórias, 3 plataformas, 1 urna |
