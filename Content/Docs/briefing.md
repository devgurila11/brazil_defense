# Briefing atual — Brazil Defense

**Versão: 2026-10-01 16:30**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Regra de ouro da economia + 2 correções

## 1. REGRA DE OURO: preço de construir é FIXO; só evolução sobe

Decisão estrutural que vale para o JOGO INTEIRO (modelo Clash of
Clans). Hoje vários preços estão atrelados à onda/candidato e viram
"areia movediça" — o preço foge da renda e a peça fica inalcançável
(aconteceu com o palácio: custou 3.546 na onda 16 com o jogador tendo
3.064).

Nova regra:
- CONSTRUIR qualquer peça (palácio, palanque, caminhão, arquibancada,
  personagem, torre, cerca/separador — tudo): PREÇO FIXO. Nunca sobe
  com a onda. O jogador sempre sabe quanto custa e pode planejar.
- EVOLUIR (subir nível/estrela): o preço CRESCE por nível. Curva
  sugerida: TRIPLICAR a cada nível (nível 1→2 = base; 2→3 = 3×;
  3→4 = 9×; 4→5 = 27×). Expor o multiplicador (3.0) em settings.
- RENDA (propina por candidato morto): continua CRESCENDO com a onda —
  é a ÚNICA coisa que sobe, para acompanhar a dificuldade e permitir
  evoluir conforme as ondas endurecem.

Aplicar:
- Remover QUALQUER custo de construção atrelado a onda/candidato.
  Procurar todos os preços que hoje escalam (PalaceCostInCandidates e
  outros que a reforma da economia atrelou) e torná-los FIXOS.
- Palácio: preço fixo de ~1.800 (o valor-alvo do 3º candidato, para
  manter a progressão "junta uns candidatos e compra", mas SEM fugir
  depois).
- Reportar quais peças tinham preço atrelado à onda e viraram fixas.
- A evolução é que usa a curva de triplicar.

## 2. Som dos passos dos militantes -50%

- Reduzir o volume do casco (passos) em 50%. Está alto demais em
  relação ao resto.

## 3. DIAGNÓSTICO — candidato passou na wave 11 sem game over

O usuário relatou: na wave 11 um candidato passou e o jogo CONTINUOU
(não deu game over), e o vermelho subiu ~500.

Investigar:
- Era um CANDIDATO de verdade (chefe) ou um militante comum? Se foi
  militante comum, os ~500 vermelhos são o HP dele e está correto
  (não é game over).
- Se foi CANDIDATO mesmo: o game over por candidato na urna está
  QUEBRADO? Era regra central (candidato na urna = derrota imediata).
  Checar se alguma mudança recente quebrou isso.
- Logar: quando um candidato chega na urna, ele dispara Defeat? Testar
  forçando um candidato até a urna.

Explicar o que achou antes de corrigir.

## NÃO fazer agora
- Ministros, garrafa (fatias 3 e 4).

## Entregável
- Todos os preços de CONSTRUÇÃO fixos; só evolução sobe (triplicando).
- Palácio fixo ~1.800.
- Lista das peças que tinham preço atrelado à onda e foram corrigidas.
- Passos -50%.
- Diagnóstico do candidato na wave 11: dizer se era candidato e se o
  game over está funcionando.
- BD.Test.Regression passa; compilar os dois alvos; commit.
