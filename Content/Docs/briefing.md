# Briefing atual — Brazil Defense

**Versão: 2026-09-23 19:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Relatório de fim de partida (post-match report)

Ao terminar QUALQUER partida jogada na tela (vitória, derrota ou sair),
gravar um relatório com os parâmetros gerais da partida — para
acumular dados reais de teste e calibrar o balanceamento com número,
não só com o olho.

## Onde e como

- Gravar num arquivo próprio, acrescentando (append) a cada partida:
  Saved/Logs/PostMatch.csv (uma linha por partida) OU um .log legível.
  CSV é melhor — dá para abrir em planilha e comparar dezenas de
  partidas.
- Também logar um resumo legível no fim (LogBDMatch) para leitura
  rápida.
- Vale tanto para partida na tela quanto para as simuladas
  (BD.Sim.Run), com uma coluna dizendo qual foi.

## Dados por partida (as colunas)

Identificação:

- Data/hora, seed, dificuldade, se foi simulação ou jogada na tela.
- Resultado: vitória / derrota / abandonada.
- Onda alcançada. Motivo do fim (candidato na urna / apuração onda
  100 / abandono).

Placar:

- Votos azul e vermelho no fim, e a razão entre eles.
- Nulos no fim.

Economia:

- Fundos públicos ganhos no total (soma dos candidatos).
- Fundos gastos em construção vs. em evolução (separado).
- Fundos que sobraram no fim.
- Capital inicial usado.

Defesa:

- Nº de cada tipo no fim: torres, personagens, plataformas
  (por tipo), divisórias.
- Nível médio dos defensores e nível máximo atingido.
- Quantas evoluções foram compradas.

Combate:

- Creeps mortos, creeps que chegaram na urna.
- Candidatos mortos (dos 20) e qual chegou na urna, se algum.
- Dano total aplicado, overkill total (nulo), % de desperdício.
- Pico de creeps vivos simultâneos.

Ritmo:

- Duração da partida (tempo real e nº de ondas).
- Onda em que a defesa "estabilizou" (parou de crescer) se der para
  medir.

## Objetivo

Depois de várias partidas, esse CSV mostra padrões: em que onda se
perde mais, se os fundos sobram ou faltam, se a evolução acompanha, se
o placar fica disputado ou é atropelo. É a base de dados para a
calibração final (que continua pausada até o elenco, mas os dados vão
se acumulando desde já).

## Entregável

- PostMatch.csv sendo escrito ao fim de cada partida (tela e sim).
- Resumo legível no log.
- Confirmar com uma partida simulada que a linha sai completa e os
  números batem com o BD.Economy.Report.
