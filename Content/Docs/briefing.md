# Briefing atual — Brazil Defense

**Versão: 2026-10-10 12:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Ajustes do atirador: altura, nome no menu, giro da arma

Animações e sincronia de disparo da Nicole estão ÓTIMAS. Faltam três
ajustes de acabamento.

## 1. Atiradores flutuando ~1,5m acima do deck da plataforma

- Os atiradores (Nicole) ficam ~1,5m ACIMA do piso da plataforma —
  flutuando, não pisando no deck.
- Acontece no PALANQUE e no CAMINHÃO (e provavelmente arquibancada) —
  é geral dos slots de plataforma, não de uma só.
- Corrigir a altura (Z) do atirador no slot para ele PISAR no deck da
  plataforma, não flutuar acima.
- Verificar nos três tipos (palanque, caminhão, arquibancada) e nos
  andares de evolução (quando a plataforma sobe de nível, o atirador
  acompanha o deck).

## 2. Menu drop-up mostra "Shooter" em vez de "Nicole"

- Ao clicar em Atiradores, o item no drop-up aparece como "Shooter"
  (nome genérico). Deve aparecer o nome do PERSONAGEM: "Nicole".
- Ajustar o DisplayName do DA_Shooter_Pistol para "Nicole" (FText,
  pt/en). O menu lê o DisplayName de cada atirador — quando entrarem
  outros, cada um mostra o seu nome.

## 3. Arma precisa de leve giro para a direita (alinhar com a linha de tiro)

- A pistola está quase alinhada, mas precisa de um leve giro para a
  DIREITA (lado de fora da mão) para o cano ficar alinhado com a linha
  de tiro.
- Ajuste fino no campo GRIP do nível 0 (um pequeno yaw à direita), nos
  DataAssets da Nicole (DA_Shooter_Pistol) e do Mito (DA_PalaceData) —
  a mesma correção provavelmente serve para os dois, já que usam a
  mesma pistola.
- Conferir pelo frame do BD Shot que o cano aponta na direção do tiro.

## Entregável
- Atiradores pisando no deck das plataformas (todos os tipos e níveis).
- Menu drop-up mostrando "Nicole" (DisplayName).
- Arma alinhada com a linha de tiro (giro fino na Grip).
- BD.Test.Regression passa; compilar os dois alvos; commit (inclui o
  que ficou da auditoria anterior).
