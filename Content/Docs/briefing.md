# Briefing atual — Brazil Defense

**Versão: 2026-10-10 16:30**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# FASE B: Falas de comemoração por personagem (originalidade)

Cada personagem tem VOZ e FALAS PRÓPRIAS — a Nicole fala as dela com a
voz dela, o Mito as dele com a voz dele, e os próximos atiradores idem.
Nunca cruzam. A estrutura é POR PERSONAGEM, não um banco global.

Áudios da Nicole já importados em:
C:\Users\rafag\Documents\Unreal\Brazil_Defense\Content\audio\Nicole
(CONVERTER para WAV antes de renomear.)

## 1. Converter e renomear (lote, como as falas do jumento)
- Converter os áudios da Nicole para WAV, depois renomear:
  VO_Nicole_01 ... _NN. Originais para SourceAudio, anotar no rename
  map, como o padrão do projeto.

## 2. Banco de falas POR PERSONAGEM (estrutura)
- Cada personagem (atirador, Agente) tem seu PRÓPRIO conjunto de falas
  de comemoração — campo no DataAsset dele apontando o Cue/banco dele.
- NÃO um banco global compartilhado. Nicole usa só as da Nicole.
- Montar o Cue da Nicole (SCue_Nicole_Festejo) com as falas dela
  (Random sem repetir a anterior, leve pitch), roteado para SC_Effects
  (ou uma class de voz), 3D na posição do personagem, atenuação por
  câmera, concurrency.
- Apontar no DataAsset da Nicole (DA_Shooter_Pistol).
- Deixar o slot do MITO pronto (DA_PalaceData), VAZIO — o usuário gera
  as falas dele depois (voz própria). Quando apontar, funciona igual.
- Estruturar para novos atiradores: cada um aponta seu banco, sem
  refazer o sistema.

## 3. Gatilho: só ao ABATER, frequência curta
- A fala dispara SÓ quando o personagem ABATE um NPC (derruba), NÃO a
  cada tiro.
- Frequência CURTA / esporádica: NÃO a cada abate. Só de vez em quando
  (ex: chance ~15-20% por abate, com intervalo mínimo entre falas do
  mesmo personagem e concurrency global — como as falas dos militantes,
  para não virar tagarelice). Expor a chance e o intervalo em settings.

## 4. Liga na exclamação AZUL (já existe)
- Quando a Nicole/Mito solta a fala, disparar a marca de exclamação
  AZUL acima dele (o sistema de exclamação por lado já existe; o código
  da fala do personagem precisa avisar o sistema, como o terminal
  deixou indicado).

## Entregável
- Falas da Nicole convertidas/renomeadas; Cue montado e apontado só no
  DataAsset dela.
- Sistema de falas POR PERSONAGEM (slot do Mito pronto e vazio).
- Fala só ao abater, esporádica (chance + intervalo + concurrency).
- Exclamação azul disparada junto com a fala.
- BD.Test.Regression passa; compilar os dois alvos; commit.
