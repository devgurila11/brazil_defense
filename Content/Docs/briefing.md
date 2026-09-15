# Briefing atual — Brazil Defense

**Versão: 2026-09-14 23:25**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

---

## 1. Barra grande do candidato no HUD — suportar vários

A barra flutuante sobre cada cubo já funciona (mostra o HP de cada um).
O problema é a BARRA GRANDE no HUD, que expõe o HP em número: ela
mostra só UM candidato, provavelmente o agendado atual.

Quando a leva volta pela virada de votos, há vários candidatos ao
mesmo tempo. A barra do HUD precisa mostrar TODOS os candidatos vivos:

- Uma barra POR CANDIDATO vivo (individual, nunca agregada),
  empilhadas, cada uma com HP e a cor do candidato (o TintColor de
  debug). No futuro cada candidato terá nome/rosto próprio, então a
  barra precisa identificar QUEM está apanhando — deixar já um campo
  de nome/label por barra, mesmo que por ora seja "Candidato N".
- Some conforme cada um morre.
- Se ficar muito cheio (ex: 16 voltando), limitar a lista visível e
  indicar "+N" para o resto, ou encolher as barras — o importante é
  não esconder candidatos.

Motivo: com candidato na urna = game over, o jogador precisa ver
TODOS que estão vindo, não só um.

---

## 2. Recompensa por matar candidato agendado

Cada candidato AGENDADO (ondas 5, 10, 15...) morto aumenta o TETO de
orçamento de peças do jogador.

- Incremento FIXO por chefe: +1 torre e +1 personagem de teto por
  candidato agendado morto. (Expor em UBDGameBalanceSettings:
  TowerBudgetPerBoss, CharacterBudgetPerBoss.)
- NÃO dá peça pronta — dá ESPAÇO. O jogador ainda paga cada peça com
  votos azuis (que derrubam o placar). Preserva o modelo cruel: o
  chefe dá teto, não poder de graça.
- Feedback claro ao matar: mensagem/HUD "Orçamento aumentado: +1
  torre, +1 personagem".

REGRA CRÍTICA: candidatos que voltam pela VIRADA DE VOTOS (o desfile
dos caídos) NÃO dão recompensa nenhuma. Só os agendados dão. Senão o
jogador provoca a virada de propósito para farmar orçamento — o mesmo
tipo de exploit que o "igualar por baixo" já fecha no placar.

---

## Confirmado OK (não mexer)

- Barra flutuante sobre os cubos: funcionando.
- Igualar votos por baixo, sistema de votos: aprovado.
- A derrota apertada na onda 45 (candidato chegou com 330/460) é o
  comportamento certo — tensão desejada.

---

## Entregável

Compilar limpo nos dois targets.

- Barra grande do HUD mostra todos os candidatos vivos, não só um;
  testar com a leva voltando (vários ao mesmo tempo).
- Matar candidato agendado sobe o teto de torre e personagem, com
  feedback na tela.
- Candidato do desfile de virada não dá recompensa.
