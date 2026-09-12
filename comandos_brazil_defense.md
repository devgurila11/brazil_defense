# Brazil Defense — comandos de console

Atualizado: 12/09/2026

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
Desenha as seis rotas coloridas. Só funciona com a urna já posicionada.

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
BD.Place.RemoveAt <x> <y>
BD.Place.RemoveAtEdge <x> <y> <dir>
```
Colocação por console. `dir`: 0 = +X, 1 = +Y.

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
```

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
