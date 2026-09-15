# Briefing atual — Brazil Defense

**Versão: 2026-09-14 17:10**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

---

## 1. Candidatos como chefes agendados

O candidato deixa de ser evento único e vira o sistema de chefes.

- Um candidato surge a cada 5 ondas (ondas 5, 10, 15... 100) = 20 no
  total. Expor CandidateInterval (5) em UBDGameBalanceSettings.
- Cada um mais forte que o anterior. HP = HP do creep daquela onda ×
  CandidateHealthMultiplier (o x40 que já existe). Como o HP do creep
  cresce por onda, o candidato da onda 100 é muito mais forte que o
  da 5, naturalmente.
- Sai de uma boca sorteada. Lento. Alvo prioritário de todo defensor
  no alcance.
- Só um agendado por vez.

O gatilho antigo "sai quando o vermelho passa o azul" NÃO some — vira
outra coisa, ver item 3.

---

## 2. Condições de vitória e derrota

Substitui a vitória por sobrevivência.

DERROTA, por qualquer uma:

- Um candidato alcança a urna — a QUALQUER momento, mesmo na onda 27.
  Morte súbita. Fase Defeat.
- Chegar ao fim da onda 100 com o VERMELHO na frente no placar.

VITÓRIA:

- Chegar ao fim da onda 100 com o AZUL na frente. Liberta os presos
  da dificuldade. Endless continua disponível depois.

Creep normal chegando na urna NÃO é game over — só soma voto vermelho
(por HP, como já está). Só CANDIDATO na urna é morte súbita.

Atualizar o PLANO.md §8 com isto — a vitória mudou de "sobreviver a X
ondas" para "vencer a apuração ao fim da onda 100, sem deixar nenhum
candidato chegar".

---

## 3. Retorno dos candidatos (a punição da virada)

Quando o VERMELHO ultrapassa o AZUL no placar:

- Todos os candidatos JÁ MORTOS nesta partida voltam, cada um com a
  VIDA ORIGINAL de quando morreu (o da onda 80 volta forte).
- A onda atual vira SÓ o desfile deles — nenhum creep normal sai
  durante o retorno.
- Distribuídos ao longo da duração da onda.
- Defesas grudam neles (alvo prioritário).
- Qualquer um que alcance a urna = morte súbita, como qualquer
  candidato.

Se o jogador MATAR TODOS os que voltaram:

- O placar IGUALA POR BAIXO: azul e vermelho vão ambos ao valor do
  MENOR (o vermelho). O jogador NÃO ganha votos de brinde — só para
  de perder. Isso fecha o exploit de provocar a virada de propósito
  para zerar a dívida dos gastos.
- A partida segue normal, com os chefes agendados voltando a cada 5
  ondas.
- Se o vermelho passar o azul de novo mais tarde, o retorno dispara
  outra vez.

Se um dos que voltaram chegar na urna antes de todos morrerem:

- Derrota (morte súbita).

---

## 4. Cor de debug nos candidatos-cubo

Enquanto os candidatos são cubos de blocagem, dar a cada um uma cor
distinta pela ordem de surgimento (onda 5 = cor 1, onda 10 = cor 2...),
só para eu distinguir quais são quando a leva volta na virada.

- TintColor no material do cubo basta.
- É debug de blocagem — sai quando os 20 modelos reais de político
  entrarem. Não investir em paleta elaborada.

---

## 5. Economia inflada — registrar, não corrigir agora

A simulação deu ~500 mil votos na onda 68. A mudança de vermelho/azul
por HP multiplicou a escala. Os custos de peça e upgrade (dezenas)
ficaram irrelevantes.

NÃO recalibrar agora — é placeholder até o conteúdo real (vários
atiradores e inimigos). Mas registrar no PLANO que a escala de votos
e os custos precisam ser recalibrados juntos quando o conteúdo entrar.

---

## Entregável

Compilar limpo nos dois targets.

- 20 candidatos ao longo de 100 ondas, um a cada 5, cada um mais forte.
- Candidato na urna = Defeat imediato, em qualquer onda.
- Fim da onda 100: azul na frente = vitória, vermelho na frente =
  derrota.
- Virada de placar = todos os candidatos mortos voltam com vida
  original, onda vira só o desfile; matar todos iguala por baixo.
- Log claro de cada evento (candidato agendado, retorno disparado,
  igualar por baixo, condição de fim).
