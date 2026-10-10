# Briefing atual — Brazil Defense

**Versão: 2026-10-10 14:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Montar o Cue de som de disparo da pistola

O usuário importou os sons de DISPARO da pistola em:
C:\Users\rafag\Documents\Unreal\Brazil_Defense\Content\audio\Effects

ATENÇÃO: essa pasta JÁ tem outros sons (ambiência AMB_*, motor
S_Bus_Engine_*, buzina S_Bus_Horn_*). Identificar os NOVOS (os
disparos de pistola) pelos arquivos recém-adicionados / nome / tipo —
não misturar com os que já existem.

## Fazer
1. Identificar os ~4 sons de disparo novos. Renomear num padrão limpo:
   S_Pistola_Tiro_01 ... _04 (ou o padrão do projeto). Se ficar
   ambíguo qual é disparo, listar para o usuário confirmar.
2. Montar o Cue SCue_Pistola_Tiro com as 4 variações (Random, leve
   variação de pitch), como os outros Cues de tiro.
   - Reusar o BDBuildSoundCue: SCue em /Game/BD/Audio/SCue_Pistola_Tiro.
3. Apontar o SCue_Pistola_Tiro no FireSound da arma NÍVEL 0 (pistola)
   nos DOIS DataAssets:
   - DA_Shooter_Pistol (Nicole)
   - DA_PalaceData (Mito)
4. O som do tiro deve sair no frame do BD Shot (já sincronizado),
   3D na posição da arma, atenuação por câmera, SC_Effects, com o
   concurrency dos sons de tiro que já existe.

## Entregável
- Sons de disparo identificados e renomeados, sem misturar com
  ambiência/motor/buzina.
- SCue_Pistola_Tiro montado e apontado no FireSound nível 0 da Nicole
  e do Mito.
- Tiro com som (sincronizado com a animação), 3D, atenuado, concurrency.
- BD.Test.Regression passa; compilar os dois alvos; commit.
