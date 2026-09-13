# Briefing atual — Brazil Defense

**Versão: 2026-09-13 10:15**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.
> Compare a versão acima com a última que você leu.

# Camada de interface — esqueleto funcional

Tudo em UMG. Tudo funcional e SEM ESTILO — visual cru, caixa e texto
puro. O estilo (assets do Nano, cores, animação) vem depois, por
cima. Não gastar tempo em aparência agora.

Todo texto visível é FText localizável. Dois idiomas: inglês (padrão)
e português. String table por idioma, nada de texto hardcoded.

---

## 1. Fluxo de telas

Sequência de abertura:

```
Splash (vídeo da marca) → Loading → Menu principal → [Play] → fade → Jogo
```

- **Splash**: placeholder por enquanto — uma tela preta com o texto
  "Gurila Games" por 2s, pulável com qualquer tecla. Deixar o ponto
  de encaixe pronto para trocar por MediaPlayer/vídeo depois.
- **Loading**: tela simples com indicador de carregamento. Placeholder.
- **Menu principal**: fundo placeholder (cor sólida ou imagem estática
  qualquer) com o ponto de encaixe para o cinecut em looping futuro.
- **Transições**: fade in/out entre todas as telas, incluindo menu →
  jogo. Duração configurável.

Gerenciar por um GameInstance ou um subsystem de UI que controla qual
tela está ativa.

---

## 2. Menu principal

Botões:

- **Play** — vai para seleção de dificuldade (ou direto ao jogo, se a
  seleção ainda não existir; deixar o gancho)
- **Options** — abre o painel de opções
- **Quit** — sai do jogo

Layout cru, empilhado. O visual entra depois.

---

## 3. Painel de opções

Três seções, todas persistidas em SaveGame:

**Gráficos**

- Preset de qualidade via Scalability da engine (Low/Medium/High/Epic)
- Resolução
- Tela cheia / janela / borderless
- V-Sync on/off

**Áudio**

- Volume de música (slider 0–100)
- Volume de efeitos (slider 0–100)
- Mudo geral (toggle)
- Ligar os sliders a Sound Classes / Sound Mix reais, para já
  afetarem o som que existe

**Idioma**

- Inglês (padrão) e Português
- Troca em runtime via FInternationalization, sem reiniciar — todo
  FText atualiza na hora

---

## 4. Persistência

UBDSettingsSave (USaveGame) guardando gráficos, áudio e idioma.

- Carregado no início do jogo, antes do menu
- Aplicado automaticamente
- Salvo a cada mudança no painel de opções
- Se não existir save, cria com os defaults (idioma inglês, qualidade
  auto, volumes em 80)

---

## 5. HUD de jogo

UMG, sobre o jogo. Substitui o overlay de debug atual.

### Sempre visível

- **Placar**: votos azul e vermelho, lado a lado, cada um na sua cor
- **Onda atual** e **cronômetro** até a próxima (ou "montagem" na
  fase Building)
- **Velocidade de jogo**: botões 1x / 2x / 4x, com o atual destacado
- **Bocas ativas da onda** (discreto)

### Sob demanda — ao selecionar um defensor

- Nome, nível atual, dano, alcance, cadência
- Botão de **upgrade** com o custo
- Botão de **vender** com o valor de reembolso
- **CRÍTICO**: antes de confirmar upgrade, venda ou movimentação,
  mostrar o placar RESULTANTE. Se o gasto for inverter a liderança
  (WouldInvertScoreboard já existe), destacar em vermelho. É o que
  impede o jogador de perder sem entender.

### Sob demanda — ao selecionar uma peça para colocar

- Nome, custo, footprint
- Motivo da recusa quando o preview está inválido (o
  EBDPlacementRefusal já existe — traduzir os motivos para FText
  legível)

### Candidato vivo

- Barra de HP do candidato no topo da tela
- Aviso claro quando ele surge
- "APURAÇÃO CONGELADA" com contagem durante a pausa pós-morte

---

## 6. Ligar aos sistemas existentes

Nada de lógica nova de gameplay. O HUD só lê e exibe o que já existe:

- Votos: ABDMatchManager (VotesBlue, VotesRed, OnVotesChanged)
- Onda e fase: ABDMatchManager
- Seleção e custos: UBDPlacementComponent, ABDTowerBase
- Candidato: UBDCandidateSubsystem

Onde faltar um delegate para o HUD escutar, criar — mas sem mudar a
lógica, só expor.

Desligar o overlay de debug (BD.HUD.Debug) quando o HUD real estiver
ativo, ou deixar os dois coexistindo atrás de cvar.

---

## Entregável

Compilar limpo nos dois targets.

- Abrir o jogo cai no splash placeholder → loading → menu.
- Menu: Play entra no jogo com fade; Options abre o painel; Quit sai.
- Opções: mudar idioma troca os textos na hora; mudar volume afeta o
  som; mudar qualidade muda o gráfico; tudo persiste ao fechar e
  reabrir.
- No jogo: HUD mostra placar, onda, cronômetro, velocidade.
  Selecionar defensor mostra stats, upgrade com custo e placar
  resultante, venda com reembolso. Candidato mostra barra de HP.
- Tudo funcional, visual cru. Estilo é a próxima fase.
