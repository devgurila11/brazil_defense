# Briefing atual — Brazil Defense

**Versão: 2026-10-03 10:30**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Bugs + sistema de áudio/ambiência + estrelas + HUD limpo + coice

O usuário vai gerar e importar os áudios (música, ambiência, motor,
buzina) numa pasta; o terminal monta os sistemas e liga quando os
arquivos chegarem. Há 3 BUGS para corrigir também.

---

## 0. Renomear os áudios importados (ler as pastas, script em lote)

O usuário importou os áudios em duas pastas. Ler, identificar o que é
cada um pelo conteúdo/tipo e renomear num padrão limpo, via script em
lote (como foi feito com as falas do jumento):

- C:\Users\rafag\Documents\Unreal\Brazil_Defense\Content\audio\Musics
  → as ~10 músicas de batalha. Renomear MUS_Battle_01 ... _NN.
- C:\Users\rafag\Documents\Unreal\Brazil_Defense\Content\audio\Effects
  → ambiência, motor e buzina misturados. Separar por tipo e renomear:
    - Ambiência de cidade/dia  → AMB_City_Day
    - Ambiência de pássaros/dia → AMB_Birds_Day (se vier separado)
    - Ambiência de noite/coruja → AMB_Night
    - Partidas de motor (4)     → S_Bus_Engine_01 ... _04
    - Buzinas (4)               → S_Bus_Horn_01 ... _04
- Identificar pelo conteúdo (duração, nome atual, tipo de som). Se
  algum ficar ambíguo, listar para o usuário confirmar em vez de
  adivinhar.
- Depois de renomear, apontar cada um nos sistemas (música, ambiência,
  motor, buzina) dos itens 7, 8 e 9.

## BUGS (corrigir primeiro)

### 1. Mais de um palácio pôde ser construído (deveria ser 1)
- A regra é: MÁXIMO 1 palácio por partida (1 palácio = 1 agente).
- O usuário conseguiu pôr mais de um. A trava não está funcionando.
- Corrigir: o segundo palácio é RECUSADO com motivo claro ("já existe
  um palácio"). Se o palácio for vendido, pode construir outro.
- Regression: adicionar/confirmar invariante "só 1 palácio aceito".

### 2. Agente afunda no chão ao dormir
- Na animação Sleeping_Idle, metade do corpo do Agente fica ENTERRADA
  no chão. Ajustar o offset/altura (Z) do Agente deitado para ele
  ficar SOBRE o chão, não dentro.

### 3. Pés do Agente abaixo do chão ao caminhar
- Andando (Pistol_Walk), os pés ficam abaixo da superfície. Ajustar a
  altura base do Agente no grid para os pés tocarem o chão, não
  afundarem. (Pode ser a mesma origem Z do bug 2.)

---

## 4. Personagens de plataforma — DIAGNÓSTICO + coice

Dois problemas:

a) DIAGNÓSTICO: os personagens de plataforma parecem não girar
   dinamicamente atrás dos militantes e às vezes "observam passar sem
   atacar". Pode ser o mesmo bug que as torres tiveram (giro/mira
   desconectado). Investigar: eles giram a mira para o alvo? Estão
   deixando militantes no alcance passarem sem atirar? Logar e corrigir
   se for bug.

b) COICE (feedback de disparo): após cada disparo, o personagem faz um
   leve RECUO para trás (simulando o coice da arma), e volta. Assim dá
   para ver que ele ESTÁ atacando, não parado. Movimento pequeno e
   rápido, sincronizado com o tiro. Vale para os personagens de
   plataforma e, se fizer sentido, para o Agente também.

---

## 5. Estrelas de evolução em TODAS as construções

- Hoje só o palácio tem o indicador de 5 estrelas. Todas as peças que
  evoluem (torres, plataformas, personagens) também vão até nível 5.
- Pôr o mesmo indicador de 5 estrelas (billboard para a câmera, fade
  por distância, vazias = não evoluído) em TODAS as construções que
  evoluem, igual ao palácio.
- Reusar o sistema de estrelas que já existe no palácio.

---

## 6. Tecla para esconder TODO o HUD (modo limpo)

- Uma tecla (ex: H) alterna esconder/mostrar TODO o HUD — placar,
  barras, estrelas, medidores, debug, tudo. Para o jogador ver o jogo
  LIMPO, só a ação acontecendo.
- Apertar de novo traz tudo de volta.
- Esconde inclusive os indicadores de alcance e as estrelas.

---

## 7. Sistema de MÚSICA (ligar quando os áudios chegarem)

- ~10 músicas instrumentais brasileiras, em LOOP. O usuário importa.
- Toca UMA aleatória DURANTE a onda. Em loop sem corte (ponto de loop
  / crossfade para não dar sensação de corte).
- Na PAUSA / fase de montagem (entre ondas): a música faz FADE OUT,
  deixando só a AMBIÊNCIA de fundo tocando.
- Ao começar a próxima onda: entra outra música aleatória (fade in).
- Roteada para uma Sound Class de MÚSICA (separada de efeitos), para o
  slider de música do menu controlar.
- Slot/pasta pronta para o usuário apontar as músicas.

---

## 8. Sistema de AMBIÊNCIA de fundo (ligar quando chegar)

- Ambiência tocando SEMPRE (mais presente na pausa, por baixo da música
  na onda):
  - Dia: cidade em movimento + pássaros.
  - Noite: menos cidade, coruja/grilos.
- Troca conforme o ciclo dia/noite (usar o CycleAlpha que já existe).
- Em loop sem corte. Roteada para efeitos (ou uma class de ambiente).
- Slots prontos: AMB_City_Day, AMB_Birds_Day, AMB_Night, etc.

---

## 9. MOTOR e BUZINA dos ônibus (ligar quando chegar)

- 4 sons de MOTOR (partida): tocam no ônibus pouco ANTES da onda sair
  daquela boca (o ônibus se prepara). 3D na posição do ônibus, reverb
  de exterior, atenuação por câmera.
- 4 sons de BUZINA: tocam no momento EXATO da saída dos militantes
  daquele ônibus. 3D, mesma posição.
- Sorteados (Random). Só nas bocas ATIVAS da onda (seguem o sorteio de
  bocas que já existe).
- Slots prontos: S_Bus_Engine_01-04, S_Bus_Horn_01-04.
- Isso liga nos ganchos que já foram deixados prontos no sistema do
  ônibus (quando o ônibus se move / quando a onda sai).

---

## Entregável
- 3 bugs corrigidos (1 palácio só; Agente não afunda dormindo nem
  andando).
- Personagens de plataforma: diagnóstico do giro/mira + coice no
  disparo.
- Estrelas de evolução em todas as construções.
- Tecla de HUD limpo (esconde tudo).
- Sistemas de música, ambiência, motor e buzina PRONTOS com slots
  vazios, para o usuário apontar os áudios quando importar.
- BD.Test.Regression passa; compilar os dois alvos; commit.
