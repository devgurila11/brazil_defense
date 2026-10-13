# Briefing atual — Brazil Defense

**Versão: 2026-10-14 09:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Hordas especiais nos ônibus + Black Bloc (Horda_02)

Consultar o RAIOX (§3 horda, §4 candidato, §11 elenco) antes e
atualizá-lo no fim. O comício já brota os tipos 1..N; esta leva é a
chegada dos tipos pelos ônibus e o primeiro tipo especial de verdade.

## Liberação (regra fechada)

- Horda_01 (jumentos) sai desde a onda 1.
- A morte do candidato **agendado** N libera a Horda N+1. Quem volta
  no desfile não libera nada. Teto: Horda_20 (liberada pelo 19; o 20
  não libera).
- A horda liberada passa a sair dos ônibus **a partir da onda seguinte**
  à da morte (morreu na onda 5 → Horda_02 nos ônibus da onda 6).
- O conjunto liberado é derivado, não guardado: liberados = 1 + número
  de agendados distintos mortos (teto 20). Save/load já grava os
  caídos, então o load reconstrói sozinho. Conferir que o registro de
  caídos distingue agendado de retorno (hoje `Fallen` guarda só a 1ª
  morte, que é sempre a agendada — serve).

## Mistura por onda

- Total da onda não muda (W × bocas). Dentro dele, **1 especial a cada
  `SpecialEvery` = 10 jumentos**: o 10º, 20º, 30º… creep da sequência
  round-robin global é um especial, sorteado uniforme (com a semente
  da onda) entre os tipos liberados 2..K. Sem liberado, só jumento.
- O especial substitui o jumento da posição, não soma.
- `SpecialEvery` e um `SpecialWeights` opcional (peso por tipo, default
  1) ficam em `UBDWaveSettings`.
- Spawn pelo mesmo caminho do comício (`SpawnEnemyAtCell` / resolução
  de DA por tipo) para não haver dois códigos de "qual DA é a Horda
  N". Vida do especial = MaxHealth do DA dele × 1,022^(W−1), como hoje.
- Votos: **1 por corpo para qualquer tipo** (bVotesByBody continua;
  `VotesOnDeath` segue sem uso). Placar de abates por `KillType`/
  `KillIcon` do DA, como já existe.

## Black Bloc — Horda_02

Vândalo. Comportamento novo, parametrizado no `UBDEnemyData` para
qualquer horda poder usar:

- `SpeedScale` 1,4 (sobre a velocidade do jumento, 1,5 → 2,1 cél/s).
- `HealthScale` 2,0 (vida base 20; escala por onda igual).
- `bStops` true · `StopIntervalMin/Max` 4–8 s · `StopDuration` 1,5 s
  · `StopAnimation` (agressão). Parado ele é alvo normal (vulnerável).
  Nesta leva a parada **não tem efeito de jogo** — só a animação; o
  efeito (chutar cerca ou puxar a horda) é decisão futura do usuário.
- Primeiro stop só depois de `StopFirstDelay` 3 s fora do ônibus; não
  para na perna final (≤ 2 células da urna).
- Placeholder de corpo até a arte: a malha do jumento com um MI preto
  (`MI_Jumento_BlackBloc`, só cor), anim de agressão = Idle do jumento
  se existir, senão o Run congelado. `KillIcon` provisório distinto do
  jumento (silhueta preta) para o placar de abates mostrar o tipo.
- `DisplayName` "Black Bloc" nas duas tabelas (`Enemy.02`).

Hordas 03..20 continuam só esqueleto (DA vazio), fora dos ônibus até
terem DA preenchido: um tipo liberado cujo DA não tem malha **não é
sorteado** (cai no jumento) — e o `BD.Cast.Audit` já mostra o que
falta.

## HUD

- No início da primeira onda em que um tipo novo sai: aviso
  "NOVA HORDA: <nome>" 3 s, na linha dos avisos do candidato.

## Console

- `BD.Wave.Unlocked` — tipos liberados e desde qual onda.
- `BD.Wave.SetUnlocked <k>` — força k liberados (debug).
- `BD.Wave.Status` passa a mostrar a composição da onda (por tipo).

## Checks (regressão)

- Sem candidato morto: 100 % jumento em qualquer onda.
- Candidato 1 morto na onda 5 → onda 5 sem especial; onda 6 com 1
  Black Bloc a cada 10 (contar exato pela sequência round-robin).
- Retornante morto não libera.
- Dois liberados → sorteio só entre eles, com semente (mesma onda,
  mesma seed → mesma composição).
- Liberado sem malha → não sorteado.
- Black Bloc: velocidade e vida pelos scales; para e retoma nos tempos
  do DA; não para na perna final; vulnerável parado; 1 voto ao morrer.
- Load de um save com 2 agendados caídos → 3 liberados sem gravar nada
  a mais.
- Regressão inteira antes dos checks novos; dois alvos; commit; §12;
  RAIOX (§3 horda — tipo, composição, stop; §4 morte do candidato;
  settings); PLANO: seção "Hordas" só com a regra.

## Entregável

- Onda 6 depois de matar o candidato 1 mostrando Black Blocs pretos
  entre os jumentos, parando de vez em quando. Para ver na tela:
  `BD.Wave.SetUnlocked 2` + `BD.Match.SetWave 6` + `BD.Match.CallWave`.
