# Briefing atual — Brazil Defense

**Versão: 2026-10-08 10:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Primeiro ATIRADOR de plataforma — estrutura completa

O usuário vai criar/importar: modelo do personagem, arma (separada),
animações (Idle, Shooting), 4 sons de disparo, partículas no Niagara
(muzzle flash, impacto) e textura de sangue (decal). O terminal monta
TODA A ESTRUTURA com os encaixes PRONTOS e slots vazios, para o usuário
preencher conforme cria. Nada do terminal espera os assets — deixar
tudo plugável.

Este é o PRIMEIRO atirador de verdade — substitui o placeholder atual
de personagem de plataforma (vira o padrão). Single shot, como as
defesas protótipo atuais.

---

## 1. O atirador (personagem de plataforma)

- Novo personagem de plataforma: fica no slot, Idle quando sem alvo,
  Shooting quando NPC entra no raio.
- Mira: mesma regra das defesas de disparo (mais adiantado no próprio
  alcance, reavaliado a cada tiro, sem prioridade de candidato — o que
  já vale para plataformas).
- Single shot (cadência de pistola por enquanto).
- Slot de MESH do personagem pronto (placeholder até o usuário importar
  o modelo). Slots de ANIMAÇÃO: Idle e Shooting (apontar quando
  importar).

## 2. Arma SEPARADA + socket na mão (base da evolução)

- A arma é um ator/mesh SEPARADO, anexado a um SOCKET na mão do
  personagem (empunhadura). O usuário vai criar o socket na mão no
  editor de skeletal; deixar o código/anexo pronto para usar o socket
  (ex: nome "hand_r" ou o que o usuário definir — confirmar o nome).
- Evolução = TROCAR A ARMA: array de armas por nível (nível 0-5), cada
  uma com: modelo da arma, dano, cadência, muzzle flash (Niagara), som
  de disparo. Só a arma base (pistola) preenchida; as outras 4-5 como
  placeholder.
- Subir de nível (estrela) troca o modelo da arma no socket e os
  parâmetros (dano/cadência maiores). Preparar isso já, mesmo com só a
  pistola existindo.

## 3. Muzzle flash no cano (Niagara — slot pronto)

- Socket no CANO da arma (muzzle) para o flash sair da ponta.
- Slot para um Niagara System de muzzle flash, disparado a cada tiro,
  sincronizado com a animação Shooting (Anim Notify) e a cadência.
- O usuário cria o Niagara; deixar o ponto de disparo e o socket
  prontos. Placeholder vazio até ele criar.

## 4. Partícula de impacto no NPC (Niagara — slot pronto)

- Quando o tiro ACERTA o NPC, disparar um Niagara System de impacto na
  posição do acerto.
- Slot pronto, placeholder vazio. O usuário cria o efeito.

## 5. Decal de sangue VERDE no chão

- Quando um NPC é ABATIDO, aplicar um DECAL de sangue verde no chão, na
  posição da queda.
- A textura vem do usuário (gera no Nano); deixar o slot de textura
  pronto.
- O decal some após alguns segundos (DecalFadeTime, ex: 4-6s,
  ajustável) — fade out, não corte seco — para NÃO acumular memória com
  muitos NPCs mortos.
- Limite de decais simultâneos (pool, como os corpos da morte): acima
  do teto, o mais antigo some. Numa onda densa, não encher o chão de
  decais nem pesar.

## 6. Som de disparo (4 variações — slot pronto)

- Cue de disparo single shot com 4 variações (Random), disparado a cada
  tiro, sincronizado com a animação/cadência.
- 3D na posição da arma, atenuação por câmera, SC_Effects, concurrency
  (como os outros sons de tiro). Slot vazio até o usuário importar os 4.
- Reusar/estender o sistema de som de tiro que já existe (o FireSound
  por arma do Agente).

## 7. Menu DROP-UP de seleção de atiradores

- Na barra de peças, ao selecionar "atiradores/personagens", sobe um
  SUBMENU (drop-up — expande PARA CIMA a partir do botão) listando os
  tipos de atirador disponíveis.
- Hoje só este primeiro atirador, mas o menu já é PREPARADO para vários
  (lista que cresce). Cada entrada: ícone, nome, custo.
- Selecionar um no submenu põe ele "na mão" para posicionar no slot.
- Estruturar para novos atiradores entrarem na lista sem refazer o
  menu.

---

## Notas
- Tudo com SLOT VAZIO / placeholder, plugável quando o usuário importar
  (modelo, arma, animações, Niagara, sons, textura de sangue).
- O que o terminal faz AGORA: a lógica, os sockets, os pontos de
  disparo, o array de armas, o sistema de decal com pool, o menu
  drop-up, o roteamento de som. Os ASSETS o usuário pluga depois.
- Confirmar nomes de socket (mão e cano) com o usuário quando ele criar
  no skeletal.

## Entregável
- Atirador de plataforma funcional (placeholder de mesh/anim), mira
  correta, single shot.
- Arma separada no socket da mão; array de armas por nível (pistola
  preenchida), troca ao evoluir.
- Slots prontos: muzzle (cano), impacto (NPC), som (4 tiros), textura
  de sangue (decal com pool e fade).
- Menu drop-up de atiradores, preparado para vários.
- BD.Test.Regression passa; compilar os dois alvos; commit.
