# Briefing atual — Brazil Defense

**Versão: 2026-10-08 16:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Remover os obstáculos sorteados (campo limpo)

Decisão de design: REMOVER os 20 obstáculos sorteados por seed. Num
jogo onde o jogador posiciona a urna e desenha o labirinto do zero,
células aleatórias bloqueadas que ele não escolheu só atrapalham — ele
tenta posicionar num ponto estratégico e dá vermelho sem motivo
visível, num cenário que parece limpo. O tabuleiro deve começar LIMPO
e desobstruído.

## Remover, mas SEM prejudicar o jogo (remoção limpa)

O gerador de obstáculos (UBDObstacleGenerator) está no projeto desde o
início e várias coisas podem depender dele. Remover com cuidado:

1. Desligar a geração dos obstáculos: o tabuleiro inicia SEM nenhuma
   célula Blocked por obstáculo. Campo totalmente livre (só as bocas,
   a urna quando posicionada, e o que o jogador construir).

2. Verificar e limpar as dependências (reportar o que achou):
   - O AutoSetup / simulador usava obstáculos? A varredura de grid, as
     rotas, o WouldBlockPath — algo assume que há células Blocked de
     obstáculo? Ajustar para funcionar com campo limpo.
   - A seed ainda é usada para outras coisas (sorteio de bocas, wander
     da horda, etc.)? NÃO quebrar esses — só a parte de obstáculos sai.
   - Os blocos visíveis (cubo cinza / ObstacleMesh) adicionados hoje
     saem junto (não há mais obstáculo para mostrar).
   - As checagens de regressão criadas hoje para os obstáculos: ajustar
     ou remover as que não fazem mais sentido; manter a auditoria de
     grid (BD.Grid.Audit) e a invariante de consistência de células
     (essa continua útil).

3. Manter o BD.Grid.Audit e a invariante "nenhuma célula ocupada sem
   dono" — são úteis independente dos obstáculos.

4. Opcional/decisão futura: deixar o gerador no código DESLIGADO por
   um cvar/flag (bGenerateObstacles=false) em vez de apagar tudo, caso
   um dia se queira obstáculos autorados (não aleatórios) no mapa. Mas
   por padrão: OFF, campo limpo.

## Confirmar
- Campo começa limpo (nenhum bloco, nenhuma célula Blocked de
  obstáculo).
- Posicionar a urna e construir em qualquer célula livre funciona sem
  vermelho indevido.
- Rotas, bocas, wander, pathfinding, candidato — tudo funciona sem os
  obstáculos.
- Balanceamento: os números da §13 foram medidos COM obstáculos. Anotar
  que a remoção muda isso (campo mais aberto = horda com rota mais
  curta se o jogador não cercar). NÃO recalibrar agora — só registrar.

## Entregável
- Obstáculos removidos; tabuleiro inicia limpo.
- Dependências verificadas e ajustadas; nada quebrado (rotas, bocas,
  pathfinding, candidato).
- Blocos cinza e checks de obstáculo removidos; auditoria de grid e
  consistência de célula mantidas.
- BD.Test.Regression passa; compilar os dois alvos; commit.
