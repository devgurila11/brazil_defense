# Briefing atual — Brazil Defense

**Versão: 2026-10-07 16:15**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# BUG — postes não acendem nem à noite

Na última mudança (tirar a intensidade do amanhecer/entardecer), a
curva CF_StreetLightIntensity foi zerada DEMAIS: o pico da noite também
ficou em 0. Agora os postes NÃO acendem em horário nenhum — confirmado
que BD.Day.SetAlpha 0.7 (noite cheia, ~22h47) deixa tudo apagado.

## Correção
- Os postes devem acender quando o CENÁRIO FICA ESCURO (noite), e
  apagar quando está claro (dia/amanhecer/entardecer).
- A lógica mais robusta: a intensidade do poste segue o INVERSO da luz
  do sol — sol forte = poste apagado; sol apagado (cenário preto) =
  poste aceso (pico). Assim nunca descasa do visual.
- Mantido o que o usuário pediu: amanhecer e entardecer SEM poste
  aceso (eles são bonitos sozinhos). Só a NOITE ESCURA acende.
- Garantir que o PICO da noite seja a intensidade cheia (10 cd nos
  spots / brilho cheio no vidro), não 0. O erro foi zerar o pico junto
  com as pontas.
- Transição suave (fade) entrando e saindo da noite, mas o miolo da
  noite com luz CHEIA.

## Testar
- BD.Day.SetAlpha 0.7 (noite cheia) → postes ACESOS, spots 10 cd,
  vidro e point light no máximo.
- 0.5 (entardecer) → apagado (ou quase).
- 0.3 (dia) → apagado.
- Conferir visualmente: quando o cenário escurece, os postes acendem.

## Entregável
- Postes acendem na noite escura (pico = luz cheia), apagados de dia e
  no amanhecer/entardecer.
- Spots, vidro e point light todos acendendo juntos no pico da noite.
- BD.Test.Regression: a checagem da curva deve confirmar PICO CHEIO na
  noite (não só "0 fora da noite") — ajustar para pegar este caso, que
  escapou (a curva passou no teste zerada).
- Compilar os dois alvos; commit.
