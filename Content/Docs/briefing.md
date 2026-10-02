# Briefing atual — Brazil Defense

**Versão: 2026-10-01 15:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# FATIA 2 de 4: O Agente (o "Mito")

A fatia 1 (palácio) está pronta. Agora o Agente que sai do palácio e
patrulha. É 1 agente só (palácio é único). Ministros e garrafa são
fatias 3 e 4 — NÃO fazer agora, EXCETO deixar o chute pronto (ver fim).

Modelo e animações já estão na engine (confirmar nomes/caminhos):
- Animações: Kicking, Pistol_Idle, Pistol_Walk, Shooting, Sleeping_Idle
- Modelo do agente (o "Mito") já importado.

## Spawn e vínculo
- Ao construir o palácio, nasce 1 Agente que sai dele.
- O Agente pertence ao palácio. Se o palácio for vendido/removido, o
  Agente some.

## Patrulha
- O Agente anda pelo grid de forma aleatória (Pistol_Walk), parando às
  vezes em idle (Pistol_Idle) por um tempo curto aleatório, depois
  volta a andar. Patrulha sem destino fixo.
- SEM colisão com construções, NPCs ou outros atores — só o SEPARADOR
  DE FILA o bloqueia (igual aos NPCs).
- Movimento livre pelo grid (não segue a rota da horda; vagueia).

## Detecção e tiro
- Raio de 3 células centrado no Agente (debug sphere, como o alcance
  das torres).
- Quando um NPC entra no raio: o Agente PARA, vira para o alvo, toca
  Shooting e atira. NÃO persegue — só para e atira de onde está.
- Alvo: UM NPC por vez. Escolher o mais próximo do Agente dentro do
  raio (confirmar se prefere o mais adiantado na rota).
- Dano: é DANO (o NPC tem HP), não morte instantânea. Começa com
  PISTOLA (nível 0): cadência de uma Glock (tiro a tiro, mais lento
  que metralhadora) — expor FireRate em settings.
- Enquanto houver alvo no raio, fica parado atirando até limpar; sem
  alvo, volta a patrulhar.
- Projétil/efeito de tiro simples por enquanto (o importante é a
  mecânica).

## Barra de tempo de patrulha + recarga (sleep)
- O Agente tem uma BARRA DE TEMPO (estilo a barra de vida do NPC,
  billboard para a câmera) que conta o tempo de patrulha.
- O tempo corre SEMPRE, inclusive durante as ondas.
- Quando a barra esgota, o Agente volta ao palácio e DORME
  (Sleeping_Idle) para recarregar.
  - Ele dorme DEITADO NO GRID, À FRENTE do palácio (não dentro da
    malha do palácio — evitar efeito feio de interpenetração).
  - Recarga dura o tempo de UMA ONDA (a próxima). Durante a recarga
    ele NÃO defende — janela de vulnerabilidade intencional.
  - Recarregado, volta a patrulhar com a barra cheia.

## Tempo extra por abate
- Cada NPC morto pelo Agente adiciona tempo de patrulha: +1s por abate
  no nível 0 (pistola).
- A cada nível de arma (evolução do palácio), o bônus por abate
  DIMINUI 0,15s (arma mais forte mata mais fácil, então cada abate
  vale menos descanso extra): nível 0 = +1,0s; nível 5 = +0,25s.
  (CONFIRMAR esta leitura com o usuário antes de fechar.)
- Acumula de forma lenta e gradual, sem teto rígido definido por ora.
- Expor os valores em settings.

## Evolução (palácio → armas do Agente)
- As 5 estrelas do palácio já existem. Preparar a estrutura: cada
  nível (1-5) troca a ARMA do Agente (dano, cadência, e no futuro a
  mesh/efeito da arma). Por ora só a PISTOLA (nível 0) funciona.
- Deixar o array de 5 armas no DataAsset (dano, cadência, bônus de
  tempo por nível), com só a pistola preenchida. As outras 4 ficam
  como placeholder para depois.

## Chute — deixar PRONTO, sem uso ainda
- A animação Kicking e a ação de chute devem ficar implementadas e
  prontas, mesmo sem alvo (os ministros são a fatia 3).
- Quando o primeiro ministro existir (fatia 3), o Agente já saберá
  chutar. Por ora, deixar um comando de debug (ex: BD.Agent.Kick) para
  testar a animação de chute isolada.

## NÃO fazer agora
- Ministros, STF, garrafa (fatias 3 e 4).
- As 4 armas evoluídas (só a pistola).

## Entregável
- 1 Agente sai do palácio, patrulha (anda/idle aleatório), sem colisão
  exceto separador.
- Raio de 3 células; para e atira (Glock) o NPC mais próximo, dano,
  sem perseguir.
- Barra de tempo billboard; esgotou → dorme à frente do palácio
  (deitado no grid, fora da malha), recarrega em 1 onda, não defende.
- +1s por abate (nível 0), diminuindo 0,15s por nível de arma.
- Array de 5 armas no DataAsset, só pistola preenchida.
- Chute pronto (comando de debug), sem alvo ainda.
- BD.Test.Regression passa; compilar os dois alvos; commit.
- Testável: palácio na tela, agente saindo, patrulhando, atirando em
  NPC, dormindo ao esgotar a barra.
