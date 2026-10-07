# Briefing atual — Brazil Defense

**Versão: 2026-10-07 14:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# HUD transparente + estrelas + postes só à noite + point light

## 1. HUD de seleção mais transparente

- A barra/painel onde se selecionam as peças (divisória, torre,
  plataforma, etc.) está meio escura e atrapalha ver o cenário atrás.
- Aumentar a TRANSPARÊNCIA (baixar a opacidade do fundo desse HUD) para
  ver o jogo por trás com mais nitidez. Manter o texto/ícones legíveis.

## 2. BUG — estrelas "brigam" com o personagem no palanque

- Quando se põe um personagem no palanque, as estrelas de nível SOBEM,
  como se brigassem com a colisão/altura do personagem.
- Causa: a altura das estrelas está sendo calculada pelo TOPO DINÂMICO
  da peça (bounds que inclui o personagem em cima), então muda quando
  há personagem.
- Correto: a altura das estrelas é FIXA em relação à CONSTRUÇÃO (o
  palanque/plataforma), não ao que está em cima. As estrelas marcam a
  PLATAFORMA, não a tropa — ficam sempre na mesma altura, com ou sem
  personagem, em qualquer nível.
- Usar um offset fixo a partir do ator da construção, não o
  bounds/topo dinâmico. Vale para todas as construções.

## 3. Postes só acendem à NOITE (sem intensidade no amanhecer/entardecer)

- O amanhecer e o entardecer estão bonitos e NÃO precisam dos postes
  acesos. Os postes só devem acender na NOITE fechada.
- Ajustar a curva CF_StreetLightIntensity: 0 no amanhecer e no
  entardecer, subindo só na noite. Isso também zera o "25% às 06h na
  montagem" (vai para 0 — poste apagado no início da partida).
- Transição ainda suave, mas concentrada na noite.

## 4. Novo Point Light do poste deve respeitar o acendimento

- O usuário adicionou um POINT LIGHT a mais no BP do poste (além dos
  spots), para o efeito de luz sumindo mais realista.
- Esse Point Light deve seguir o MESMO controle de ciclo dos spots:
  acende à noite, apaga de dia, com a mesma curva/fade. Hoje o código
  controla os spots; incluir o Point Light novo no mesmo controle.
- Confirmar que o BD.Day.Lamps e as checagens de regressão também
  cobrem o Point Light (não deixar luz do poste fora do controle).

## Entregável
- HUD de seleção mais transparente, texto legível.
- Estrelas com altura fixa da construção (não brigam com o personagem).
- Postes só acendem à noite (curva 0 no amanhecer/entardecer).
- Point Light do poste controlado pelo ciclo, como os spots.
- BD.Test.Regression passa; compilar os dois alvos; commit.
