# Briefing atual — Brazil Defense

**Versão: 2026-09-23 14:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Reforma da economia: fundos públicos são a moeda, votos são só placar

Decisão estrutural (fechada com o usuário). Hoje construir custa
votos, e votos explodem (5,8 milhões na onda 145, torre custa 100 =
grátis). E os fundos públicos crescem numa escala sã (30 mil na onda
135). A solução é trocar os papéis.

## 1. Votos = placar puro, não se gastam

- Voto azul e voto vermelho continuam existindo e continuam sendo o
  PLACAR da eleição (barra de apuração, condição de vitória/derrota
  no fim da onda 100).
- Votos NÃO são mais gastos em nada. Construir e evoluir não tocam
  nos votos. O modelo "cruel" (gastar derruba o placar) SAI — era
  baseado em votos como moeda, e votos deixaram de ser moeda.
- A barra de apuração e a disputa azul/vermelho ficam como estão.

## 2. Fundos públicos = moeda única de construção E evolução

- Tudo que o jogador coloca (torre, personagem, plataforma, divisória)
  e toda evolução se paga com FUNDOS PÚBLICOS (a propina convertida).
- Fundos vêm SÓ dos candidatos agendados mortos (os 20 chefes), como
  já é. Escassos por natureza.
- O contador de fundos públicos (Casa da Moeda) vira o recurso central
  do HUD. O medidor de votos vira informativo (placar), não gastável.

## 3. Custo de reposição ≈ valor de um candidato

O ponto central. Hoje construir é barato demais e o mapa enche antes
de qualquer evolução.

- O custo de construir/repor uma defesa deve ser PRÓXIMO ao que um
  candidato solta de fundos ao morrer.
- Efeito: cada chefe morto paga aproximadamente UMA defesa nova OU
  uma evolução — nunca as duas. Isso força a escolha "construo mais
  ou evoluo o que tenho?".
- Como o valor do candidato cresce por onda (HP × fator), o custo de
  reposição também deve escalar na mesma direção — construir na onda
  80 custa mais que na onda 10, proporcional aos fundos que os chefes
  daquela altura dão.
- Calibrar para que, ao longo dos 20 chefes, o jogador NÃO consiga
  encher o mapa E evoluir tudo — tem que escolher. A defesa completa
  e evoluída só na reta final, como já era o alvo da propina.

## 4. Capital inicial de fundos

- Como agora tudo se paga com fundos e o jogador começa sem matar
  chefe nenhum, dar um capital INICIAL de fundos públicos por
  dificuldade (StartingFunds), para montar a defesa base da fase de
  montagem.
- Suficiente para uma defesa inicial decente, não para lotar o mapa.

## 5. Liberar mais itens, não só torre

Hoje só a torre "reacende" como disponível ao longo do jogo; o mapa
vira um monte de torres iguais.

- Divisória, plataformas (stage/truck/bleachers) e personagens devem
  ir sendo liberados/disponibilizados ao longo das ondas também, não
  só a torre.
- Rever a lógica de desbloqueio: o que fica disponível quando. Se há
  UnlockWave por peça, escalonar para o jogador ter variedade, não
  só torre repetida.

## 6. Candidato sai PRIMEIRO na onda dele

- Na onda que solta um candidato, ele é o PRIMEIRO a sair, antes de
  qualquer creep comum, de uma boca sorteada.
- Motivo: hoje ele vem no meio da horda e passa despercebido — o
  usuário perdeu na onda 145 sem entender que foi candidato na urna.
  Saindo primeiro, o jogador vê, reage e prioriza.

## Nota de balanceamento

Os valores exatos (custo de reposição, StartingFunds, fator de escala)
são calibração — e a calibração fina continua pausada até o elenco de
defesas/inimigos/políticos existir. Implementar a ESTRUTURA agora
(fundos como moeda, votos como placar, custo atrelado ao candidato) e
deixar os números como placeholder ajustável em settings.

## Entregável

Compilar os dois alvos. Verificar headless que:
- Construir e evoluir debitam FUNDOS, não votos.
- Votos não são mais gastos por nada.
- Custo de reposição escala com a onda, próximo ao valor do candidato.
- Candidato sai primeiro na onda dele.
- Mais tipos de peça disponíveis ao longo do jogo, não só torre.
Relatório no fim, sem perguntar no meio.
