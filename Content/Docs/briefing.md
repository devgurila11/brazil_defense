# Briefing atual — Brazil Defense

**Versão: 2026-10-01 11:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Fechamento da Fatia 1 (Palácio) + commit

Dois ajustes de acabamento no palácio e o commit da fatia 1, antes de
partir para a fatia 2 (o Agente).

## 1. Preço do palácio

- PalaceCostInCandidates = 6.4 (era 3.0). Assim o palácio só fica
  pagável por volta do 3º candidato morto, como desenhado — não dá
  para comprar na onda 1 com o dinheiro inicial.

## 2. Estrelas somem progressivamente no zoom afastado

- Hoje as 5 estrelas têm tamanho fixo na tela e, com dois palácios
  perto, as fileiras encostam na visão de longe.
- Fazer as estrelas desaparecerem PROGRESSIVAMENTE (fade de opacidade)
  conforme a câmera afasta — não um corte seco. Perto: visíveis e
  nítidas. Afastando: vão ficando transparentes até sumir.
- Objetivo: só ver as estrelas de um palácio quando a câmera está
  perto o bastante para ler/decidir evoluir aquele palácio. No zoom
  de visão geral, somem e não poluem.
- Expor a faixa de distância do fade em settings, para ajustar.

## Commit
- Commitar a Fatia 1 inteira (palácio 2×2, estrelas, preço 6.4, o
  fade) com os testes. Confirmar o hash.

## NÃO fazer agora
- Nada de Agente, ministros, garrafa (fatias 2, 3, 4).

## Entregável
- Preço 6.4; palácio pagável ~3º candidato.
- Estrelas com fade progressivo por distância da câmera.
- BD.Test.Regression passa; compilar os dois alvos; commit + push.
