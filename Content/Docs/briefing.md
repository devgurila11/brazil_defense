# Briefing atual — Brazil Defense

**Versão: 2026-09-24 22:30**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Log por onda (wave-by-wave) para estudo de balanceamento

Já existe o PostMatch.csv (uma linha por PARTIDA, o resultado final).
Falta o passo a passo: uma linha por ONDA, para ver a evolução dentro
da partida — em que onda o vermelho encosta, quando a defesa satura,
quando os fundos apertam.

## Onde

- Arquivo próprio: Saved/Logs/WaveLog.csv, uma linha por onda
  concluída (append). CSV, para abrir em planilha e plotar.
- Também um resumo curto no log a cada onda (LogBDMatch), legível.
- Vale para partida na tela, headless e simulação, com a coluna Mode
  distinguindo (como no PostMatch).
- Ligável/desligável por cvar (BD.WaveLog.Enabled), default ligado
  durante o desenvolvimento.

## Colunas por onda

Identificação:

- Build, seed, dificuldade, Mode, número da onda, se é onda de chefe.

Placar naquela onda:

- Azul, vermelho, null (acumulados até o fim da onda).
- Delta de azul e de vermelho na onda (quanto cada um subiu SÓ nesta
  onda) — é o que mostra o ritmo, mais útil que o acumulado.

Economia:

- Fundos públicos no fim da onda, fundos ganhos na onda (chefe),
  fundos gastos na onda.
- Cota de separador restante.

Defesa (estado no fim da onda):

- Nº de torres, personagens, plataformas, separadores.
- Nível médio dos defensores.

Combate na onda:

- Creeps gerados, mortos, que chegaram na urna NESTA onda.
- Candidato: se saiu, se morreu, se chegou.
- Overkill (null) da onda.
- Pico de creeps vivos na onda.

Comprimento da rota atual (para ver o efeito do labirinto ao longo
do jogo).

## Objetivo

Depois de uma partida, o WaveLog.csv vira um gráfico: as curvas de
azul e vermelho por onda, a de fundos, a de nível médio. Aí dá para
ver PADRÕES ("o vermelho sempre encosta na onda X", "os fundos sempre
apertam entre Y e Z") que uma linha só de resultado não mostra.

## Cuidado

Não poluir o log de texto — o resumo por onda em LogBDMatch deve ser
UMA linha curta. O detalhe fica no CSV. E o CSV segue a mesma regra do
PostMatch: se as colunas mudarem, versiona o antigo com data.

## Entregável

- WaveLog.csv escrito a cada onda, com as colunas acima.
- Resumo de uma linha por onda no log.
- Ligável por cvar.
- Testar numa simulação de ~30 ondas e confirmar que sai uma linha por
  onda com os números batendo com o BD.Economy.Report e o placar.
- Rodar BD.Test.Regression e confirmar que passa.
