# Briefing atual — Brazil Defense

**Versão: 2026-10-02 16:30**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Placar 0 a 0 + regra de mira das torres

## 1. Eleição começa 0 a 0 (remover StartingVotes)

O voto inicial azul (StartingVotes: 3000/1500/0 por dificuldade) é
ilógico num jogo de votação — começar com votos sem ter feito nada. E
com o voto agora por corpo, 1600 azuis "equivalem a 30 ondas de
abates", o que não faz sentido.

- REMOVER o StartingVotes: o placar começa 0 AZUL x 0 VERMELHO nas
  TRÊS dificuldades.
- A diferença entre Easy/Normal/Hard NÃO vem mais de voto de brinde —
  vem do resto (recursos, ondas, etc.). Sem handicap de placar.

### Consequência desejada (mecânica, confirmada): punir a inação
- Com o placar 0 a 0, se o jogador NÃO posiciona defesa, os primeiros
  militantes chegam na urna e o VERMELHO passa o azul já no começo.
- O gatilho de candidato por VIRADA (vermelho > azul) já existe e deve
  continuar ativo. Com 0 a 0, ele pode disparar CEDO se o jogador não
  defender — o candidato sai antes da onda agendada dele, chega na
  urna, e é game over precoce.
- Resultado: quem não joga, perde rápido. É intencional.
- Os DOIS gatilhos de candidato convivem:
  - Agendado (a cada 5 ondas), e
  - Virada (vermelho passa azul, a qualquer momento).
  O que vier primeiro dispara.
- O retorno dos caídos (vermelho passa azul em ondas futuras com
  vários candidatos mortos → todos voltam juntos) NÃO muda, continua
  como está.

## 2. Mira das torres de disparo — LOCAL à torre

O terminal mudou as torres para mirar "o mais próximo". Corrigir para a
regra certa, que é LOCAL à própria torre:

- Cada torre de disparo mira o inimigo MAIS ADIANTADO DENTRO DO
  ALCANCE DELA — ou seja, entre os inimigos que a torre alcança, o que
  está mais à frente no caminho (mais perto de SAIR do alcance dela
  pelo lado da urna / o que já avançou mais no trecho que ela cobre).
- Reavaliar A CADA DISPARO (disparo pausado, não contínuo): como a
  horda se move de forma dinâmica e um militante passa na frente do
  outro, se alguém ULTRAPASSA dentro do raio, o próximo tiro vai para
  o que passou na frente.
- NÃO mirar "o mais perto da urna" em termos globais — isso faria a
  torre ignorar os que passam na cara dela para tentar atirar longe,
  o que é feio e sem sentido. A referência é SEMPRE o alcance da
  própria torre.
- Cada torre defende o SEU trecho: atira em quem passa por ela,
  priorizando quem está mais adiantado dentro do raio dela.

### Resumo das miras por tipo (fixar como regra do projeto)
- Torre de disparo (não-contínuo): mais adiantado no alcance da
  própria torre, reavaliado a cada tiro.
- Agente (disparo, patrulheiro): mais próximo DELE, com trava de 15%
  (já implementado) — reage à ameaça perto enquanto patrulha.
- Arma contínua (laser, FUTURO): mira grudada no alvo (dano crescente).

## Entregável
- StartingVotes removido; placar 0 a 0 nas três dificuldades.
- Gatilho de virada ativo com 0 a 0 (candidato precoce se não defender);
  dois gatilhos convivem; retorno dos caídos mantido.
- Torres miram o mais adiantado no PRÓPRIO alcance, reavaliando a cada
  disparo (reverter o "mais próximo" das torres; Agente continua
  dinâmico).
- BD.Test.Regression passa; compilar os dois alvos; commit.
