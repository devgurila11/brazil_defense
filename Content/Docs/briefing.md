# Briefing atual — Brazil Defense

**Versão: 2026-09-26 10:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Som dos Militantes (jumentos): dois Sound Cues + casco sincronizado

Dar voz e passos à horda de jumentos. O usuário importa os áudios na
engine; o terminal monta os Cues e os disparos. Estrutura pensada para
REUSO: falas e efeitos em Cues SEPARADOS, para reaproveitar em outros
NPCs/personagens no futuro só ajustando parâmetros (ex: pitch da voz).

Assets que o usuário vai importar (confirmar os nomes/caminhos reais
quando ele avisar que importou):
- Falas neutras (várias, vozes variadas) — humor do "militante
  fanático genérico", sem mirar lado político.
- Relinchos / rosnados — efeitos prontos.
- Casco batendo no chão — efeito pronto (1-2 variações).

---

## 1. Cue de FALAS (separado, reutilizável)

- Sound Cue próprio só com as falas: nó Random sorteando entre os
  clipes de voz.
- Parâmetro de PITCH exposto/fácil de ajustar no Cue — para reusar em
  outro personagem engrossando/afinando a voz sem refazer o Cue.
- Modulação leve de pitch aleatória (ex: ±5%) para as repetições não
  soarem idênticas.
- Este Cue é o que será reaproveitado; manter genérico e limpo.

## 2. Cue de EFEITOS (separado)

- Sound Cue próprio com relinchos e rosnados: nó Random.
- Separado das falas de propósito — efeitos animais ficam só para os
  bichos; as falas viajam para outros personagens.
- Modulação leve de pitch aleatória também.

## 3. Disparo das vocalizações (falas + efeitos) — evitar o enxame

Cada militante solta uma vocalização de tempos em tempos, NÃO
constante, e o jogo limita quantas tocam juntas.

- Por NPC: timer com intervalo ALEATÓRIO entre disparos (ex: 8 a 20s,
  expor em settings). Ao disparar, sorteia entre falar (Cue de Falas)
  ou fazer efeito (Cue de Efeitos) — proporção ajustável (ex: 30%
  fala, 70% relincho/rosnado, para não virar tagarelice).
- GLOBAL (concurrency): Sound Concurrency limitando o total de
  vocalizações simultâneas no jogo todo (ex: máx 3-4). Mesmo com 50
  militantes, só 3-4 vocalizam ao mesmo tempo — vira "murmuração de
  multidão", não enxame. Cortar a mais antiga quando estourar.
- Este é o ponto que faz ou quebra o resultado: sem o concurrency,
  uma horda densa vira parede de falas.

## 4. Casco sincronizado com a passada (via animação, não aleatório)

- O som de casco toca no MOMENTO em que o pé toca o chão, sincronizado
  com a animação de corrida — NÃO em timer aleatório.
- Mecanismo: Anim Notify na animação Run Forward, nos frames de
  contato de cada pé (2 ou 4 por ciclo). O Notify dispara o som do
  casco.
- O terminal tenta identificar os frames de contato. Se não conseguir
  (headless não visualiza bem a animação), o usuário marca os Anim
  Notifies no editor de animação — deixar essa parte pronta para ele
  plugar o som no Notify.
- Casco também com leve modulação de pitch para os passos não soarem
  idênticos.
- O casco NÃO passa pelo concurrency das vocalizações (é passo, não
  voz) — mas pode ter um concurrency próprio mais alto se a horda
  densa fizer muito casco junto (avaliar).

## 5. Áudio espacial (como os outros sons do jogo)

- Todos os Cues 3D, tocados na posição do NPC.
- Atenuação por CÂMERA (listener na câmera, como já configurado):
  militante longe/zoom afastado = quase inaudível; perto = claro.
- Roteados para SC_Effects (o slider de efeitos e o mudo geral
  controlam eles).

---

## Reuso futuro (deixar preparado)

- O Cue de Falas deve ser fácil de duplicar/reaproveitar para outro
  personagem só trocando o pitch e, se quiser, o conjunto de clipes.
- Estruturar para que um novo NPC vocal precise de pouco: apontar o
  Cue, ajustar pitch, definir intervalo.

## Entregável

- Cue de Falas e Cue de Efeitos, separados, com Random e pitch
  ajustável.
- Vocalização por NPC com timer aleatório + concurrency global (sem
  enxame).
- Casco sincronizado por Anim Notify (terminal tenta; usuário marca se
  preciso).
- Tudo 3D, atenuação por câmera, SC_Effects.
- BD.Test.Regression passa; compilar os dois alvos.
- Testar numa onda densa e confirmar que soa como multidão, não
  enxame — reportar quantas vocalizações simultâneas em pico.
