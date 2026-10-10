# Briefing atual — Brazil Defense

**Versão: 2026-10-10 18:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Falas do Mito: comemoração (abate) + chute (ministro)

O Mito tem a MESMA VOZ em dois EVENTOS diferentes:
- COMEMORAÇÃO ao abater militante (como a Nicole).
- CHUTE ao chutar ministro (evento especial).

Áudios importados:
- Comemoração: C:\...\Content\audio\Mito (raiz da pasta Mito)
- Chute:       C:\...\Content\audio\Mito\Kick
(CONVERTER para WAV antes de renomear, como sempre.)

## 1. Converter e renomear
- Comemoração (pasta Mito, raiz): WAV, renomear VO_Mito_Festejo_01...
- Chute (pasta Mito/Kick): WAV, renomear VO_Mito_Chute_01...
- Originais para SourceAudio, anotar no rename map. Mesmo padrão das
  falas da Nicole/jumento.

## 2. Dois Cues, mesma voz, eventos diferentes
- SCue_Mito_Festejo: as falas de comemoração (Random sem repetir, leve
  pitch).
- SCue_Mito_Chute: as falas de chute.
- Ambos 3D na posição do Mito, atenuação por câmera, class de voz/
  efeitos, concurrency (as mesmas regras de fala).
- Apontar os dois nos campos do DataAsset do Mito (DA_PalaceData): um
  campo de fala de comemoração e um campo NOVO de fala de chute.

## 3. Gatilhos e frequência (DIFERENTES)
- COMEMORAÇÃO (ao abater militante): MESMAS regras da Nicole —
  esporádica (chance ~18%, intervalos, concurrency). Nada muda aqui.
- CHUTE (ao chutar ministro): toca SEMPRE que o Mito chuta. É evento
  raro e especial (o clímax cômico), então não precisa de chance nem
  intervalo — chutou, falou. Não entope porque chutar é raro.
  - Os MINISTROS ainda não existem (Fatia 3). Então: ligar o gancho
    "ao chutar → SCue_Mito_Chute + exclamação azul" JÁ, testável com o
    comando BD.Agent.Kick que já existe. Quando os ministros entrarem
    (Fatia 3), o chute real já virá com voz, sem mexer mais.

## 4. Exclamação azul
- As duas falas (comemoração e chute) disparam a exclamação AZUL do
  Mito, como a da Nicole.

## Entregável
- Falas do Mito convertidas/renomeadas (festejo e chute, separados).
- Dois Cues (festejo e chute) apontados no DataAsset do Mito.
- Comemoração esporádica (regras da Nicole); chute SEMPRE que chuta.
- Chute testável agora com BD.Agent.Kick; pronto para os ministros.
- Exclamação azul nos dois eventos.
- BD.Test.Regression passa; compilar os dois alvos; commit.
