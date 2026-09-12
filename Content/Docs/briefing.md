# Briefing atual — Brazil Defense

**Versão: 2026-09-12 21:40**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.
> Compare a versão acima com a última que você leu. Se mudou, há
> trabalho novo. Se igual, nada a fazer.

---

## 1. Overlay de placar na tela

Não existe HUD nenhum ainda — o `BD.HUD.Debug` que eu pedi nunca
entrou, foi erro meu assumir que sim. Fazer agora, pequeno, sobre o
`UDebugDrawService` que o `BDGridDebugDrawer` já usa.

Canto superior esquerdo, via `UCanvas`, atualizado a cada frame:

```
AZUL 240        VERMELHO 85
Onda 12  ·  próxima em 23s
Bocas: 0, 3, 5  ·  vivos: 41
```

- Azul e vermelho nas cores.
- Se o candidato estiver vivo, uma linha extra: `CANDIDATO  HP 1840/3200`
- Durante a pausa pós-candidato: `APURAÇÃO CONGELADA  18s`

Ligado por `BD.HUD.Debug 1`. Ligado automaticamente pelo autossetup.

Isso é placar de teste, não o HUD de verdade — esse vem depois com
localização e arte.

---

## 2. Variação de rota por creep

### Problema
O A* devolve sempre o caminho mais curto, e ele é sempre o mesmo
enquanto o labirinto não muda. A horda inteira repete a mesma linha,
onda após onda. Previsível, e o jogador que acertou uma vez nunca
mais precisa mexer.

### Solução
Rota por creep, com custo aleatório por célula.

- No spawn, cada creep sorteia (com a seed) um mapa de custo: cada
  célula transitável recebe um multiplicador entre 1.0 e
  `RouteCostVariance` (default 1.3).
- O A* daquele creep usa esse custo em vez do uniforme.
- Dentro da mesma onda, os creeps se dividem entre os corredores
  quase-equivalentes. A multidão se espalha.

### O que NÃO muda
- Não faz a horda "acertar mais". Se o jogador cercou bem, todos os
  caminhos passam pela defesa.
- `WouldBlockPath` continua com custo uniforme. Validação de bloqueio
  é sobre existência de caminho, não sobre qual caminho. Se usasse
  custo aleatório, a mesma cerca seria aceita numa partida e recusada
  na outra.
- O repath em movimento mantém o mapa de custo do creep.

### Modo
Enum em `UBDWaveSettings`, `RouteVarianceMode`:
- `PerCreep` — cada creep sorteia o próprio mapa (default)
- `PerSpawnPointPerWave` — um mapa por boca por onda, creeps da mesma
  boca seguem juntos

### Debug
`BD.Path.ShowRoutes` mostra, por boca ativa, a rota de custo uniforme
mais fina e as rotas efetivas dos creeps vivos mais grossas.

### Parâmetros
`UBDWaveSettings`: `RouteCostVariance` (1.3), `RouteVarianceMode`.

---

## 3. Verificação visual pendente (minha)

Já implementado, falta eu confirmar no PIE:
- Rotas por onda desenhando só as bocas ativas
- Candidato: `BD.Votes.AddRed 50`, deixar inverter, ver o cubo sair,
  morrer ou chegar

Lembrete para mim: `BD.Path.Debug` (última busca, linha única) e
`BD.Path.ShowRoutes` (rotas por boca) são cvars diferentes.

---

## Entregável

Compilar limpo nos dois targets.

- Overlay visível no PIE com os números batendo com `BD.Votes.Status`.
- Onda de 30 creeps de uma boca só, `RouteCostVariance 1.3`: creeps
  distribuídos por pelo menos dois corredores quando houver alternativa.
- Mesma onda com 1.0: todos na mesma linha, como hoje.
- `WouldBlockPath` continua recusando a última cerca que fecharia o
  caminho, independente da variação.
