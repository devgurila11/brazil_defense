# Briefing atual — Brazil Defense

**Versão: 2026-10-14 15:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Comício brota os tipos 1..N (alinhamento)

Leva curta. O comício do candidato N deve brotar as hordas 1..N, pela
mesma regra dos ônibus: só tipo liberado e com corpo (malha no DA).
Hoje brota só jumento.

## Regra

- `RallyComposition` do `DA_Candidato_NN`: tipos 1..N com peso 1 cada
  (o commandlet já deixou a lista; conferir que está 1..N e não tudo
  Horda_01).
- No brotar, cada militante é sorteado entre os tipos da lista que
  estão **liberados** (≤ 1 + agendados caídos) **e têm corpo**; os
  demais caem em jumento. Semente da onda, como nos ônibus.
- Na prática hoje: candidato 2 em diante brota Black Blocs misturados
  (≈ metade, pelo peso igual) — se ficar pesado demais no teste, não
  calibrar agora: anotar no §13.
- Sem mexer em `RallyCount`, tempos ou invencibilidade.

## Checks

- Candidato 1: 100 % jumento.
- Candidato 2 com Black Bloc liberado e com corpo: brota os dois tipos
  na proporção dos pesos; sem corpo, 100 % jumento.
- Tipo liberado > N nunca brota no comício de N.

Regressão inteira antes e depois; dois alvos; commit; §12; RAIOX §4
(linha do comício) e PLANO "Comício" (uma frase).

## Também, se couber na mesma leva

- Load de save com candidato 1 caído: conferir que a onda seguinte ao
  load ainda traz Black Blocs (liberação recalculada). Já há check de
  liberação no load; adicionar um que percorra uma onda inteira pós-load
  e conte os especiais.
