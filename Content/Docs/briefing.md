# Briefing atual — Brazil Defense

**Versão: 2026-09-25 16:30**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Placar visual de abates por tipo (HUD flex, laterais)

Placar informativo mostrando quantos de cada tipo foram abatidos, com
o rosto de cada um. Só visual, não afeta gameplay.

## Layout

- LATERAL ESQUERDA: tipos de INIMIGO (horda). Um ícone por tipo, com o
  número de abates daquele tipo ao lado.
- LATERAL DIREITA: CANDIDATOS. Um ícone com quantos dos 20 candidatos
  já foram derrubados.
- Layout FLEX/dinâmico: o ícone de um tipo só APARECE depois que o
  primeiro daquele tipo morre. A lista cresce para baixo conforme
  novos tipos entram em cena. Nada de mostrar tipo que ainda não
  apareceu.

## O ícone do jumento

- T_UI_Jumento já está na engine.
- Conta o TOTAL de jumentos mortos — as 4 skins (amarelo, arco-íris,
  tie-dye, e a 4a) somadas num contador ÚNICO. Skin diferente = mesmo
  inimigo = um ícone só.
- Regra geral: o ícone é por TIPO DE GAMEPLAY, não por textura. Quando
  entrarem inimigos com stats diferentes, cada um ganha ícone próprio;
  variações de skin do mesmo inimigo compartilham o ícone.

## O ícone dos candidatos

- Fazer um ícone PADRÃO/placeholder de candidato por enquanto (os
  candidatos ainda são cubos; o rosto real vem depois). Pode ser um
  ícone genérico simples — será substituído em breve.
- Mostra quantos dos 20 candidatos agendados foram derrubados
  (ex: "7").
- Deixar o caminho pronto para, no futuro, cada candidato ter rosto
  próprio (20 ícones), mas por ora um contador único com o placeholder.

## Animação (igual ao placar de pontuação)

- Quando um abate soma no contador, o ícone daquele tipo faz a mesma
  micro-animação do placar de votos: pulso de escala (1.0 → 1.15 →
  1.0) e o número dá um tick. Só o ícone que somou.
- Mesmo cuidado com rajada: em onda densa, muitos abates por segundo —
  não reiniciar a animação a cada morte, acumular num pulso só, senão
  vira tremor contínuo.
- Reusar a lógica de pulso que o placar de votos já tem (FBDPulse ou
  equivalente).

## Fonte dos dados

- Já existe contagem de creeps mortos e candidatos mortos no
  MatchManager / PostMatch. Ligar o HUD a essas contagens, agora
  quebradas POR TIPO (o jumento precisa somar as mortes de qualquer
  uma das 4 skins no mesmo contador).
- Se hoje a contagem de mortes não distingue tipo, adicionar um
  contador por tipo de inimigo (chave = tipo, não skin).

## Responsivo

- Segue a mesma regra do resto do HUD: ancorado nas laterais,
  escala por DPI, ícones dimensionados por fração da tela, não pixel
  fixo. Não pode quebrar em 1080p/1440p/4K.

## Entregável

- Lateral esquerda com ícone de jumento + total de abates (4 skins
  somadas), aparecendo quando o primeiro jumento morre.
- Lateral direita com ícone placeholder de candidato + quantos dos 20
  derrubados.
- Layout flex: cresce conforme novos tipos morrem.
- Pulso animado ao somar, como o placar de votos, com trava de rajada.
- Responsivo nas três resoluções.
- BD.Test.Regression passa; compilar os dois alvos.
