# Briefing atual — Brazil Defense

**Versão: 2026-09-23 21:45**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Consolidação dos testes de hoje — ajustes por prioridade

Contexto: jogado como player natural, tudo no nível 1, sem evoluir.
Chegou à onda 115 com placar 62/38 (a eleição VIRA disputa nas ondas
altas — funciona). A base está sólida. Estes são os atritos que
apareceram jogando, em ordem de impacto.

NÃO é calibração fina de número (essa segue pausada até o elenco). São
ajustes de estrutura e de coerência que melhoram a experiência já.

---

## 1. [PRIORIDADE] Separador de fila com orçamento próprio

O maior atrito. Hoje separador e defesa saem do MESMO bolso (fundos
públicos). Cada torre abate do que sobraria para separadores, então o
jogador nunca faz o labirinto denso ("queijo ralado") que é a alma do
maze TD — sob pressão sempre escolhe defesa, e o labirinto morre.

Solução: DESACOPLAR o separador dos fundos.

- Separador passa a ter orçamento PRÓPRIO, independente dos fundos
  públicos gastos em defesa/evolução. (Como o muro no Clash of Clans:
  economia separada das defesas.)
- O jogador ganha uma quantidade de separadores por partida, e MAIS
  ao longo do jogo (recompensa por onda/chefe), para o labirinto poder
  crescer e ficar denso.
- Assim "desenhar o caminho" (separadores, recurso próprio) e
  "defender o caminho" (fundos, escasso) viram duas decisões que NÃO
  competem.
- Expor o orçamento inicial e o ganho por onda em settings/DA.

Motivo: hoje o item 6 da fila (separador ganho na onda 67 = 1 só)
mostra que a quantidade não escala e é irrelevante no meio-fim.
Orçamento próprio + ganho crescente resolve os dois.

---

## 2. Barra de vida do candidato só após o primeiro dano

O candidato nasce com a barra de HP já exposta, mesmo sem levar hit.
Os creeps comuns já seguem a regra: barra só aparece após o primeiro
dano. Alinhar o candidato ao mesmo padrão (barra flutuante e, se
aplicável, a linha dele no painel). Só consistência visual.

---

## 3. Evolução deve ser livre durante a onda

Evoluir uma defesa em campo parece travado durante a onda ativa,
liberando só na pausa. Construir peça nova continua só na montagem
(correto), mas EVOLUIR o que já existe deve ser livre durante a
batalha — é reação tática ("essa torre não aguenta, subo ela agora").
Confirmar e liberar a evolução durante a onda.

---

## 4. Opção de personagem indisponível sem slot livre

O jogo oferece colocar personagem mesmo quando não há plataforma
posicionada com slot livre para recebê-lo. Deve ficar indisponível
com o motivo escrito ("nenhuma plataforma com vaga"), como as outras
peças recusadas — não oferecer o que não tem onde ir.

---

## Registrado, NÃO agir agora (para o elenco / balanceamento futuro)

- Partida longa demais / satura cedo: onda 83+ com tudo nível 1, sem
  decisão nova, recompensa irrelevante. Possível que 100 ondas seja
  muito para o conteúdo atual. NÃO mexer no número de ondas agora —
  depende do elenco (variedade de inimigos/torres/políticos é o que
  dá sentido às ondas altas). Anotar no PLANO como questão aberta.
- Placar: CONFIRMADO que funciona (62/38 na onda 115). Morto cedo,
  disputa tarde, como previsto. NÃO reformar o vermelho — o design
  atual entrega a disputa quando a defesa cede. Remover da lista de
  problemas.

---

## O que está certo e NÃO deve quebrar

Ao mexer no que está acima, preservar:

- Candidato que "rouba e carrega" (ameaça + sustento) e escuda a
  horda naturalmente (vira alvo, creeps passam).
- Economia unida à recompensa (mata chefe → fundo → escolhe).
- Interação natural defesa/ataque/grid já no nível 1.
- A disputa de placar que emerge nas ondas altas.

---

## Entregável

- Separador com orçamento próprio, desacoplado dos fundos, com ganho
  crescente ao longo do jogo.
- Barra do candidato só após dano.
- Evolução livre durante a onda.
- Personagem indisponível (com motivo) sem slot livre.
- Compilar os dois alvos, verificar headless, relatório no fim.
- NÃO tocar em número de ondas nem no sistema de placar.
