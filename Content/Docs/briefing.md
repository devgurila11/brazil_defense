# Briefing atual — Brazil Defense

**Versão: 2026-10-12 12:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Tecla "H": esconder só o PAINEL, manter o mundo

Hoje o "H" esconde TUDO, inclusive coisas que pertencem ao mundo do
jogo (exclamações de fala, barras de vida). Corrigir: o "H" deve
esconder SÓ os dados de PAINEL (interface sobreposta de gerenciamento)
e MANTER tudo que é do mundo do jogo (billboards sobre os atores).

## Com "H" — SOME (painel/interface de gerenciamento)
- Placar de votos (azul/vermelho), a barra de apuração.
- Medidores de recurso (fundos públicos, cota de separador).
- Menu de peças / barra de construção.
- Placar de abates lateral (ícones de tipo).
- Overlays de debug (alcances on-click, etc.).
- Qualquer elemento fixo na tela (HUD 2D sobreposto).

## Com "H" — FICA (mundo do jogo)
- Exclamações de fala (indicam quem está falando) — NUNCA esconder.
- Barras de vida dos NPCs.
- Estrelas de evolução das construções.
- Barra de tempo do Mito, barra do candidato.
- Qualquer indicador que flutua SOBRE um ator na cena (billboard no
  mundo).

## Regra geral
- Critério: elemento FIXO NA TELA (painel/HUD 2D) = some com H.
  Elemento que FLUTUA SOBRE UM ATOR no mundo (billboard) = fica.
- Objetivo: ver a cena/batalha viva (inimigos apanhando, falas
  pipocando) sem os dados de gerenciamento atrapalhando.

## Entregável
- "H" esconde só o painel; mantém exclamações, barras de vida,
  estrelas e barras flutuantes do mundo.
- BD.Test.Regression passa; compilar os dois alvos; commit.
