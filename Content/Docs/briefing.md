# Briefing atual — Brazil Defense

**Versão: 2026-10-10 15:30**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Balão de exclamação acima de quem FALA (por lado, colorido)

Com falas vindo de NPCs e dos personagens, não dá para saber de quem
veio a frase engraçada. Solução: uma EXCLAMAÇÃO colorida acima de quem
está falando, com a cor indicando o LADO. Dá leitura instantânea de
origem + lado, sem precisar mapear texto.

## Quando aparece
- SÓ em FALAS (as frases com voz). NÃO em relincho, casco, tiro, zurro
  — só quando o personagem solta uma FALA.
- Aparece no instante em que a fala COMEÇA.
- SOME quando a fala TERMINA — sem fade, corte direto ao acabar o áudio.

## Cor por lado (de quem fala)
- VERMELHO: militantes e CANDIDATOS (lado adversário).
- AZUL: personagens do jogador — Nicole, Mito, e os próximos atiradores.
- PRETO: ministros (preto da toga) — para a Fatia 3 (ministros ainda
  não existem; deixar a cor preta JÁ mapeada para quando entrarem).

## Visual
- Ícone de EXCLAMAÇÃO ("!") acima da cabeça de quem fala.
- BILLBOARD: sempre virado para a câmera.
- SOME/encolhe no zoom afastado (como a barra de vida do creep) — não
  poluir a visão geral.
- PULSA ao aparecer: um pop rápido de escala para chamar o olho.
- Levemente EMISSIVO: brilha um pouco, para destacar no meio da ação e
  à noite (com os postes). Emissivo LEVE, não um farol.

## Poluição / limites
- As falas já têm concurrency (poucas tocam juntas), então poucos
  balões por vez — ok. Mas garantir: um personagem só mostra UMA
  exclamação por vez (não empilha).
- Billboard + fade por distância evita sopa de ícones no zoom out.

## Entregável
- Exclamação acima de quem fala, cor por lado (vermelho militante/
  candidato, azul personagem do jogador, preto ministro-futuro).
- Aparece ao começar a fala, some seco ao terminar.
- Billboard, some no zoom afastado, pulsa, levemente emissiva.
- Reusar o sistema de billboard das barras/estrelas.
- BD.Test.Regression passa; compilar os dois alvos; commit.
