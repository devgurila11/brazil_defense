# Briefing atual — Brazil Defense

**Versão: 2026-09-24 23:20**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Limpar contador de votos duplicado no HUD

O HUD mostra o placar azul DUAS vezes, com o mesmo valor:
- No topo: "BLUE 5.707.053" com a barra de apuração (azul/vermelho).
- No meio: "blue count 5.707.053" ao lado de um ícone de urna.

São idênticos. O "blue count" é resquício de quando votos eram moeda
gastável; depois da reforma (votos = placar puro, fundos = moeda) ele
perdeu a função. A barra de apuração no topo já mostra o azul com o
contexto do vermelho.

## Fazer

- REMOVER a linha "blue count" e o ícone de urna que a acompanha (o do
  meio, NÃO o da barra do topo).
- Manter no topo: a barra de apuração (BLUE / barra / RED), o NULL, e
  logo abaixo o PUBLIC FUNDS (a moeda real, fica em destaque).
- Conferir que não sobra ícone de urna solto depois de remover.

## Nota (não agir agora, só registrar)

Warning de Lumen visto na tela: "Cached lighting in Lumen and
real-time sky capture lighting is going to be clipped... adjust
r.EyeAdaptation... Exposure -8.5, safe range [-8.0, 12.0]". É ajuste
de exposição/iluminação, não do HUD. Anotar no PLANO para olhar quando
mexer em iluminação/cena. NÃO agora.

## Entregável

- "blue count" e seu ícone removidos do HUD; placar azul aparece só
  uma vez (barra do topo).
- PUBLIC FUNDS mantido em destaque.
- BD.Test.Regression passa (22/22).
- Commit + push com o hash no PLANO §12.
